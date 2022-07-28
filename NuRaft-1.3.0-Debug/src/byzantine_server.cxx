#include "context.hxx"
#include "tracer.hxx"
#include "byzantine_server.hxx"
#include "raft_server.hxx"
#include "cluster_config.hxx"
#include "context.hxx"
#include "error_code.hxx"
#include "global_mgr.hxx"
#include "handle_client_request.hxx"
#include "handle_custom_notification.hxx"
#include "peer.hxx"
#include "snapshot.hxx"
#include "stat_mgr.hxx"
#include "state_machine.hxx"
#include "state_mgr.hxx"
#include <iostream>
#include <algorithm>
namespace nuraft {
byz_server::byz_server(context* ctx, const init_options& opt) 
            : log_server::log_server(ctx, opt)
            {p_in("I AM BYZANTINE");}
ptr<resp_msg> byz_server::process_req(req_msg& req) {
    cb_func::Param param(id_, leader_);
    param.ctx = &req;
    CbReturnCode rc = ctx_->cb_func_.call(cb_func::ProcessReq, &param);
    if (rc == CbReturnCode::ReturnNull) {
        p_wn("by callback, return null");
        return nullptr;
    }

    p_db( "Receive a %s message from %d with LastLogIndex=%llu, "
          "LastLogTerm=%llu, EntriesLength=%d, CommitIndex=%llu and Term=%llu",
          msg_type_to_string(req.get_type()).c_str(),
          req.get_src(),
          req.get_last_log_idx(),
          req.get_last_log_term(),
          req.log_entries().size(),
          req.get_commit_idx(),
          req.get_term() );

    if (stopping_) {
        // Shutting down, ignore all incoming messages.
        p_wn("stopping, return null");
        return nullptr;
    }

    if ( req.get_type() == msg_type::client_request ) {
        // Client request doesn't need to go through below process.
        return handle_cli_req_prelock(req);
    }

    recur_lock(lock_);
    if ( req.get_type() == msg_type::append_entries_request ||
         req.get_type() == msg_type::request_vote_request ||
         req.get_type() == msg_type::install_snapshot_request ) {
        
        // we allow the server to be continue after term updated to save a round message
        bool term_updated = update_term(req.get_term());

        if ( !im_learner_ &&
             !hb_alive_ &&
             term_updated &&
             req.get_type() == msg_type::request_vote_request ) {
            // If someone has newer term, that means leader has not been
            // elected in the current term, and this node's election timer
            // has been reset by this request.
            // We should decay the target priority here.
            // decay_target_priority();
        }

        // Reset stepping down value to prevent this server goes down when leader
        // crashes after sending a LeaveClusterRequest
        if (steps_to_down_ > 0) {
            steps_to_down_ = 2;
        }
    }

    ptr<resp_msg> resp;
    if (req.get_type() == msg_type::append_entries_request) {
        resp = handle_append_entries(req);

    } else if (req.get_type() == msg_type::request_vote_request) {
        resp = handle_vote_req(req);

    } else if (req.get_type() == msg_type::pre_vote_request) {
        resp = handle_prevote_req(req);

    } else if (req.get_type() == msg_type::ping_request) {
        p_in("got ping from %d", req.get_src());
        resp = cs_new<resp_msg>( state_->get_term(),
                                 msg_type::ping_response,
                                 id_,
                                 req.get_src() );

    } else if (req.get_type() == msg_type::priority_change_request) {
        resp = handle_priority_change_req(req);
    } else {
        // extended requests
        resp = handle_ext_msg(req);
    }

    if (resp) {
        p_db( "Response back a %s message to %d with Accepted=%d, "
              "Term=%llu, NextIndex=%llu",
              msg_type_to_string(resp->get_type()).c_str(),
              resp->get_dst(),
              resp->get_accepted() ? 1 : 0,
              resp->get_term(),
              resp->get_next_idx() );
    }
    if (commence && is_leader_alive())
        initiate_vote();
    return resp;
}

void byz_server::handle_peer_resp(ptr<resp_msg>& resp, ptr<rpc_exception>& err) {
    recur_lock(lock_);
    if (err) {
        int32 peer_id = err->req()->get_dst();
        ptr<peer> pp = nullptr;
        auto entry = peers_.find(peer_id);
        if (entry != peers_.end()) pp = entry->second;

        int rpc_errs = 0;
        if (pp) {
            pp->inc_rpc_errs();
            rpc_errs = pp->get_rpc_errs();

            check_snapshot_timeout(pp);
        }

        if (rpc_errs < raft_server::raft_limits_.warning_limit_) {
            p_wn("peer (%d) response error: %s", peer_id, err->what());
        } else if (rpc_errs == raft_server::raft_limits_.warning_limit_) {
            p_wn("too verbose RPC error on peer (%d), "
                 "will suppress it from now", peer_id);
        }

        if (pp && pp->is_leave_flag_set()) {
            // If this is to-be-removed server, proceed it without
            // waiting for the response.
            handle_join_leave_rpc_err(msg_type::leave_cluster_request, pp);
        }
        return;
    }

    if (!resp.get()) {
        p_wn("empty peer response");
        return;
    }

    p_db( "Receive a %s message from peer %d with "
          "Result=%d, Term=%llu, NextIndex=%llu",
          msg_type_to_string(resp->get_type()).c_str(),
          resp->get_src(),
          resp->get_accepted() ? 1 : 0,
          resp->get_term(),
          resp->get_next_idx() );

    p_tr("src: %d, dst: %d, resp->get_term(): %d\n",
         (int)resp->get_src(), (int)resp->get_dst(), (int)resp->get_term());

    if (resp->get_accepted()) {
        // On accepted response, reset response timer.
        auto entry = peers_.find(resp->get_src());
        if (entry != peers_.end()) {
            peer* pp = entry->second.get();
            int rpc_errs = pp->get_rpc_errs();
            if (rpc_errs >= raft_server::raft_limits_.warning_limit_) {
                p_wn("recovered from RPC failure from peer %d, %d errors",
                     resp->get_src(), rpc_errs);
            }
            pp->reset_rpc_errs();
            pp->reset_resp_timer();
        }
    }

    if ( is_valid_msg(resp->get_type()) ) {
        bool update_term_succ = update_term(resp->get_term());

        // if term is updated, no more action is required
        if (update_term_succ) return;
    }

    // ignore the response that with lower term for safety
    switch (resp->get_type())
    {
    case msg_type::pre_vote_response:
        handle_prevote_resp(*resp);
        break;

    case msg_type::request_vote_response:
        handle_vote_resp(*resp);
        break;

    case msg_type::append_entries_response:
        handle_append_entries_resp(*resp);
        break;

    case msg_type::install_snapshot_response:
        handle_install_snapshot_resp(*resp);
        break;

    case msg_type::priority_change_response:
        handle_priority_change_resp(*resp);
        break;

    case msg_type::ping_response:
        p_in("got ping response from %d", resp->get_src());
        break;

    case msg_type::custom_notification_response:
        handle_custom_notification_resp(*resp);
        break;

    default:
        p_er( "received an unexpected response: %s, ignore it",
              msg_type_to_string(resp->get_type()).c_str() );
        break;
    }
}

}