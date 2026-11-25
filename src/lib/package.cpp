#include "lapm/package.hpp"

#include <format>

namespace lapm {

Result<Package> Package::parse(std::string_view s) {
    // Expected format: name_version-rev_arch
    auto first_us = s.find('_');
    if (first_us == std::string_view::npos) {
        return fail(Errc::InvalidPackageName,
                    std::format("missing '_' in '{}'", s));
    }
    auto second_us = s.find('_', first_us + 1);
    if (second_us == std::string_view::npos) {
        return fail(Errc::InvalidPackageName,
                    std::format("missing second '_' in '{}'", s));
    }

    auto name   = s.substr(0, first_us);
    auto middle = s.substr(first_us + 1, second_us - first_us - 1);
    auto arch   = s.substr(second_us + 1);

    if (name.empty() || middle.empty() || arch.empty()) {
        return fail(Errc::InvalidPackageName,
                    std::format("empty component in '{}'", s));
    }

    auto dash = middle.rfind('-');
    if (dash == std::string_view::npos) {
        return fail(Errc::InvalidPackageName,
                    std::format("missing '-' between version and revision in '{}'", s));
    }

    auto version_s  = middle.substr(0, dash);
    auto revision_s = middle.substr(dash + 1);

    auto v = Version::parse(version_s);
    if (!v) return std::unexpected(v.error());

    Package p;
    p.name     = std::string{name};
    p.version  = std::move(*v);
    p.revision = std::string{revision_s};
    p.arch     = std::string{arch};
    p.raw      = std::string{s};
    return p;
}

Result<Package> Package::from_filename(const fs::path& p) {
    if (p.extension() != ".lof") {
        return fail(Errc::InvalidPackageName,
                    std::format("not a .lof file: {}", p.string()));
    }
    return parse(p.stem().string());
}

std::string Package::str() const {
    return std::format("{}_{}-{}_{}", name, version.str(), revision, arch);
}

fs::path Package::filename() const {
    return fs::path{str() + ".lof"};
}

} // namespace lapm
