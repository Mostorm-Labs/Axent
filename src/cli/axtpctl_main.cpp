#include <iostream>
#include <string>
#include <vector>

#include "axent/tooling/axtp_cli.hpp"

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    args.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index] != nullptr ? argv[index] : "");
    }

    const std::string executable_path = argc > 0 && argv[0] != nullptr ? argv[0] : "axtpctl";
    return axent::run_axtp_cli(
        args,
        axent::AxtpCliInvocation{"axtpctl", executable_path, "axtpctl"},
        std::cout,
        std::cerr);
}
