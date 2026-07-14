#include "md5.hpp"

#include <array>
#include <vector>

namespace axent::firmware::detail {
namespace {

constexpr std::array<std::uint32_t, 64> k_constants{
    0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU,
    0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
    0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
    0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
    0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
    0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
    0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU,
    0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
    0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
    0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
    0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U,
    0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
    0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U,
    0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
    0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
    0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U,
};

constexpr std::array<std::uint32_t, 64> k_rotations{
    7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U,
    5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U,
    4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U,
    6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U,
};

std::uint32_t rotate_left(std::uint32_t value, std::uint32_t count)
{
    return (value << count) | (value >> (32U - count));
}

std::uint32_t read_u32_le(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

} // namespace

std::string md5_hex(const std::uint8_t* data, std::size_t size)
{
    const std::uint64_t bit_length = static_cast<std::uint64_t>(size) * 8U;
    std::vector<std::uint8_t> message;
    message.reserve(size + 72U);
    if (data != nullptr && size != 0U) {
        message.insert(message.end(), data, data + size);
    }
    message.push_back(0x80U);
    while ((message.size() % 64U) != 56U) {
        message.push_back(0U);
    }
    for (std::uint32_t index = 0; index < 8U; ++index) {
        message.push_back(static_cast<std::uint8_t>((bit_length >> (index * 8U)) & 0xffU));
    }

    std::uint32_t h0 = 0x67452301U;
    std::uint32_t h1 = 0xefcdab89U;
    std::uint32_t h2 = 0x98badcfeU;
    std::uint32_t h3 = 0x10325476U;

    for (std::size_t offset = 0; offset < message.size(); offset += 64U) {
        std::array<std::uint32_t, 16> words{};
        for (std::size_t index = 0; index < words.size(); ++index) {
            words[index] = read_u32_le(message.data() + offset + index * 4U);
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        for (std::uint32_t index = 0; index < 64U; ++index) {
            std::uint32_t f = 0;
            std::uint32_t word_index = 0;
            if (index < 16U) {
                f = (b & c) | ((~b) & d);
                word_index = index;
            } else if (index < 32U) {
                f = (d & b) | ((~d) & c);
                word_index = (5U * index + 1U) % 16U;
            } else if (index < 48U) {
                f = b ^ c ^ d;
                word_index = (3U * index + 5U) % 16U;
            } else {
                f = c ^ (b | (~d));
                word_index = (7U * index) % 16U;
            }

            const std::uint32_t previous_d = d;
            d = c;
            c = b;
            b += rotate_left(a + f + k_constants[index] + words[word_index], k_rotations[index]);
            a = previous_d;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
    }

    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(32U);
    for (const std::uint32_t word : {h0, h1, h2, h3}) {
        for (std::uint32_t index = 0; index < 4U; ++index) {
            const auto byte = static_cast<std::uint8_t>((word >> (index * 8U)) & 0xffU);
            result.push_back(digits[(byte >> 4U) & 0x0fU]);
            result.push_back(digits[byte & 0x0fU]);
        }
    }
    return result;
}

} // namespace axent::firmware::detail
