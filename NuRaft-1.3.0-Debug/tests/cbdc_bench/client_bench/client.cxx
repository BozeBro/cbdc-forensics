#include "nuraft.hxx"

#include <iostream>
#include <jsoncpp/json/json.h>
#include <sstream>
using namespace nuraft;
namespace client {
struct test_config {
    test_config() = default;
};
test_config parse_config(char* path) { return test_config(); }
void run_benchmark(test_config& cli_data) {}
} // namespace client

using namespace client;

void usage(int argc, char** argv) {
    std::stringstream ss;
    ss << "Usage: \n"
       << "    - Client:\n"
       << "    client_bench /path/to/config.json\n"
       << std::endl;
    std::cout << ss.str();
}

int main(int argc, char** argv) {
    // 0            1
    // <exec> </path/to/config>
    if (argc != 2) {
        usage(argc, argv);
        return 0;
    }
    test_config cli_data{parse_config(argv[1])};
    run_benchmark(cli_data);
    return 0;
}