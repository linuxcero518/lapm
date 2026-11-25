#include "lapm/database.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/logger.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace lapm {

namespace {

Result<std::vector<std::string>> read_lines_if_exists(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) {
        return std::vector<std::string>{};
    }
    std::ifstream in(p);
    if (!in) {
        return fail(Errc::IoError, p.string());
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(std::move(line));
    }
    return lines;
}

std::string join_lines(const std::vector<std::string>& v) {
    std::string out;
    for (const auto& s : v) {
        out += s;
        out += '\n';
    }
    return out;
}

} // namespace

Database::Database(const fs::path& root) : root_{root} {}

// -------------------- tracking --------------------

fs::path Database::tracking_path(std::string_view pkg) const {
    return root_ / "tracking" / (std::string{pkg} + ".tracking");
}

bool Database::is_installed(std::string_view pkg) const {
    std::error_code ec;
    return fs::is_regular_file(tracking_path(pkg), ec) && !ec;
}

Result<std::vector<std::string>> Database::read_tracking(std::string_view pkg) const {
    auto p = tracking_path(pkg);
    std::ifstream in(p);
    if (!in) {
        return fail(Errc::NotFound, p.string());
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(std::move(line));
    }
    return lines;
}

Result<void> Database::write_tracking(std::string_view pkg,
                                      const std::vector<std::string>& files) {
    return atomic_write(tracking_path(pkg), join_lines(files));
}

Result<void> Database::remove_tracking(std::string_view pkg) {
    std::error_code ec;
    fs::remove(tracking_path(pkg), ec);
    if (ec) {
        return fail(Errc::IoError, ec.message());
    }
    return {};
}

Result<std::vector<std::string>> Database::list_installed() const {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root_ / "tracking", ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        auto name = entry.path().filename().string();
        if (name.size() > 9 && name.ends_with(".tracking")) {
            out.push_back(name.substr(0, name.size() - 9));
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// -------------------- info --------------------

fs::path Database::info_path(std::string_view pkg) const {
    return root_ / "info" / (std::string{pkg} + ".info");
}

Result<Manifest> Database::read_info(std::string_view pkg) const {
    return Manifest::from_file(info_path(pkg));
}

Result<void> Database::write_info(std::string_view pkg, const Manifest& m) {
    return atomic_write(info_path(pkg), m.to_string());
}

Result<void> Database::set_status(std::string_view pkg, std::string_view status) {
    auto m = read_info(pkg);
    if (!m) return std::unexpected(m.error());
    m->status = std::string{status};
    return write_info(pkg, *m);
}

// -------------------- ideps --------------------

fs::path Database::ideps_path(std::string_view pkg) const {
    return root_ / "ideps" / (std::string{pkg} + ".ideps");
}

Result<std::vector<std::string>> Database::read_ideps(std::string_view pkg) const {
    return read_lines_if_exists(ideps_path(pkg));
}

Result<void> Database::add_idep(std::string_view dep, std::string_view dependent) {
    if (dep.empty()) return {};

    auto file = ideps_path(dep);
    if (auto r = ensure_dir(file.parent_path()); !r) return r;

    auto existing = read_lines_if_exists(file);
    if (!existing) return std::unexpected(existing.error());

    if (std::find(existing->begin(), existing->end(), dependent) != existing->end()) {
        return {};
    }
    existing->emplace_back(dependent);
    return atomic_write(file, join_lines(*existing));
}

Result<void> Database::remove_ideps(std::string_view pkg) {
    std::error_code ec;
    fs::remove(ideps_path(pkg), ec);
    if (ec) {
        return fail(Errc::IoError, ec.message());
    }
    return {};
}

// -------------------- desinst --------------------

bool Database::has_desinst(std::string_view pkg) const {
    std::error_code ec;
    return fs::is_regular_file(desinst_path(pkg), ec) && !ec;
}

fs::path Database::desinst_path(std::string_view pkg) const {
    return root_ / "desinst" / (std::string{pkg} + ".desinst");
}

Result<void> Database::remove_desinst(std::string_view pkg) {
    std::error_code ec;
    fs::remove(desinst_path(pkg), ec);
    if (ec) {
        return fail(Errc::IoError, ec.message());
    }
    return {};
}

} // namespace lapm
