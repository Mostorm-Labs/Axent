#include "axent/core/adapter_registry.hpp"

#include <algorithm>
#include <utility>

namespace axent {

void AdapterRegistry::register_adapter(AdapterMetadata metadata)
{
    auto existing = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& current) {
        return current.name == metadata.name;
    });
    if (existing == adapters_.end()) {
        adapters_.push_back(std::move(metadata));
        return;
    }
    *existing = std::move(metadata);
}

std::optional<AdapterMetadata> AdapterRegistry::find(const std::string& name) const
{
    auto found = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& current) {
        return current.name == name;
    });
    if (found == adapters_.end()) {
        return std::nullopt;
    }
    return *found;
}

const std::vector<AdapterMetadata>& AdapterRegistry::all() const
{
    return adapters_;
}

} // namespace axent
