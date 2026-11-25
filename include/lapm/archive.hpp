#pragma once

#include "lapm/error.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace lapm {

namespace fs = std::filesystem;

// Extract a .lof (tar.gz) safely:
//   - Rejects absolute paths and ".." components.
//   - Rejects symlinks whose canonical target escapes dest.
//   - Skips entries under "scripts/" (they are handled separately).
// On success, returns the list of relative paths extracted (files + symlinks).
Result<std::vector<std::string>> safe_extract(const fs::path& archive,
                                              const fs::path& dest);

// Extract only the "scripts/" subtree of a .lof into a separate directory.
// Returns the list of relative paths (e.g. "scripts/postinst").
Result<std::vector<std::string>> extract_scripts(const fs::path& archive,
                                                 const fs::path& dest);

// Create a .lof from a directory tree.
// Sources are collected from fs::recursive_directory_iterator, ignoring
// any path under "scripts/". mtime is set to SOURCE_DATE_EPOCH if available,
// otherwise to 0 (reproducible builds).
Result<void> create_package(const fs::path& source_dir,
                            const fs::path& output_file);

} // namespace lapm
