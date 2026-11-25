#include "lapm/fs_utils.hpp"
#include "lapm/logger.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <random>
#include <string>
#include <vector>

extern char** environ;

namespace lapm {

namespace {

// Generates a random suffix for temp files.
std::string random_suffix() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    return std::format(".tmp.{:016x}", rng());
}

// Runs an external command and waits for it. Returns exit status.
Result<int> run_process(const std::vector<std::string>& argv) {
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) {
        cargv.push_back(const_cast<char*>(a.c_str()));
    }
    cargv.push_back(nullptr);

    pid_t pid = 0;
    int rc = ::posix_spawnp(&pid, cargv[0], nullptr, nullptr, cargv.data(), environ);
    if (rc != 0) {
        return fail(Errc::ExternalToolFailed,
                    std::format("posix_spawnp({}) failed: {}", argv[0], std::strerror(rc)));
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            return fail(Errc::ExternalToolFailed,
                        std::format("waitpid failed: {}", std::strerror(errno)));
        }
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return fail(Errc::ExternalToolFailed,
                std::format("{} terminated abnormally", argv[0]));
}

} // namespace

Result<void> atomic_write(const fs::path& dest, std::string_view content) {
    auto dir = dest.parent_path();
    if (dir.empty()) dir = ".";

    if (auto r = ensure_dir(dir); !r) return r;

    fs::path tmp = dest;
    tmp += random_suffix();

    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return fail(Errc::IoError,
                        std::format("cannot open {} for writing", tmp.string()));
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out.flush()) {
            return fail(Errc::IoError,
                        std::format("write failed on {}", tmp.string()));
        }
    }

    std::error_code ec;
    fs::rename(tmp, dest, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(tmp, ignored);
        return fail(Errc::IoError,
                    std::format("rename {} -> {}: {}", tmp.string(), dest.string(), ec.message()));
    }
    return {};
}

Result<void> ensure_dir(const fs::path& p) {
    std::error_code ec;
    if (fs::exists(p, ec)) {
        if (ec) {
            return fail(Errc::IoError,
                        std::format("{}: {}", p.string(), ec.message()));
        }
        if (!fs::is_directory(p, ec)) {
            return fail(Errc::IoError,
                        std::format("{} exists and is not a directory", p.string()));
        }
        return {};
    }
    if (!fs::create_directories(p, ec) && ec) {
        return fail(Errc::IoError,
                    std::format("create_directories({}): {}", p.string(), ec.message()));
    }
    return {};
}

Result<void> verify_gpg(const fs::path& keyring,
                        const fs::path& sig,
                        const fs::path& data) {
    std::vector<std::string> argv = {
        "gpgv",
        "--keyring", keyring.string(),
        sig.string(),
        data.string(),
    };
    auto rc = run_process(argv);
    if (!rc) return std::unexpected(rc.error());
    if (*rc != 0) {
        return fail(Errc::GpgInvalid,
                    std::format("gpgv exited with status {}", *rc));
    }
    return {};
}

bool is_root() noexcept {
    return ::geteuid() == 0;
}

bool confirm(std::string_view question, bool assume_yes) {
    if (assume_yes) return true;

    std::fputs(question.data(), stdout);
    std::fputs(" (s/n): ", stdout);
    std::fflush(stdout);

    char buf[16] = {};
    if (std::fgets(buf, sizeof(buf), stdin) == nullptr) {
        return false;
    }
    return buf[0] == 's' || buf[0] == 'S';
}

// -------------------- FileLock --------------------

FileLock::FileLock(const fs::path& p) : path_{p} {
    if (auto r = ensure_dir(p.parent_path()); !r) {
        log_error("lock: {}", r.error().message());
        return;
    }

    fd_ = ::open(p.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd_ < 0) {
        log_error("lock: open({}) failed: {}", p.string(), std::strerror(errno));
        return;
    }

    if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        log_error("lock: flock({}) failed: {}", p.string(), std::strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return;
    }
}

FileLock::~FileLock() {
    if (fd_ >= 0) {
        ::flock(fd_, LOCK_UN);
        ::close(fd_);
    }
}

FileLock::FileLock(FileLock&& other) noexcept
    : fd_{other.fd_}, path_{std::move(other.path_)} {
    other.fd_ = -1;
}

FileLock& FileLock::operator=(FileLock&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
        }
        fd_ = other.fd_;
        path_ = std::move(other.path_);
        other.fd_ = -1;
    }
    return *this;
}

Result<void> FileLock::release() {
    if (fd_ < 0) return {};
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
    return {};
}

} // namespace lapm
