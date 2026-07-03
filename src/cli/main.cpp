#include <iostream>
#include <string>

#include "axent/version.hpp"

namespace {

void print_usage()
{
    std::cout << axent::product_name() << ' ' << axent::version()
              << "\nusage: axent status|list|reload|diagnostics\n";
}

} // namespace

int main(int argc, char** argv)
{
    const std::string command = argc >= 2 ? argv[1] : "help";

    if (command == "status") {
        const bool offline = argc >= 3 && std::string(argv[2]) == "--offline";
        std::cout << "axentd status: " << (offline ? "offline" : "unknown") << '\n';
        return 0;
    }
    if (command == "list") {
        std::cout << "axent list: connect to axentd websocket for live devices\n";
        return 0;
    }
    if (command == "reload") {
        std::cout << "axent reload: requested\n";
        return 0;
    }
    if (command == "diagnostics") {
        std::cout << "axent diagnostics: requested\n";
        return 0;
    }

    print_usage();
    return command == "help" || command == "--help" || command == "-h" ? 0 : 1;
}
