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
byz_server::byz_server(context* ctx, const init_options& opt, bool verbose) 
            : verbose_server::verbose_server(ctx, opt, verbose)
            {p_in("I AM BYZANTINE!");}
ptr<resp_msg> byz_server::process_req(req_msg& req) {
    print_msg("Trying to manipulate votes");
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
    request_prevote();
    return resp;
}
void byz_server::handle_prevote_resp(resp_msg& resp) {
    if (resp.get_term() != pre_vote_.term_) {
        // Vote response for other term. Should ignore it.
        p_in("[PRE-VOTE RESP] from peer %d, my role %s, "
             "but different resp term %zu (pre-vote term %zu). "
             "ignore it.",
             resp.get_src(), srv_role_to_string(role_).c_str(),
             resp.get_term(), pre_vote_.term_);
        return;
    }

    if (resp.get_accepted()) {
        // Accept: means that this peer is not receiving HB.
        pre_vote_.dead_++;
    } else {
        if (resp.get_next_idx() != std::numeric_limits<ulong>::max()) {
            // Deny: means that this peer still sees leader.
            pre_vote_.live_++;
        } else {
            // `next_idx_for_resp == MAX`, it is a special signal
            // indicating that this node has been already removed.
            pre_vote_.abandoned_++;
        }
    }

    int32 election_quorum_size = get_quorum_for_election() + 1;

    p_in("[PRE-VOTE RESP] peer %d (%s), term %zu, resp term %zu, "
         "my role %s, dead %d, live %d, "
         "num voting members %d, quorum %d\n",
         resp.get_src(), (resp.get_accepted())?"O":"X",
         pre_vote_.term_, resp.get_term(),
         srv_role_to_string(role_).c_str(),
         pre_vote_.dead_.load(), pre_vote_.live_.load(),
         get_num_voting_members(), election_quorum_size);

    if (pre_vote_.dead_ >= election_quorum_size) {
        p_in("[PRE-VOTE DONE] SUCCESS, term %zu", pre_vote_.term_);

        bool exp = false;
        bool val = true;
        if (pre_vote_.done_.compare_exchange_strong(exp, val)) {
            p_in("[PRE-VOTE DONE] initiate actual vote");

            // Immediately initiate actual vote.
            initiate_vote();

            // restart the election timer if this is not yet a leader
            if (role_ != srv_role::leader) {
                restart_election_timer();
            }

        } else {
            p_in("[PRE-VOTE DONE] actual vote is already initiated, do nothing");
        }
    }

    if (pre_vote_.live_ >= election_quorum_size) {
        pre_vote_.quorum_reject_count_.fetch_add(1);
        p_wn("[PRE-VOTE] rejected by quorum, count %zu",
             pre_vote_.quorum_reject_count_.load());
        if ( pre_vote_.quorum_reject_count_ >=
                 raft_server::raft_limits_.pre_vote_rejection_limit_ ) {
            p_ft("too many pre-vote rejections, probably this node is not "
                 "receiving heartbeat from leader. "
                 "we should re-establish the network connection");
            raft_server::send_reconnect_request();
        }
    }

    if (pre_vote_.abandoned_ >= election_quorum_size) {
        p_er("[PRE-VOTE DONE] this node has been removed, stepping down");
        steps_to_down_ = 2;
    }
}
void byz_server::initiate_vote(bool force_vote) {
    int grace_period = ctx_->get_params()->grace_period_of_lagging_state_machine_;
    ulong cur_term = state_->get_term();
    if ( !force_vote &&
         grace_period &&
         sm_commit_index_ < lagging_sm_target_index_ ) {
        p_in("grace period option is enabled, and state machine needs catch-up: "
             "%lu vs. %lu",
             sm_commit_index_.load(),
             lagging_sm_target_index_.load());
        if (vote_init_timer_term_ != cur_term) {
            p_in("grace period: %d, term increment detected %lu vs. %lu, reset timer",
                grace_period, vote_init_timer_term_.load(), cur_term);
            vote_init_timer_.set_duration_ms(grace_period);
            vote_init_timer_.reset();
            vote_init_timer_term_ = cur_term;
        }

        if ( vote_init_timer_term_ == cur_term &&
             !vote_init_timer_.timeout() ) {
            // Grace period, do not initiate vote.
            p_in("grace period: %d, term %lu, waited %lu ms, skip initiating vote",
                 grace_period, cur_term, vote_init_timer_.get_ms());
            return;

        } else {
            p_in("grace period: %d, no new leader detected for term %lu for %lu ms",
                 grace_period, cur_term, vote_init_timer_.get_ms());
        }
    }

    if ( my_priority_ >= target_priority_ ||
         force_vote ||
         check_cond_for_zp_election() ||
         get_quorum_for_election() == 0 ) {
        // Request vote when
        //  1) my priority satisfies the target, OR
        //  2) I'm the only node in the group.
        state_->inc_term();
        state_->set_voted_for(-1);
        role_ = srv_role::candidate;
        votes_granted_ = 0;
        votes_responded_ = 0;
        election_completed_ = false;
        ctx_->state_mgr_->save_state(*state_);
        request_vote(force_vote);
    }

    if (role_ != srv_role::leader) {
        hb_alive_ = false;
        leader_ = -1;
    }
}

