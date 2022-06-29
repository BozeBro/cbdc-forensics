#include "context.hxx"
#include "tracer.hxx"
#include "byzantine_server.hxx"

#include <iostream>
#include <algorithm>
namespace nuraft {
/*ptr<resp_msg> byz_server::process_req(req_msg& req)  {
    std::cout << "The leader is node " << leader_ << "\n";
    std::cout << state_->get_term() << "\n"; 
    if (is_leader()) {
        return verbose_server::process_req(req); 
    }
    ulong term = req.get_term();
    cb_func::Param param(id_, leader_);
    param.ctx = &req;
    CbReturnCode rc = ctx_->cb_func_.call(cb_func::ProcessReq, &param);
    // Assume normal behavior where it is a leader sending info to node.
    // A new leader implies higher term
    /* 
        rc != CbReturnCode::ReturnNull &&
        !stopping_ && 
        req.get_type() != msg_type::client_request
    
    if ( term > state_->get_term() &&
        rc != CbReturnCode::ReturnNull &&
        !stopping_ && 
        req.get_type() != msg_type::client_request ) {
            ptr<resp_msg> res = verbose_server::process_req(req);
            // my_priority is protected by a lock. 
            recur_lock(lock_); 
            // Now the Byzantine server can convince any node to vote for it.
            my_priority_ = target_priority_; 
            initiate_vote();
            return res; 
    } else {
        return verbose_server::process_req(req);
    }
}
*/
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
            decay_target_priority();
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
    verbose_server::request_prevote();
    return resp;
}

}