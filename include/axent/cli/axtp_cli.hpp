#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace axent {

int run_axtp_cli(const std::vector<std::string>& args,
                 const std::string& executable_path,
                 std::ostream& out,
                 std::ostream& err);

} // namespace axent
