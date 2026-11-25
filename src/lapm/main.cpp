#include "lapm/commands/update.hpp"
#include "lapm/commands/upgrade.hpp"
#include "lapm/config.hpp"
#include "lapm/error.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/installer.hpp"
#include "lapm/logger.hpp"
#include "lapm/repository.hpp"
#include "lapm/version.hpp"

#include <CLI/CLI.hpp>

#include <cstdio>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace lapm {
Result<void> not_implemented(std::string_view cmd);
}

namespace {

int handle_error(const lapm::Error& e) {
    lapm::log_error("{}", e.message());
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    using namespace lapm;

    if (!is_root()) {
        log_error("lapm requires root privileges");
        return 1;
    }

    CLI::App app{std::format("lapm {} - Lofros Advanced Package Manager", kVersion)};
    app.set_version_flag("-v,--version", std::string{kVersion});
    app.require_subcommand(0, 1);

    std::string root;
    app.add_option("--root", root, "Override data directory (default /etc/lapm)");

    std::string arch_override;
    app.add_option("--arch", arch_override,
                   "Target architecture (default: <root>/arch or uname -m)");

    bool assume_yes = false;
    app.add_flag("-y,--yes", assume_yes, "Answer yes to all prompts");
    bool verbose = false;
    app.add_flag("-V,--verbose", verbose, "Verbose output");

    // update
    auto* update_cmd = app.add_subcommand("update", "Update package lists");
    bool update_ignore_gpg = false;
    update_cmd->add_flag("--insecure", update_ignore_gpg, "Skip GPG verification");
    unsigned update_jobs = 4;
    update_cmd->add_option("-j,--jobs", update_jobs,
                           "Parallel .info downloads (default 4)");

    // install
    std::vector<std::string> install_pkgs;
    auto* install_cmd = app.add_subcommand("install", "Install packages");
    install_cmd->add_option("packages", install_pkgs, "Packages to install")->required();
    bool keep_cache = true;
    install_cmd->add_flag("--no-keep-cache", keep_cache, "Delete .lof after install");

    // local install
    std::string local_file;
    auto* il_cmd = app.add_subcommand("install-local", "Install a local .lof file");
    il_cmd->add_option("file", local_file, "Path to .lof file")->required();

    // reinstall
    std::string reinstall_pkg;
    auto* re_cmd = app.add_subcommand("reinstall", "Reinstall a package");
    re_cmd->add_option("package", reinstall_pkg, "Package name")->required();

    // remove
    std::string remove_pkg;
    auto* rm_cmd = app.add_subcommand("remove", "Remove a package");
    rm_cmd->add_option("package", remove_pkg, "Package name")->required();
    bool rm_show = false;
    rm_cmd->add_flag("-v,--show-files", rm_show, "Show files being removed");

    // upgrade
    auto* up_cmd = app.add_subcommand("upgrade", "Upgrade all installed packages");
    bool up_dry = false;
    up_cmd->add_flag("--dry-run", up_dry, "Show what would be upgraded");

    // view
    std::string view_pkg;
    auto* view_cmd = app.add_subcommand("view", "Show package information");
    view_cmd->add_option("package", view_pkg, "Package name")->required();

    // list
    auto* list_cmd = app.add_subcommand("list", "List installed packages");
    bool list_avail = false;
    list_cmd->add_flag("--available", list_avail, "List available packages instead");

    // clean
    auto* clean_cmd = app.add_subcommand("clean", "Clean package cache");

    // repo info
    auto* ec_cmd = app.add_subcommand("repo", "Show active repository");
    (void)ec_cmd;

    CLI11_PARSE(app, argc, argv);

    if (verbose) Logger::instance().set_level(LogLevel::Debug);

    if (!root.empty()) {
        Config::instance().set_root(root);
    }
    if (!arch_override.empty()) {
        Config::instance().set_arch(arch_override);
    }

    auto& cfg = Config::instance();

    InstallOptions opts;
    opts.assume_yes = assume_yes;
    opts.keep_cache = keep_cache;
    opts.verbose = verbose;

    auto base_url = Repository::load_active_url();

    if (*update_cmd) {
        UpdateOptions uopts;
        uopts.assume_yes = assume_yes;
        uopts.insecure   = update_ignore_gpg;
        uopts.verbose    = verbose;
        uopts.jobs       = update_jobs;
        auto r = cmd_update(uopts);
        if (!r) return handle_error(r.error());
        return 0;
    }

    if (*install_cmd) {
        if (!base_url) return handle_error(base_url.error());
        auto repo = Repository::load_index();
        if (!repo) return handle_error(repo.error());
        Database db{cfg.root()};
        Installer inst{db, *repo, cfg.packs_cache(), cfg.info_dir()};
        auto r = inst.install(install_pkgs, *base_url, opts);
        if (!r) return handle_error(r.error());
        return 0;
    }

    if (*il_cmd) {
        auto repo = Repository::load_index();
        Database db{cfg.root()};
        Installer inst{db,
                       repo ? *repo : Repository{},
                       cfg.packs_cache(),
                       cfg.info_dir()};
        auto r = inst.install_local(local_file, opts);
        if (!r) return handle_error(r.error());
        return 0;
    }

    if (*re_cmd) {
        if (!base_url) return handle_error(base_url.error());
        auto repo = Repository::load_index();
        if (!repo) return handle_error(repo.error());
        Database db{cfg.root()};
        Installer inst{db, *repo, cfg.packs_cache(), cfg.info_dir()};
        auto r = inst.reinstall(reinstall_pkg, *base_url, opts);
        if (!r) return handle_error(r.error());
        return 0;
    }

    if (*rm_cmd) {
        Database db{cfg.root()};
        Installer inst{db, Repository{}, cfg.packs_cache(), cfg.info_dir()};
        auto r = inst.remove(remove_pkg, opts, rm_show);
        if (!r) return handle_error(r.error());
        return 0;
    }

    if (*up_cmd) {
        UpgradeOptions uopts;
        uopts.assume_yes = assume_yes;
        uopts.dry_run    = up_dry;
        uopts.verbose    = verbose;
        auto r = cmd_upgrade(uopts);
        if (!r) return handle_error(r.error());
        return 0;
    }

    if (*view_cmd) {
        Database db{cfg.root()};
        auto pkg = db.read_info(view_pkg);
        if (!pkg) {
            auto repo = Repository::load_index();
            if (repo) {
                if (auto best = repo->find_best(view_pkg)) {
                    pkg = Manifest::from_file(cfg.info_dir() / (best->raw + ".info"));
                }
            }
        }
        if (!pkg) return handle_error(pkg.error());
        std::printf("%s", pkg->to_string().c_str());
        return 0;
    }

    if (*list_cmd) {
        if (list_avail) {
            auto repo = Repository::load_index();
            if (!repo) return handle_error(repo.error());
            for (const auto& p : repo->packages()) std::printf("%s\n", p.raw.c_str());
        } else {
            Database db{cfg.root()};
            auto list = db.list_installed();
            if (!list) return handle_error(list.error());
            for (const auto& n : *list) std::printf("%s\n", n.c_str());
        }
        return 0;
    }

    if (*clean_cmd) {
        Database db{cfg.root()};
        Installer inst{db, Repository{}, cfg.packs_cache(), cfg.info_dir()};
        auto r = inst.clean_cache();
        if (!r) return handle_error(r.error());
        return 0;
    }

    if (*ec_cmd) {
        if (auto u = cfg.active_repo()) {
            std::printf("%s\n", u->c_str());
            return 0;
        }
        log_error("no active repository");
        return 1;
    }

    // No subcommand: show help and exit 1.
    // (Explicit --help / --version is handled by CLI11 earlier and exits 0.)
    std::cerr << app.help() << '\n';
    return 1;
}
