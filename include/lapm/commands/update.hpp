#pragma once

#include "lapm/error.hpp"

#include <filesystem>
#include <string>

namespace lapm {

namespace fs = std::filesystem;

// Options for the update command.
struct UpdateOptions {
    bool assume_yes    = false;
    bool insecure      = false;  // skip GPG verification (dangerous)
    bool verbose       = false;
    unsigned jobs      = 4;      // parallel .info downloads
};

// Runs the full update flow:
//   1. Download <repo>/dists/lofros/<arch>/lofpack.list -> temp
//   2. Download <repo>/dists/lofros/<arch>/lofpack.list.gpg -> temp
//   3. Verify gpgv with the configured keyring (unless insecure)
//   4. Atomically replace lofpack.list
//   5. Download every <pkg>.info referenced in lofpack.list (parallel)
//   6. Remove .info files that no longer appear in lofpack.list
Result<void> cmd_update(const UpdateOptions& opts);

} // namespace lapm
