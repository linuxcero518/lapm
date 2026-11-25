#include <catch2/catch_test_macros.hpp>

#include "lapm/version_cmp.hpp"

using namespace lapm;

TEST_CASE("Version parses simple forms", "[version]") {
    auto v = Version::parse("1.2.3");
    REQUIRE(v.has_value());
    REQUIRE(v->numbers.size() == 3);
    REQUIRE(v->numbers[0] == 1);
    REQUIRE(v->numbers[1] == 2);
    REQUIRE(v->numbers[2] == 3);
    REQUIRE(v->suffix.empty());
}

TEST_CASE("Version parses pre-release suffix", "[version]") {
    auto v = Version::parse("1.0-rc1");
    REQUIRE(v.has_value());
    REQUIRE(v->numbers.size() == 2);
    REQUIRE(v->suffix == "rc1");
}

TEST_CASE("Version 1.10 > 1.9", "[version]") {
    auto a = Version::parse("1.10");
    auto b = Version::parse("1.9");
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    REQUIRE(*a > *b);
}

TEST_CASE("Version stable > pre-release", "[version]") {
    auto stable = Version::parse("1.0");
    auto pre = Version::parse("1.0-rc1");
    REQUIRE(stable.has_value());
    REQUIRE(pre.has_value());
    REQUIRE(*stable > *pre);
}

TEST_CASE("Version rejects empty and invalid", "[version]") {
    REQUIRE_FALSE(Version::parse("").has_value());
    REQUIRE_FALSE(Version::parse("abc").has_value());
}
