#include <cassert>
#include <string>

#include "axent/version.hpp"

int main()
{
    assert(std::string(axent::product_name()) == "Axent");
    assert(std::string(axent::full_name()) == "Axtp Endpoint Agent");
    assert(std::string(axent::version()) == "0.1.0-dev");
    return 0;
}
