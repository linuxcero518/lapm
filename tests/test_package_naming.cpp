#include <catch2/catch_test_macros.hpp>

#include "lapm/package.hpp"

using namespace lapm;

TEST_CASE("Package parses canonical name", "[package]") {
    auto p = Package::parse("nano_8.7-1_i686");
    REQUIRE(p.has_value());
    REQUIRE(p->name == "nano");
    REQUIRE(p->version.str() == "8.7");
    REQUIRE(p->revision == "1");
    REQUIRE(p->arch == "i686");
    REQUIRE(p->raw == "nano_8.7-1_i686");
}

TEST_CASE("Package roundtrips via str()", "[package]") {
    auto p = Package::parse("libc_2.39-3_x86_64");
    REQUIRE(p.has_value());
    REQUIRE(p->str() == "libc_2.39-3_x86_64");
}

TEST_CASE("Package rejects malformed", "[package]") {
    REQUIRE_FALSE(Package::parse("nano").has_value());
    REQUIRE_FALSE(Package::parse("nano_8.7_i686").has_value());
    REQUIRE_FALSE(Package::parse("_8.7-1_i686").has_value());
}
