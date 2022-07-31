#include "nuraft.hxx"

#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <sstream>
#include <string>
using namespace nuraft;
namespace server {
struct test_config {
    test_config() = default;
    test_config(size_t id, std::string endpoint) {
        std::cout << id << '\n';
        std::cout << endpoint << '\n';
    }
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
    return test_config(static_cast<size_t>(id), my_endpoint);
}
void run_leader(test_config& srv_data) { std::cout << "leader\n"; }
void run_follower(test_config& srv_data) { std::cout << "follower\n"; }
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