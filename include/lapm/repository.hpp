#pragma once

#include "lapm/error.hpp"
#include "lapm/package.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lapm {

namespace fs = std::filesystem;

// In-memory representation of lofpack.list and repository.list.
class Repository {
public:
    // Loads lofpack.list from the configured location.
    static Result<Repository> load_index();

    // Loads repository.list and returns the first non-comment URL.
    static Result<std::string> load_active_url();

    // Returns every line of lofpack.list as a Package, in file order.
    [[nodiscard]] const std::vector<Package>& packages() const noexcept {
        return packages_;
    }

    // Finds a package by name, ignoring version/revision/arch.
    // If multiple versions exist, returns the highest one.
    [[nodiscard]] std::optional<Package> find_best(std::string_view name) const;

    // Finds a package by exact canonical string (name_version-rev_arch).
    [[nodiscard]] std::optional<Package> find_exact(std::string_view raw) const;

private:
    std::vector<Package> packages_;
};

} // namespace lapm
