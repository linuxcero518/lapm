#pragma once

#include "lapm/error.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace lapm {

namespace fs = std::filesystem;

// Writes content to dest atomically (temp file + rename).
// The temp file is created in the same directory to guarantee that the
// rename is on the same filesystem and therefore atomic.
Result<void> atomic_write(const fs::path& dest, std::string_view content);

// Creates a directory and all its parents. Succeeds if it already exists.
Result<void> ensure_dir(const fs::path& p);

// Verifies a detached gpg signature using gpgv.
// Wraps `gpgv --keyring <keyring> <sig> <data>` via posix_spawn.
Result<void> verify_gpg(const fs::path& keyring,
                        const fs::path& sig,
                        const fs::path& data);

// Returns true if the process has an effective UID of 0.
[[nodiscard]] bool is_root() noexcept;

// Asks the user a yes/no question on the terminal.
// Returns true if the answer starts with 's' or 'S'.
// If "assume_yes" is true, returns true without prompting.
[[nodiscard]] bool confirm(std::string_view question, bool assume_yes = false);

// Exclusive lock on a file using flock(2). RAII.
class FileLock {
public:
    // Creates and locks the file at p (creating parents if needed).
    // If the lock cannot be acquired, held() returns false and last_error()
    // contains the reason.
    explicit FileLock(const fs::path& p);
    ~FileLock();

    FileLock(const FileLock&)            = delete;
    FileLock& operator=(const FileLock&) = delete;
    FileLock(FileLock&&) noexcept;
    FileLock& operator=(FileLock&&) noexcept;

    [[nodiscard]] bool held() const noexcept { return fd_ >= 0; }
    [[nodiscard]] Result<void> release();

private:
    int fd_ = -1;
    fs::path path_;
};

} // namespace lapm
