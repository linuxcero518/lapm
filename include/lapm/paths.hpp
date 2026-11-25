#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace lapm {

namespace fs = std::filesystem;

// Builds the base URL for the repository's dists tree.
// Ensures the URL ends with a trailing '/'.
[[nodiscard]] std::string dist_url(std::string_view base_url,
                                   std::string_view distro,
                                   std::string_view arch);

} // namespace lapm
