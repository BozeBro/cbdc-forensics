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
            virtual ptr<resp_msg> process_req(req_msg& req);
    };
}
#endif // _BYZANTINE_SERVER_HXX_