#ifndef _BYZANTINE_SERVER_HXX_
#define _BYZANTINE_SERVER_HXX_

#include "verbose_server.hxx"
#include <iostream>
namespace nuraft {
    class byz_server : public verbose_server {
        public:
            byz_server(context* ctx, const init_options& opt, bool verbose) 
            : verbose_server::verbose_server(ctx, opt, verbose)
            {}
            virtual ptr<resp_msg> process_req(req_msg& req) {
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
                */
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
    };
}
#endif // _BYZANTINE_SERVER_HXX_