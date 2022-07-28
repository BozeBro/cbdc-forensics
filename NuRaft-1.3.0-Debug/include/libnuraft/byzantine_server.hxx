#ifndef _BYZANTINE_SERVER_HXX_
#define _BYZANTINE_SERVER_HXX_

#include "raft_server.hxx"
#include "logs.hxx"

namespace nuraft {
    class byz_server : public log_server {
        public:
            // byz_server(context* ctx, bool verbose, const init_options& opt = init_options());
            byz_server(context* ctx, const raft_server::init_options& opt = raft_server::init_options());
            ptr<resp_msg> process_req(req_msg& req);
            bool commence{false};
        protected:
            void handle_vote_resp(resp_msg& resp);
            void handle_peer_resp(ptr<resp_msg>& resp, ptr<rpc_exception>& err);
            void handle_prevote_resp(resp_msg& resp);
            void reconfigure(const ptr<cluster_config>& new_config);
    };
}
#endif // _BYZANTINE_SERVER_HXX_