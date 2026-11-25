#include "lapm/config.hpp"
#include "lapm/error.hpp"
#include "lapm/version.hpp"

#include <sys/utsname.h>

#include <cstdlib>
#include <fstream>
#include <string_view>

namespace lapm {

namespace {

std::optional<fs::path> env_root() {
    if (const char* v = std::getenv("LAPM_ROOT"); v != nullptr && *v != '\0') {
        return fs::path{v};
    }
    return std::nullopt;
}

std::optional<std::string> env_arch() {
    if (const char* v = std::getenv("LAPM_ARCH"); v != nullptr && *v != '\0') {
        return std::string{v};
    }
    return std::nullopt;
}

std::string trim_ws(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
        ++i;
    }
    if (i > 0) s.erase(0, i);
    return s;
}

} // namespace

Config& Config::instance() {
    static Config inst;
    return inst;
}

const fs::path& Config::root() const {
    if (!root_detected_) {
        const_cast<Config*>(this)->detect_root();
    }
    return root_;
}

const std::string& Config::architecture() const {
    if (!arch_detected_) {
        const_cast<Config*>(this)->detect_arch();
    }
    return arch_;
}

void Config::detect_root() {
    if (auto r = env_root()) {
        root_ = *r;
        root_detected_ = true;
        return;
    }

    std::error_code ec;
    if (fs::exists("/etc/lapm", ec) && !ec) {
        root_ = "/etc/lapm";
    } else if (fs::exists("/etc/lofpkg", ec) && !ec) {
        root_ = "/etc/lofpkg";
    } else {
        root_ = "/etc/lapm";
    }
    root_detected_ = true;
}

void Config::detect_arch() {
    if (auto a = env_arch()) {
        arch_ = *a;
        arch_detected_ = true;
        return;
    }

    // Try <root>/arch file (single line).
    std::ifstream f(root() / "arch");
    if (f) {
        std::string line;
        if (std::getline(f, line)) {
            line = trim_ws(std::move(line));
            if (!line.empty()) {
                arch_ = std::move(line);
                arch_detected_ = true;
                return;
            }
        }
    }

    // Fallback: uname -m.
    struct utsname u{};
    if (::uname(&u) == 0) {
        arch_ = std::string{u.machine};
    } else {
        arch_ = "unknown";
    }
    arch_detected_ = true;
}

fs::path Config::repository_dir() const   { return root() / "repository"; }
fs::path Config::repository_list() const  { return repository_dir() / "repository.list"; }
fs::path Config::lofpack_list() const     { return repository_dir() / "lofpack.list"; }
fs::path Config::trusted_dir() const      { return repository_dir() / "trusted.d"; }
fs::path Config::keyring() const          { return trusted_dir() / "lofpack.gpg"; }
fs::path Config::info_dir() const         { return root() / "info"; }
fs::path Config::tracking_dir() const     { return root() / "tracking"; }
fs::path Config::ideps_dir() const        { return root() / "ideps"; }
fs::path Config::desinst_dir() const      { return root() / "desinst"; }
fs::path Config::packs_cache() const      { return root() / "packs"; }
fs::path Config::extrepos_dir() const     { return repository_dir() / "extrepos.d"; }

std::optional<std::string> Config::active_repo() const {
    std::ifstream in(repository_list());
    if (!in) return std::nullopt;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.front() == '#') continue;
        line = trim_ws(std::move(line));
        if (!line.empty()) return line;
    }
    return std::nullopt;
}

std::string Config::distro() const {
    return std::string{kDefaultDistro};
}

} // namespace lapm
