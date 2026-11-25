#include "lapm/manifest.hpp"

#include <charconv>
#include <cctype>
#include <format>
#include <fstream>
#include <sstream>

namespace lapm {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        auto cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb) return false;
    }
    return true;
}

std::vector<std::string> split_ws(std::string_view s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= s.size()) break;
        std::size_t j = i;
        while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
        out.emplace_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

} // namespace

Result<Manifest> Manifest::parse(std::string_view text) {
    Manifest m;
    std::size_t line_start = 0;
    while (line_start <= text.size()) {
        std::size_t line_end = text.find('\n', line_start);
        if (line_end == std::string_view::npos) line_end = text.size();
        auto line = text.substr(line_start, line_end - line_start);

        auto colon = line.find(':');
        if (colon != std::string_view::npos) {
            auto key = trim(line.substr(0, colon));
            auto val = trim(line.substr(colon + 1));

            if (iequals(key, "Nombre")) {
                m.name = std::string{val};
            } else if (iequals(key, "versión") || iequals(key, "version")) {
                m.version = std::string{val};
            } else if (iequals(key, "Revisión") || iequals(key, "revision")) {
                m.revision = std::string{val};
            } else if (iequals(key, "Estado") || iequals(key, "status")) {
                m.status = std::string{val};
            } else if (iequals(key, "dependencias") || iequals(key, "dependencies")) {
                m.dependencies = split_ws(val);
            } else if (iequals(key, "Mantenedor") || iequals(key, "maintainer")) {
                m.maintainer = std::string{val};
            } else if (iequals(key, "Homepage")) {
                m.homepage = std::string{val};
            } else if (iequals(key, "Tamaño") || iequals(key, "size")) {
                m.download_size = std::string{val};
            } else if (iequals(key, "Espacio en disco") || iequals(key, "disk size")) {
                m.disk_size = std::string{val};
            } else if (iequals(key, "Descripción") || iequals(key, "description")) {
                m.description = std::string{val};
            }
        }

        if (line_end == text.size()) break;
        line_start = line_end + 1;
    }

    if (m.name.empty()) {
        return fail(Errc::InvalidManifest, "missing 'Nombre' field");
    }
    return m;
}

Result<Manifest> Manifest::from_file(const fs::path& p) {
    std::ifstream in(p);
    if (!in) {
        return fail(Errc::NotFound, p.string());
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (!in.good() && !in.eof()) {
        return fail(Errc::IoError, p.string());
    }
    return parse(ss.str());
}

std::string Manifest::to_string() const {
    std::string deps;
    for (std::size_t i = 0; i < dependencies.size(); ++i) {
        if (i) deps += ' ';
        deps += dependencies[i];
    }
    return std::format(
        "Nombre: {}\n"
        "versión: {}\n"
        "Revisión: {}\n"
        "Estado: {}\n"
        "dependencias: {}\n"
        "Mantenedor: {}\n"
        "Homepage: {}\n"
        "Tamaño: {}\n"
        "Espacio en disco: {}\n"
        "Descripción: {}\n",
        name, version, revision, status,
        deps, maintainer, homepage,
        download_size, disk_size, description);
}

std::uintmax_t Manifest::parse_human_size(std::string_view s) {
    // "1.5 MB" -> 1, "12 KB" -> 12, matching the old shell logic.
    auto sp = s.find(' ');
    if (sp == std::string_view::npos) return 0;
    auto num = trim(s.substr(0, sp));
    if (auto dot = num.find('.'); dot != std::string_view::npos) {
        num = num.substr(0, dot);
    }

    std::uintmax_t v = 0;
    auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), v);
    if (ec != std::errc{}) return 0;
    return v;
}

} // namespace lapm
