#include <array>
#include <cstdio>
#include <iostream>
#include <string>

#ifdef _WIN32
#define AXENT_POPEN _popen
#define AXENT_PCLOSE _pclose
#else
#define AXENT_POPEN popen
#define AXENT_PCLOSE pclose
#endif

namespace {

int fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        return fail("usage: cli_smoke_test <axent-path>");
    }

    const std::string command = std::string(argv[1]) + " status --offline";
    std::array<char, 256> buffer{};
    std::string output;
    FILE* pipe = AXENT_POPEN(command.c_str(), "r");
    if (pipe == nullptr) {
        return fail("failed to launch axent command");
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    const int code = AXENT_PCLOSE(pipe);
    if (code != 0) {
        return fail("axent command exited with non-zero status");
    }
    if (output.find("axentd") == std::string::npos) {
        return fail("status output did not mention axentd");
    }
    if (output.find("offline") == std::string::npos) {
        return fail("status output did not mention offline");
    }
    return 0;
}
