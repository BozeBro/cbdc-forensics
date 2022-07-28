#ifndef _LOG_BASE_HXX_
#define _LOG_BASE_HXX_

#include "raft_server.hxx"
#include <vector>

namespace nuraft {
    enum activity {
        null_state     = -1,
        req_prevote    = 0,
        reject_prevote = 1,
        accept_prevote = 2,
        req_vote       = 3,
        accept_vote    = 4,
        reject_vote    = 5,
        pre_commit_log = 6,
        commit_log     = 7,
        be_leader      = 8,
        be_follower    = 9,
        ignore_votes   = 10
    };

    class log_server : public raft_server{
        using raft_server::raft_server;
        public:
            void set_state(activity act);
            void set_state(activity act, req_msg& req);
            void set_state(activity act, ulong idx);
        protected:
            void request_prevote() {
                set_state(req_vote);
                return raft_server::request_prevote();
            } 
            ptr<resp_msg> handle_prevote_req(req_msg& req) {
                ptr<resp_msg> resp = raft_server::handle_prevote_req(req);
                if (resp->get_accepted()) {
                    set_state(accept_prevote, req);
                } else {
                    set_state(reject_prevote, req);
                }
                return resp; 
            }
            void request_vote(bool force_vote) {
                set_state(req_vote);
                return raft_server::request_vote(force_vote);
            }
            ptr<resp_msg> handle_vote_req(req_msg& req) {
                ptr<resp_msg> resp = raft_server::handle_vote_req(req);
                if (resp->get_accepted()) {
                    set_state(accept_vote, req);
                } else {
                    set_state(reject_vote, req);
                }
                return resp; 
            }
            bool try_update_precommit_index(ulong desired, const size_t MAX_ATTEMPTS = 10) {
                set_state(pre_commit_log);
                return raft_server::try_update_precommit_index(desired, MAX_ATTEMPTS);
            }
            void commit(ulong target_idx) {
                set_state(commit_log, target_idx);
                return raft_server::commit(target_idx);
            }
            void become_leader() {
                set_state(be_leader);
                return raft_server::become_leader();
            }
            void become_follower() {
                set_state(be_follower);
                return raft_server::become_follower();
            }
    };
}
#endif