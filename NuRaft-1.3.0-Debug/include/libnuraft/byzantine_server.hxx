#ifndef _BYZANTINE_SERVER_HXX_
#define _BYZANTINE_SERVER_HXX_

#include "raft_server.hxx"
#include "logs.hxx"

namespace nuraft {
    class byz_server : public log_server {
        public:
            // Benedict
            // [C2]
            using log_server::log_server;
            // This is where we start an election illegally
            ptr<resp_msg> process_req(req_msg& req);
            // flag to notify when to start attack
            // start attack after all servers have been added to the cluster.
            bool commence{false};
        protected:
            // Ignore the results of the votes
            void handle_vote_resp(resp_msg& resp);
            // Sending too many prevotes means program termination for byzantine node
            // Ignore statement to terminate and continue
            void handle_prevote_resp(resp_msg& resp);
            // If all servers have been added, commence is true here.
            void reconfigure(const ptr<cluster_config>& new_config);
            // [C2] end
    };
}
#endif // _BYZANTINE_SERVER_HXX_