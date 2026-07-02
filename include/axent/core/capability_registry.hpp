#pragma once

#include <optional>
#include <string>
#include <vector>

#include "axent/core/types.hpp"

namespace axent {

class CapabilityRegistry {
public:
    void register_core_capabilities();
    void register_capability(Capability capability);
    bool has(const std::string& name) const;
    bool has_method(const std::string& method) const;
    std::optional<CapabilityMethod> find_method(const std::string& method) const;
    const std::vector<Capability>& all() const;

private:
    std::vector<Capability> capabilities_;
};

} // namespace axent
