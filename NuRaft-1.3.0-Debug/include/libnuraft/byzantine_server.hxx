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
    };
    class crash_server : public verbose_server {
        using verbose_server::verbose_server;
        public:
            virtual ptr<resp_msg> process_req(req_msg& req);
        protected:
            void handle_election_timeout();
    };
}
#endif // _BYZANTINE_SERVER_HXX_