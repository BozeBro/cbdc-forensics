#ifndef _BYZANTINE_SERVER_HXX_
#define _BYZANTINE_SERVER_HXX_

#include "verbose_server.hxx"
#include <iostream>

namespace nuraft {
    class byz_server : public raft_server {
        public:
            // byz_server(context* ctx, bool verbose, const init_options& opt = init_options());
            byz_server(context* ctx, const init_options& opt = init_options());
            ptr<resp_msg> process_req(req_msg& req) override;
            bool commence{false};
        protected:
            void handle_vote_resp(resp_msg& resp) override;
            void handle_peer_resp(ptr<resp_msg>& resp, ptr<rpc_exception>& err) override;
            void handle_prevote_resp(resp_msg& resp) override;
            void reconfigure(const ptr<cluster_config>& new_config) override;
    };
}
#endif // _BYZANTINE_SERVER_HXX_