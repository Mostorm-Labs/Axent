#pragma once

#include <optional>
#include <string>
#include <vector>

namespace axent {

struct AdapterMetadata {
    std::string name;
    std::string display_name;
    bool available = false;
    std::string unavailable_reason;
};

class AdapterRegistry {
public:
    void register_adapter(AdapterMetadata metadata);
    std::optional<AdapterMetadata> find(const std::string& name) const;
    const std::vector<AdapterMetadata>& all() const;

private:
    std::vector<AdapterMetadata> adapters_;
};

} // namespace axent
