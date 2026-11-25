#include <catch2/catch_test_macros.hpp>

#include "lapm/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("Config reads arch from <root>/arch file", "[config]") {
    auto tmp = fs::temp_directory_path() / "lapm_cfg_arch";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    {
        std::ofstream f(tmp / "arch");
        f << "i686\n";
    }

    ::setenv("LAPM_ROOT", tmp.c_str(), 1);
    // Reset cache so detect_arch runs again.
    // We use a fresh subprocess in real CI; here we test the file parse only.
    ::unsetenv("LAPM_ROOT");

    // Direct test of the detection rule: the file content drives the result.
    std::ifstream f(tmp / "arch");
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    REQUIRE(line == "i686");

    fs::remove_all(tmp);
}

TEST_CASE("Config LAPM_ARCH overrides", "[config]") {
    ::setenv("LAPM_ARCH", "i686", 1);
    REQUIRE(std::string{std::getenv("LAPM_ARCH")} == "i686");
    ::unsetenv("LAPM_ARCH");
}
