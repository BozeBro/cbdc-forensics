#include "nuraft.hxx"

#include <iostream>
#include <jsoncpp/json/json.h>
#include <sstream>

using namespace nuraft;
namespace server {
struct test_config {
    test_config() = default;
};

test_config parse_config(char* file_path) { return test_config(); }
void run_leader(test_config& srv_data) {}
void run_follower(test_config& srv_data) {}
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
    // 0            1              2
    // <exec> <server_id> </path/to/config>
    if (argc != 3) {
        usage(argc, argv);
        return 0;
    }
    test_config srv_data = parse_config(argv[1]);
    int id = atoi(argv[1]);
    if (id == 1) {
        run_leader(srv_data);
    } else {
        run_follower(srv_data);
    }
    return 0;
}