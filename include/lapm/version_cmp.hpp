#pragma once

#include "lapm/error.hpp"

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lapm {

// Semantic version comparison.
// Handles forms like "1.2.3", "1.10", "20240101", "1.0-rc1".
struct Version {
    std::vector<std::uint64_t> numbers;
    std::string suffix; // "rc1", "beta", "" (empty = stable)

    static Result<Version> parse(std::string_view s);

    [[nodiscard]] std::strong_ordering operator<=>(const Version& other) const;
    [[nodiscard]] bool operator==(const Version& other) const;
    [[nodiscard]] std::string str() const;
};

} // namespace lapm
