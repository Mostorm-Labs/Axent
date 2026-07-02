#pragma once

#include <cassert>
#include <nlohmann/json.hpp>

inline void assert_json_eq(const nlohmann::json& actual, const nlohmann::json& expected)
{
    if (actual != expected) {
        assert(actual.dump(2) == expected.dump(2));
    }
}
