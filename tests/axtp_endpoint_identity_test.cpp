#include <iostream>
#include <stdexcept>

#include "axent/adapters/axtp_endpoint_identity.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << std::endl;
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    require(
        axent::axtp_endpoint_id_from_key("device:axtp:hid:1234:5678:SERIAL-1")
            == "ep_3340a334b47934f471968db6b1470da6",
        "Axent must delegate canonical Endpoint identity to the AXTP runtime");

    bool empty_key_rejected = false;
    try {
        (void)axent::axtp_endpoint_id_from_key("");
    } catch (const std::invalid_argument&) {
        empty_key_rejected = true;
    }
    require(empty_key_rejected, "an empty Endpoint key must be rejected");
    return 0;
}
