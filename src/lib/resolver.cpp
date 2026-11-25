#include "lapm/resolver.hpp"
#include "lapm/logger.hpp"

#include <algorithm>
#include <format>

namespace lapm {

Resolver::Resolver(const Repository& repo, const Database& db, const fs::path& info_dir)
    : repo_{repo}, db_{db}, info_dir_{info_dir} {}

Result<ResolutionPlan> Resolver::resolve(const std::vector<std::string>& requested,
                                          bool skip_installed) {
    ResolutionPlan plan;
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;

    for (const auto& name : requested) {
        auto r = visit(name, skip_installed, visiting, visited, plan);
        if (!r) return std::unexpected(r.error());
    }

    return plan;
}

Result<void> Resolver::visit(std::string_view name,
                             bool skip_installed,
                             std::unordered_set<std::string>& visiting,
                             std::unordered_set<std::string>& visited,
                             ResolutionPlan& plan) {
    std::string key{name};
    if (visited.contains(key)) return {};

    if (visiting.contains(key)) {
        return fail(Errc::UnsatisfiedDeps,
                    std::format("dependency cycle involving '{}'", key));
    }
    visiting.insert(key);

    // Find the best package in the repository.
    auto pkg = repo_.find_best(key);
    if (!pkg) {
        // Maybe the user passed the full canonical form.
        pkg = repo_.find_exact(key);
    }
    if (!pkg) {
        return fail(Errc::NotFound,
                    std::format("package '{}' not found in repository", key));
    }

    // Skip if already installed (unless caller wants full list).
    if (skip_installed && db_.is_installed(pkg->name)) {
        plan.already_installed.push_back(pkg->name);
        visiting.erase(key);
        visited.insert(key);
        return {};
    }

    // Read the manifest for the exact version to get deps and sizes.
    auto info_path = info_dir_ / (pkg->raw + ".info");
    auto manifest = Manifest::from_file(info_path);
    if (!manifest) {
        // Fallback: try name-only .info
        auto alt = info_dir_ / (pkg->name + ".info");
        manifest = Manifest::from_file(alt);
        if (!manifest) {
            return fail(Errc::InvalidManifest,
                        std::format("no .info for '{}'", pkg->raw));
        }
    }

    // Recurse into dependencies FIRST (post-order = topological).
    for (const auto& dep : manifest->dependencies) {
        auto r = visit(dep, skip_installed, visiting, visited, plan);
        if (!r) return r;
    }

    plan.install_order.push_back(*pkg);
    plan.download_mb += Manifest::parse_human_size(manifest->download_size);
    plan.disk_mb     += Manifest::parse_human_size(manifest->disk_size);

    visiting.erase(key);
    visited.insert(key);
    return {};
}

} // namespace lapm
