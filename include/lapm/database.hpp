#pragma once

#include "lapm/error.hpp"
#include "lapm/manifest.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lapm {

namespace fs = std::filesystem;

// High-level access to the on-disk database:
//   <root>/tracking/<pkg>.tracking  -- installed files
//   <root>/ideps/<pkg>.ideps        -- reverse dependencies
//   <root>/info/<pkg>.info          -- metadata
//   <root>/desinst/<pkg>.desinst    -- uninstall script
class Database {
public:
    explicit Database(const fs::path& root);

    // ----- tracking -----
    [[nodiscard]] bool is_installed(std::string_view pkg) const;
    [[nodiscard]] Result<std::vector<std::string>> read_tracking(std::string_view pkg) const;
    Result<void> write_tracking(std::string_view pkg,
                                const std::vector<std::string>& files);
    Result<void> remove_tracking(std::string_view pkg);
    [[nodiscard]] Result<std::vector<std::string>> list_installed() const;

    // ----- info -----
    [[nodiscard]] Result<Manifest> read_info(std::string_view pkg) const;
    Result<void> write_info(std::string_view pkg, const Manifest& m);
    Result<void> set_status(std::string_view pkg, std::string_view status);

    // ----- ideps -----
    [[nodiscard]] Result<std::vector<std::string>> read_ideps(std::string_view pkg) const;
    Result<void> add_idep(std::string_view dep, std::string_view dependent);
    Result<void> remove_ideps(std::string_view pkg);

    // ----- desinst -----
    [[nodiscard]] bool has_desinst(std::string_view pkg) const;
    [[nodiscard]] fs::path desinst_path(std::string_view pkg) const;
    Result<void> remove_desinst(std::string_view pkg);

    // Convenience path helpers.
    [[nodiscard]] fs::path tracking_path(std::string_view pkg) const;
    [[nodiscard]] fs::path info_path(std::string_view pkg) const;
    [[nodiscard]] fs::path ideps_path(std::string_view pkg) const;

private:
    fs::path root_;
};

} // namespace lapm
