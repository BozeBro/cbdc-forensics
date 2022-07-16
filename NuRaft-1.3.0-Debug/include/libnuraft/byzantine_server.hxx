#ifndef _BYZANTINE_SERVER_HXX_
#define _BYZANTINE_SERVER_HXX_

#include "verbose_server.hxx"
#include <iostream>
namespace nuraft {
    class byz_server : public verbose_server {
        public:
            byz_server(context* ctx, const init_options& opt, bool verbose);
            virtual ptr<resp_msg> process_req(req_msg& req);
        protected:
            void handle_prevote_resp(resp_msg& resp);
            void initiate_vote(bool force_vote = false);
            void request_vote(bool force_vote);
            void handle_vote_resp(resp_msg& resp);
            void handle_peer_resp(ptr<resp_msg>& resp, ptr<rpc_exception>& err);
    };
}
#endif // _BYZANTINE_SERVER_HXX_