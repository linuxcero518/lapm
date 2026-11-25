#pragma once

#include "lapm/database.hpp"
#include "lapm/error.hpp"
#include "lapm/manifest.hpp"
#include "lapm/repository.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lapm {

// Result of a dependency resolution pass.
struct ResolutionPlan {
    // Packages to install, in topological order (deps before dependents).
    std::vector<Package> install_order;

    // Packages already installed (will be skipped).
    std::vector<std::string> already_installed;

    // Total download size (sum of manifest download_size, in MB).
    std::uintmax_t download_mb = 0;

    // Total disk size (sum of manifest disk_size, in MB).
    std::uintmax_t disk_mb = 0;
};

// Resolves the transitive dependency graph for a set of requested packages.
// For each dependency that is not installed, looks up the best match in the
// repository and recurses. Cycles are detected and reported as an error.
class Resolver {
public:
    Resolver(const Repository& repo, const Database& db, const fs::path& info_dir);

    // resolve({"nano"}) -> plan with nano and its deps.
    // If "skip_installed" is true, installed packages are skipped and listed
    // in plan.already_installed. If false, they are included in the plan.
    Result<ResolutionPlan> resolve(const std::vector<std::string>& requested,
                                   bool skip_installed = true);

private:
    Result<void> visit(std::string_view name,
                       bool skip_installed,
                       std::unordered_set<std::string>& visiting,
                       std::unordered_set<std::string>& visited,
                       ResolutionPlan& plan);

    const Repository& repo_;
    const Database& db_;
    fs::path info_dir_;
};

} // namespace lapm