void byz_server::request_vote(bool force_vote) {
    state_->set_voted_for(id_);
    ctx_->state_mgr_->save_state(*state_);
    votes_granted_ += 1;
    votes_responded_ += 1;
    p_in("[VOTE INIT] my id %d, my role %s, term %ld, log idx %ld, "
         "log term %ld, priority (target %d / mine %d)\n",
         id_, srv_role_to_string(role_).c_str(),
         state_->get_term(), log_store_->next_slot() - 1,
         term_for_log(log_store_->next_slot() - 1),
         target_priority_, my_priority_);

    // is this the only server?
    if (votes_granted_ > get_quorum_for_election()) {
        std::cout << "REJECT LEADER\n";
        election_completed_ = true;
        // Ignore votes. We want to perpetually continue election
        // become_leader();
        return;
    }

    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        ptr<peer> pp = it->second;
        if (!is_regular_member(pp)) {
            // Do not send voting request to learner.
            continue;
        }
        ptr<req_msg> req = cs_new<req_msg>
                           ( state_->get_term(),
                             msg_type::request_vote_request,
                             id_,
                             pp->get_id(),
                             term_for_log(log_store_->next_slot() - 1),
                             log_store_->next_slot() - 1,
                             quick_commit_index_.load() );
        if (force_vote) {
            // Add a special log entry to let receivers ignore the priority.

            // Force vote message, and wrap it using log_entry.
            ptr<force_vote_msg> fv_msg = cs_new<force_vote_msg>();
            ptr<log_entry> fv_msg_le =
                cs_new<log_entry>(0, fv_msg->serialize(), log_val_type::custom);

            // Ship it.
            req->log_entries().push_back(fv_msg_le);
        }
        p_db( "send %s to server %d with term %llu",
              msg_type_to_string(req->get_type()).c_str(),
              it->second->get_id(),
              state_->get_term() );
        if (pp->make_busy()) {
            pp->send_req(pp, req, resp_handler_);
        } else {
            p_wn("failed to send vote request: peer %d (%s) is busy",
                 pp->get_id(), pp->get_endpoint().c_str());
        }
    }
}
void byz_server::handle_vote_resp(resp_msg& resp) {
    if (election_completed_) {
        p_in("Election completed, will ignore the voting result from this server");
        return;
    }

    if (resp.get_term() != state_->get_term()) {
        // Vote response for other term. Should ignore it.
        p_in("[VOTE RESP] from peer %d, my role %s, "
             "but different resp term %zu. ignore it.",
             resp.get_src(), srv_role_to_string(role_).c_str(), resp.get_term());
        return;
    }
    votes_responded_ += 1;

    if (resp.get_accepted()) {
        votes_granted_ += 1;
    }

    if (votes_responded_ >= get_num_voting_members()) {
        election_completed_ = true;
    }

    int32 election_quorum_size = get_quorum_for_election() + 1;

    p_in("[VOTE RESP] peer %d (%s), resp term %zu, my role %s, "
         "granted %d, responded %d, "
         "num voting members %d, quorum %d\n",
         resp.get_src(), (resp.get_accepted()) ? "O" : "X", resp.get_term(),
         srv_role_to_string(role_).c_str(),
         (int)votes_granted_, (int)votes_responded_,
         get_num_voting_members(), election_quorum_size);

    if (votes_granted_ >= election_quorum_size) {
        p_in("Server is elected as leader for term %zu", state_->get_term());
        election_completed_ = true;
        // become_leader();
        // p_in("  === LEADER (term %zu) ===\n", state_->get_term());
        p_in("FAKED BEING LEADER. TRY TO RESTART ELECTION");
    }
}

ptr<resp_msg> crash_server::process_req(req_msg& req) {
    for ( ; ;) {}
    // We will never arrive here
    return verbose_server::process_req(req);
}
}