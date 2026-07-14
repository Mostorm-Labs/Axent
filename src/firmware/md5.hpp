#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace axent::firmware::detail {

std::string md5_hex(const std::uint8_t* data, std::size_t size);

} // namespace axent::firmware::detail
