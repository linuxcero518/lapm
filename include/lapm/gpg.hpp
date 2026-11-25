#pragma once

#include "lapm/error.hpp"

#include <filesystem>

namespace lapm {

namespace fs = std::filesystem;

// Verifies a detached gpg signature over data using the given keyring.
// Thin wrapper around fs_utils::verify_gpg() kept in its own header so the
// installer and the update command can depend on gpg without pulling in
// the whole fs_utils header (they still can, but this keeps intent clear).
Result<void> verify_signature(const fs::path& keyring,
                              const fs::path& sig,
                              const fs::path& data);

} // namespace lapm
