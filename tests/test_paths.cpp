#include <catch2/catch_test_macros.hpp>

#include "lapm/paths.hpp"

using namespace lapm;

TEST_CASE("dist_url adds trailing slash", "[paths]") {
    REQUIRE(dist_url("https://example.com/repo", "lofros", "x86_64") ==
            "https://example.com/repo/dists/lofros/x86_64/");
}

TEST_CASE("dist_url keeps existing trailing slash", "[paths]") {
    REQUIRE(dist_url("https://example.com/repo/", "lofros", "x86_64") ==
            "https://example.com/repo/dists/lofros/x86_64/");
}
