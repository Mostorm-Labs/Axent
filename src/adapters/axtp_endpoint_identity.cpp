#include "axent/adapters/axtp_endpoint_identity.hpp"

#include <core/protocol/endpoint/endpoint_identity.hpp>

namespace axent {

std::string axtp_endpoint_id_from_key(std::string_view endpoint_key)
{
    return axtp::endpointIdFromKey(endpoint_key);
}

} // namespace axent
