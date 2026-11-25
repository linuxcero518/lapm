#include "lapm/repository.hpp"
#include "lapm/config.hpp"
#include "lapm/logger.hpp"

#include <fstream>
#include <map>

namespace lapm {

namespace {

// Reads every non-empty, non-comment line of a file into a vector.
Result<std::vector<std::string>> read_lines(const fs::path& p) {
    std::ifstream in(p);
    if (!in) {
        return fail(Errc::NotFound, p.string());
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.front() == '#') continue;
        lines.push_back(std::move(line));
    }
    if (!in.good() && !in.eof()) {
        return fail(Errc::IoError, p.string());
    }
    return lines;
}

} // namespace

Result<Repository> Repository::load_index() {
    auto& cfg = Config::instance();
    auto lines = read_lines(cfg.lofpack_list());
    if (!lines) return std::unexpected(lines.error());

    Repository repo;
    repo.packages_.reserve(lines->size());
    for (const auto& line : *lines) {
        auto p = Package::parse(line);
        if (!p) {
            log_warn("skipping malformed entry in lofpack.list: {}", line);
            continue;
        }
        repo.packages_.push_back(std::move(*p));
    }
    return repo;
}

Result<std::string> Repository::load_active_url() {
    auto& cfg = Config::instance();
    auto url = cfg.active_repo();
    if (!url) {
        return fail(Errc::NotFound, cfg.repository_list().string());
    }
    return *url;
}

std::optional<Package> Repository::find_best(std::string_view name) const {
    const Package* best = nullptr;
    for (const auto& p : packages_) {
        if (p.name != name) continue;
        if (best == nullptr) {
            best = &p;
            continue;
        }
        if (p.version > best->version) {
            best = &p;
        } else if (p.version == best->version) {
            // Same version: prefer higher revision (lexicographic fallback).
            if (p.revision > best->revision) best = &p;
        }
    }
    if (best == nullptr) return std::nullopt;
    return *best;
}

std::optional<Package> Repository::find_exact(std::string_view raw) const {
    for (const auto& p : packages_) {
        if (p.raw == raw) return p;
    }
    return std::nullopt;
}

} // namespace lapm
