#include "nuraft.hxx"
#include "raft_functional_common.hxx"
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <sstream>
#include <string>
using namespace nuraft;
namespace comm = raft_functional_common;
namespace server {
// using state machine from raft_bench.cxx
class dummy_sm : public state_machine {
public:
    dummy_sm()
        : last_commit_idx_(0) {}
    ~dummy_sm() {}

    ptr<buffer> commit(const ulong log_idx, buffer& data) {
        ptr<buffer> ret = buffer::alloc(sizeof(ulong));
        ret->put(log_idx);
        ret->pos(0);
        last_commit_idx_ = log_idx;
        return ret;
    }

    ptr<buffer> pre_commit(const ulong log_idx, buffer& data) { return nullptr; }

    void rollback(const ulong log_idx, buffer& data) {}
    void save_snapshot_data(snapshot& s, const ulong offset, buffer& data) {}
    void save_logical_snp_obj(
        snapshot& s, ulong& obj_id, buffer& data, bool is_first_obj, bool is_last_obj) {
        obj_id++;
    }
    bool apply_snapshot(snapshot& s) { return true; }
    int read_snapshot_data(snapshot& s, const ulong offset, buffer& data) { return 0; }

    int read_logical_snp_obj(snapshot& s,
                             void*& user_snp_ctx,
                             ulong obj_id,
                             ptr<buffer>& data_out,
                             bool& is_last_obj) {
        is_last_obj = true;
        data_out = buffer::alloc(sizeof(ulong));
        data_out->put(obj_id);
        data_out->pos(0);
        return 0;
    }

    void free_user_snp_ctx(void*& user_snp_ctx) {}

    ptr<snapshot> last_snapshot() {
        std::lock_guard<std::mutex> ll(last_snapshot_lock_);
        return last_snapshot_;
    }

    ulong last_commit_index() { return last_commit_idx_; }

