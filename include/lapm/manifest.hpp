#pragma once

#include "lapm/error.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lapm {

namespace fs = std::filesystem;

// Parsed representation of a .info file.
// The parser is tolerant of accented and non-accented key names because
// both "lofpkg-rep" and potential future tools may emit either form.
struct Manifest {
    std::string name;
    std::string version;
    std::string revision;
    std::string status;       // "Instalado" / "No instalado"
    std::vector<std::string> dependencies;
    std::string maintainer;
    std::string homepage;
    std::string download_size;  // raw: "1.5 MB"
    std::string disk_size;      // raw: "12.3 MB"
    std::string description;

    static Result<Manifest> parse(std::string_view text);
    static Result<Manifest> from_file(const fs::path& p);

    // Rebuilds a canonical .info file in the legacy format.
    [[nodiscard]] std::string to_string() const;

    // Extracts the integer part of a human-readable size (e.g. "1.5 MB" -> 1).
    // Mirrors the behavior of the original shell code (cut -d. -f1).
    [[nodiscard]] static std::uintmax_t parse_human_size(std::string_view s);
};

} // namespace lapm
