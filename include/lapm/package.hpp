#pragma once

#include "lapm/error.hpp"
#include "lapm/version_cmp.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace lapm {

namespace fs = std::filesystem;

// Naming convention: <name>_<version>-<revision>_<arch>
// Example: nano_8.7-1_i686
struct Package {
    std::string name;
    Version     version;
    std::string revision;
    std::string arch;
    std::string raw; // original canonical string

    static Result<Package> parse(std::string_view s);
    static Result<Package> from_filename(const fs::path& p); // expects .lof

    [[nodiscard]] std::string str() const;
    [[nodiscard]] fs::path filename() const; // "<raw>.lof"
};

} // namespace lapm
