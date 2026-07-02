#pragma once

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

inline void assert_json_eq(const nlohmann::json& actual, const nlohmann::json& expected)
{
    if (actual != expected) {
        throw std::runtime_error(
            std::string("JSON mismatch\nactual:\n") + actual.dump(2) + "\nexpected:\n" + expected.dump(2));
    }
}
