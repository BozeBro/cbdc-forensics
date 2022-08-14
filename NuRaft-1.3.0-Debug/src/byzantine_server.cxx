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
ptr<resp_msg> byz_server::process_req(req_msg& req) {
    ptr<resp_msg> resp{log_server::process_req(req)};
    // start election when leader is alive, and all initial servers are in the server
    if (commence && is_leader_alive())
        initiate_vote();
    return resp;
}
}