    void create_snapshot(snapshot& s, async_result<bool>::handler_type& when_done) {
        {
            std::lock_guard<std::mutex> ll(last_snapshot_lock_);
            // NOTE: We only handle logical snapshot.
            ptr<buffer> snp_buf = s.serialize();
            last_snapshot_ = snapshot::deserialize(*snp_buf);
        }
        ptr<std::exception> except(nullptr);
        bool ret = true;
        when_done(ret, except);
    }

private:
    ptr<snapshot> last_snapshot_;
    std::mutex last_snapshot_lock_;
    uint64_t last_commit_idx_;
};

struct test_config {
    test_config() = default;
    test_config(size_t id, std::string endpoint, std::string port)
        : srv_id_(id)
        , srv_endpoint_(endpoint)
        , log_path("")
        , port_(port) {
        std::cout << id << '\n';
        std::cout << endpoint << '\n';
    }
    size_t peer_size_;
    int byzantine_;
    size_t srv_id_;
    std::string srv_endpoint_;
    std::string log_path;
    std::string port_;
    std::string addr_;
};

test_config parse_config(int id, char* file_path) {
    Json::Value root;
    Json::Reader reader;
    std::ifstream json_file(file_path);
    // json_file >> root;

    bool succ = reader.parse(json_file, root);
    if (!succ) {
        std::cout << "This is not a valid json path or file";
        exit(0);
    }
    const Json::Value servers = root["servers"];
    if (!servers.isArray()) {
        std::cout << "servers key must be an array\n";
        exit(0);
    }
    int arr_size = static_cast<int>(servers.size());
    if (0 > id || id >= arr_size) {
        std::cout << "server id is greater than the number of servers\n";
        exit(0);
    }
    const Json::Value srv_cfg = servers[id];
    std::string my_endpoint =
        srv_cfg["address"].asString() + ":" + srv_cfg["port"].asString();
    if (my_endpoint.find("tcp://") == std::string::npos) {
        my_endpoint = "tcp://" + my_endpoint;
    }
    auto cfg =
        test_config(static_cast<size_t>(id), my_endpoint, srv_cfg["port"].asString());
    std::stringstream path;
    path << "/NuRaft-1.3.0-Debug/results/srv" << id << ".log";
    cfg.log_path = path.str().c_str();
    cfg.peer_size_ = static_cast<size_t>(arr_size);
    cfg.addr_ = srv_cfg["address"].asString();
    // temporary
    cfg.byzantine_ = 0;
    return cfg;
}

void run_leader(test_config& srv_data) { std::cout << "leader\n"; }
void run_follower(test_config& srv_data) {
    // using the same log configuration as raft_bench.cxx
    ptr<logger_wrapper> log_wrap_ = cs_new<logger_wrapper>(srv_data.log_path, 4);
    ptr<logger> logger = log_wrap_;
    // using state manager and state machine from raft_bench.cxx
    ptr<state_mgr> smgr_ =
        cs_new<comm::TestMgr>(srv_data.srv_id_, srv_data.srv_endpoint_);
    ptr<state_machine> sm_ = cs_new<dummy_sm>();
    // Start ASIO service.
    asio_service::options asio_opt;
    asio_opt.thread_pool_size_ = 32;
    ptr<asio_service> asio_svc_ = cs_new<asio_service>(asio_opt, log_wrap_);
    int port = stoi(srv_data.port_);
    if (port < 1000) {
        std::cerr << "wrong port (should be >= 1000): " << port << std::endl;
        exit(-1);
    }
    ptr<rpc_listener> asio_listener_ = asio_svc_->create_rpc_listener(port, logger);
    ptr<delayed_task_scheduler> scheduler = asio_svc_;
    ptr<rpc_client_factory> rpc_cli_factory = asio_svc_;
    // using params from raft_bench.cxx
    raft_params params;
    params.heart_beat_interval_ = 500;
    params.election_timeout_lower_bound_ = 1000;
    params.election_timeout_upper_bound_ = 2000;
    params.reserved_log_items_ = 10000000;
    params.snapshot_distance_ = 100000;
    params.client_req_timeout_ = 4000;
    params.return_method_ = raft_params::blocking;
    context* ctx = new context(
        smgr_, sm_, asio_listener_, logger, rpc_cli_factory, scheduler, params);
    ptr<raft_server> node;
    if (srv_data.byzantine_ == -1)
        node = cs_new<byz_server>(ctx);
    else if (srv_data.byzantine_ == 0)
        node = cs_new<raft_server>(ctx);
    // Listen.
    asio_listener_->listen(node);

    // Wait until Raft server is ready (upto 10 seconds).
    const size_t MAX_TRY = 40;
    _msg("init Raft instance ");
    for (size_t ii = 0; ii < MAX_TRY; ++ii) {
        if (node->is_initialized()) {
            _msg(" done\n");
            while (true) {
            }
            return;
        }
        _msg(".");
        fflush(stdout);
        TestSuite::sleep_ms(250);
    }
    std::cout << " FAILED" << std::endl;
    exit(-1);
    return;
}
} // namespace server
using namespace server;
void usage(int argc, char** argv) {
    std::stringstream ss;
    ss << "Usage: \n"
       << "    - Leader:\n"
       << "    server 1 /path/to/config.json\n"
       << std::endl
       << "    - Follower:\n"
       << "    server <server ID> /path/to/config.json\n"
       << std::endl;

    std::cout << ss.str();
}

int main(int argc, char** argv) {
    std::cout << argc;
    // 0            1              2            3 ...
    // <exec> <server_id> </path/to/config>    <flags>
    if (argc < 3) {
        usage(argc, argv);
        return 0;
    }
    size_t id = atoi(argv[1]);
    test_config srv_data = parse_config(id, argv[2]);
    if (id == 1) {
        run_leader(srv_data);
    } else if (id > 0) {
        run_follower(srv_data);
    } else {
        std::cout << "server id must be positive\n";
    }
    std::cout << "WE ARE GOOD\n";
    return 0;
}