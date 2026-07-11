#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace axent {

struct AxtpCliInvocation {
    std::string display_name;
    std::string executable_path;
    std::string log_stem;
};

int run_axtp_cli(const std::vector<std::string>& args,
                 const AxtpCliInvocation& invocation,
                 std::ostream& out,
                 std::ostream& err);

int run_axtp_cli(const std::vector<std::string>& args,
                 const std::string& executable_path,
                 std::ostream& out,
                 std::ostream& err);

} // namespace axent
