#include "lapm/commands/upgrade.hpp"

#include "lapm/config.hpp"
#include "lapm/database.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/installer.hpp"
#include "lapm/logger.hpp"
#include "lapm/package.hpp"
#include "lapm/repository.hpp"
#include "lapm/resolver.hpp"
#include "lapm/version_cmp.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <vector>

namespace lapm {

namespace {

// Reads the version of an installed package from its .info file.
// The .info is expected at <info>/<pkg>.info or <info>/<raw>.info.
Result<std::string> installed_version(const Database& db,
                                      const fs::path& info_dir,
                                      const std::string& name) {
    auto m = db.read_info(name);
    if (m) {
        if (m->version.empty()) {
            return fail(Errc::InvalidManifest,
                        std::format("{} has empty version in .info", name));
        }
        return m->version;
    }
    // Fallback: scan info_dir for any <name>_<...>.info with matching stem prefix.
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(info_dir, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".info") continue;
        // Copy the path before mutating it (replace_extension is non-const).
        fs::path lof_path = e.path();
        lof_path.replace_extension(".lof");
        auto pkg = Package::from_filename(lof_path);
        if (!pkg) continue;
        if (pkg->name != name) continue;
        return pkg->version.str();
    }
    return fail(Errc::NotFound,
                std::format("no .info found for installed package '{}'", name));
}

struct Candidate {
    std::string installed;
    Package     best;
};

} // namespace

Result<void> cmd_upgrade(const UpgradeOptions& opts) {
    auto& cfg = Config::instance();

    auto base_url = Repository::load_active_url();
    if (!base_url) return std::unexpected(base_url.error());

    auto repo = Repository::load_index();
    if (!repo) return std::unexpected(repo.error());

    Database db{cfg.root()};
    auto installed = db.list_installed();
    if (!installed) return std::unexpected(installed.error());
    if (installed->empty()) {
        log_info("no packages installed");
        return {};
    }

    std::vector<Candidate> candidates;
    std::vector<std::string> orphans;
    std::vector<std::string> up_to_date;

    for (const auto& name : *installed) {
        auto cur = installed_version(db, cfg.info_dir(), name);
        if (!cur) {
            log_warn("skipping {}: {}", name, cur.error().message());
            continue;
        }

        auto best = repo->find_best(name);
        if (!best) {
            orphans.push_back(name);
            continue;
        }

        auto cur_v = Version::parse(*cur);
        if (!cur_v) {
            log_warn("skipping {}: invalid installed version '{}'", name, *cur);
            continue;
        }

        if (best->version > *cur_v) {
            candidates.push_back(Candidate{*cur, *best});
        } else {
            up_to_date.push_back(name);
        }
    }

    if (!orphans.empty()) {
        for (const auto& o : orphans) {
            log_warn("{} is installed but not present in the repository", o);
        }
    }

    if (candidates.empty()) {
        log_info("all packages are up to date ({} installed, {} checked)",
                 installed->size(), up_to_date.size());
        return {};
    }

    log_info("{} package(s) can be upgraded:", candidates.size());
    for (const auto& c : candidates) {
        log_info("  {}: {} -> {}", c.best.name, c.installed, c.best.version.str());
    }

    if (opts.dry_run) {
        log_info("dry run, nothing changed");
        return {};
    }

    if (!confirm("Proceed with upgrade", opts.assume_yes)) {
        return fail(Errc::Cancelled, "");
    }

    // Build a resolution plan over all candidate names so new dependencies
    // are pulled in and installed in topological order.
    std::vector<std::string> requested;
    requested.reserve(candidates.size());
    for (const auto& c : candidates) requested.push_back(c.best.name);

    Resolver resolver{*repo, db, cfg.info_dir()};
    // skip_installed=false: we WANT the upgraded packages in the plan,
    // even if they are currently installed.
    auto plan = resolver.resolve(requested, /*skip_installed=*/false);
    if (!plan) return std::unexpected(plan.error());

    // Filter: only keep packages that are either new (not installed) or
    // that appear in the upgrade candidate set.
    std::vector<std::string> to_install;
    std::vector<std::string> to_reinstall;

    auto is_candidate = [&](std::string_view n) {
        return std::any_of(candidates.begin(), candidates.end(),
                           [&](const Candidate& c) { return c.best.name == n; });
    };

    for (const auto& p : plan->install_order) {
        if (is_candidate(p.name)) {
            to_reinstall.push_back(p.name);
        } else if (!db.is_installed(p.name)) {
            to_install.push_back(p.name);
        }
    }

    InstallOptions iopts;
    iopts.assume_yes = true;   // already confirmed above
    iopts.keep_cache = true;
    iopts.verbose    = opts.verbose;

    Installer installer{db, *repo, cfg.packs_cache(), cfg.info_dir()};

    // 1. Install new dependencies (regular install path).
    if (!to_install.empty()) {
        log_info("installing {} new dependencies", to_install.size());
        if (auto r = installer.install(to_install, *base_url, iopts); !r) {
            return r;
        }
    }

    // 2. Reinstall the candidates.
    std::size_t upgraded = 0;
    for (const auto& name : to_reinstall) {
        log_info("upgrading {}", name);
        InstallOptions ropts = iopts;
        ropts.reinstall = true;
        if (auto r = installer.reinstall(name, *base_url, ropts); !r) {
            log_error("failed to upgrade {}: {}", name, r.error().message());
            continue;
        }
        ++upgraded;
    }

    log_info("upgrade complete: {} package(s) upgraded", upgraded);
    return {};
}

} // namespace lapm
