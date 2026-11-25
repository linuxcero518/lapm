#pragma once

#include "lapm/archive.hpp"
#include "lapm/database.hpp"
#include "lapm/error.hpp"
#include "lapm/fetcher.hpp"
#include "lapm/manifest.hpp"
#include "lapm/package.hpp"
#include "lapm/repository.hpp"
#include "lapm/resolver.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace lapm {

namespace fs = std::filesystem;

// Options shared across install/reinstall/remove.
struct InstallOptions {
    bool assume_yes   = false;   // -y / --yes
    bool verbose      = false;   // -v / --verbose
    bool keep_cache   = true;    // if false, delete .lof after install
    bool skip_scripts = false;   // do not run postinst/desinst (dangerous)
    bool reinstall    = false;   // for reinstall command
};

// Core installer. Owns the full lifecycle of install and remove.
// All methods return Result<void>; errors carry context.
class Installer {
public:
    Installer(Database db,
              Repository repo,
              fs::path cache_dir,
              fs::path info_dir);

    // Installs a single package from a local .lof file.
    // Used by both "install from cache" and "-il" (local package).
    Result<void> install_local(const fs::path& package_file,
                               const InstallOptions& opts);

    // Downloads a package into the cache (if not already there).
    Result<void> download(const Package& pkg, std::string_view base_url);

    // Runs the full pipeline for a set of packages:
    //   resolve -> confirm -> download -> install each in order.
    Result<void> install(const std::vector<std::string>& requested,
                         std::string_view base_url,
                         const InstallOptions& opts);

    // Reinstalls a single package (assumes it's installed).
    Result<void> reinstall(const std::string& name,
                           std::string_view base_url,
                           const InstallOptions& opts);

    // Removes a package, running desinst if present.
    Result<void> remove(const std::string& name,
                        const InstallOptions& opts,
                        bool show_files);

    // Cache cleanup.
    Result<void> clean_cache();

private:
    Result<void> run_postinst(const fs::path& script);
    Result<void> run_desinst(const fs::path& script);
    Result<void> copy_tree(const fs::path& src,
                           const fs::path& dst,
                           bool overwrite,
                           std::vector<std::string>& installed_files);

    Database db_;
    Repository repo_;
    fs::path cache_dir_;
    fs::path info_dir_;
};

} // namespace lapm
