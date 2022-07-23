#ifndef _BYZANTINE_SERVER_HXX_
#define _BYZANTINE_SERVER_HXX_

#include "verbose_server.hxx"
#include <iostream>
namespace nuraft {
    class byz_server : public verbose_server {
        public:
            byz_server(context* ctx, const init_options& opt = init_options(), bool verbose = false);
            ptr<resp_msg> process_req(req_msg& req) override;
        protected:
            void handle_vote_resp(resp_msg& resp) override;
            void handle_peer_resp(ptr<resp_msg>& resp, ptr<rpc_exception>& err) override;
            void handle_prevote_resp(resp_msg& resp) override;
    };
}
#endif // _BYZANTINE_SERVER_HXX_