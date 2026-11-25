#include "lapm/installer.hpp"
#include "lapm/config.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/logger.hpp"

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <format>
#include <fstream>

extern char** environ;

namespace lapm {

namespace {

// Reject any .lof whose name doesn't match the architecture of the system.
Result<void> check_arch(const Package& pkg) {
    auto sys = Config::instance().architecture();
    if (pkg.arch != sys && pkg.arch != "all") {
        return fail(Errc::CorruptPackage,
                    std::format("package arch '{}' does not match system '{}'",
                                pkg.arch, sys));
    }
    return {};
}

// Walks the extracted tree collecting regular files and symlinks.
// Skips the scripts/ subtree.
std::vector<std::string> collect_payload(const fs::path& root) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(root, ec)) {
        if (ec) break;
        auto rel = fs::relative(e.path(), root);
        std::string rel_s = rel.generic_string();
        if (rel_s == "scripts" || rel_s.starts_with("scripts/")) continue;
        if (e.is_regular_file() || e.is_symlink()) {
            out.push_back("/" + rel_s);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

Result<void> spawn_script(const fs::path& script) {
    std::error_code ec;
    fs::permissions(script,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add, ec);
    if (ec) {
        return fail(Errc::IoError,
                    std::format("chmod +x {}: {}", script.string(), ec.message()));
    }

    std::string abs = fs::absolute(script).string();
    std::vector<std::string> argv = {"/bin/sh", "-c", abs};
    std::vector<char*> cargv;
    for (auto& s : argv) cargv.push_back(s.data());
    cargv.push_back(nullptr);

    pid_t pid = 0;
    int rc = ::posix_spawnp(&pid, "/bin/sh", nullptr, nullptr, cargv.data(), environ);
    if (rc != 0) {
        return fail(Errc::ExternalToolFailed,
                    std::format("spawn({}) failed", abs));
    }
    int status = 0;
    while (::waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            return fail(Errc::ExternalToolFailed, "waitpid failed");
        }
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return fail(Errc::ExternalToolFailed,
                    std::format("{} exited with status {}", abs, WEXITSTATUS(status)));
    }
    return {};
}

} // namespace

Installer::Installer(Database db,
                     Repository repo,
                     fs::path cache_dir,
                     fs::path info_dir)
    : db_{std::move(db)},
      repo_{std::move(repo)},
      cache_dir_{std::move(cache_dir)},
      info_dir_{std::move(info_dir)} {}

Result<void> Installer::download(const Package& pkg, std::string_view base_url) {
    fs::path dest = cache_dir_ / pkg.filename();
    std::error_code ec;
    if (fs::exists(dest, ec) && !ec) {
        log_info("{} already in cache, skipping download", pkg.raw);
        return {};
    }

    if (auto r = ensure_dir(cache_dir_); !r) return r;

    std::string url = std::format("{}packs/{}", base_url, pkg.filename().string());
    log_info("downloading {}", url);

    CurlFetcher fetcher;
    return fetcher.download(url, dest);
}

Result<void> Installer::install_local(const fs::path& package_file,
                                       const InstallOptions& opts) {
    auto pkg = Package::from_filename(package_file);
    if (!pkg) return std::unexpected(pkg.error());

    if (auto r = check_arch(*pkg); !r) return r;

    if (db_.is_installed(pkg->name) && !opts.reinstall) {
        return fail(Errc::AlreadyInstalled, pkg->name);
    }

    // Work dir under cache root.
    fs::path tmp = cache_dir_ / ".tmp" / pkg->raw;
    std::error_code ec;
    fs::remove_all(tmp, ec);
    if (auto r = ensure_dir(tmp); !r) return r;

    // RAII cleanup of tmp dir.
    struct Cleanup {
        fs::path p;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(p, ignored);
        }
    } cleanup{tmp};

    // Split extraction: payload in tmp/, scripts in tmp/scripts/.
    auto payload = safe_extract(package_file, tmp);
    if (!payload) return std::unexpected(payload.error());
    auto scripts = extract_scripts(package_file, tmp / "scripts");
    if (!scripts) return std::unexpected(scripts.error());

    if (payload->empty() && scripts->empty()) {
        return fail(Errc::CorruptPackage,
                    std::format("{} contains neither files nor scripts", pkg->raw));
    }

    // On reinstall, run old desinst first.
    if (opts.reinstall && db_.has_desinst(pkg->name) && !opts.skip_scripts) {
        auto desinst = db_.desinst_path(pkg->name);
        log_info("running previous desinst for {}", pkg->name);
        if (auto r = run_desinst(desinst); !r) return r;
        if (auto r = db_.remove_desinst(pkg->name); !r) {
            log_warn("cannot remove desinst for {}: {}", pkg->name, r.error().message());
        }
    }

    // Conflict check (non-reinstall only).
    if (!opts.reinstall) {
        auto existing = collect_payload(tmp);
        std::size_t conflicts = 0;
        for (const auto& rel : existing) {
            std::error_code e;
            if (fs::exists(rel, e) && !e) {
                log_warn("{} already exists on disk", rel);
                ++conflicts;
            }
        }
        if (conflicts > 0) {
            if (!confirm(std::format("{} conflicts found. Continue?", conflicts),
                         opts.assume_yes)) {
                return fail(Errc::Cancelled, "");
            }
        }
    }

    // Copy payload into /.
    std::vector<std::string> installed_files;
    if (auto r = copy_tree(tmp, "/", opts.reinstall, installed_files); !r) {
        return r;
    }

    // Persist tracking.
    if (opts.reinstall) {
        if (auto r = db_.remove_tracking(pkg->name); !r) {
            log_warn("cannot remove old tracking for {}: {}",
                     pkg->name, r.error().message());
        }
    }
    if (auto r = db_.write_tracking(pkg->name, installed_files); !r) return r;

    // Run postinst.
    if (!opts.skip_scripts) {
        fs::path post = tmp / "scripts" / "postinst";
        std::error_code e;
        if (fs::exists(post, e) && !e) {
            log_info("running postinst for {}", pkg->name);
            if (auto r = run_postinst(post); !r) {
                log_warn("postinst failed for {}: {}", pkg->name, r.error().message());
            }
        }
        fs::path desinst = tmp / "scripts" / "desinst";
        if (fs::exists(desinst, e) && !e) {
            fs::path dst = db_.desinst_path(pkg->name);
            if (auto r = ensure_dir(dst.parent_path()); !r) return r;
            std::error_code ignored;
            fs::copy_file(desinst, dst, fs::copy_options::overwrite_existing, ignored);
        }
    }

    // Update .info status.
    if (auto r = db_.set_status(pkg->name, "Instalado"); !r) {
        log_warn("cannot update status for {}: {}", pkg->name, r.error().message());
    }

    // Register reverse dependencies.
    auto manifest = Manifest::from_file(info_dir_ / (pkg->raw + ".info"));
    if (!manifest) manifest = Manifest::from_file(info_dir_ / (pkg->name + ".info"));
    if (manifest) {
        for (const auto& dep : manifest->dependencies) {
            if (auto r = db_.add_idep(dep, pkg->name); !r) {
                log_warn("cannot register idep {} <- {}: {}",
                         dep, pkg->name, r.error().message());
            }
        }
    }

    log_info("{} installed", pkg->name);
    return {};
}

Result<void> Installer::install(const std::vector<std::string>& requested,
                                 std::string_view base_url,
                                 const InstallOptions& opts) {
    Resolver resolver{repo_, db_, info_dir_};
    auto plan = resolver.resolve(requested, /*skip_installed=*/true);
    if (!plan) return std::unexpected(plan.error());

    if (!plan->already_installed.empty()) {
        std::string joined;
        for (const auto& n : plan->already_installed) {
            if (!joined.empty()) joined += ", ";
            joined += n;
        }
        log_info("already installed: {}", joined);
    }

    if (plan->install_order.empty()) {
        log_info("nothing to do");
        return {};
    }

    log_info("packages to install: {}", plan->install_order.size());
    for (const auto& p : plan->install_order) {
        log_info("  {}", p.raw);
    }
    log_info("total: {} MB download, {} MB disk",
             plan->download_mb, plan->disk_mb);

    if (!confirm("Continue", opts.assume_yes)) {
        return fail(Errc::Cancelled, "");
    }

    // Download phase.
    for (const auto& p : plan->install_order) {
        if (auto r = download(p, base_url); !r) return r;
    }

    // Install phase (order is topological).
    for (const auto& p : plan->install_order) {
        fs::path file = cache_dir_ / p.filename();
        auto r = install_local(file, opts);
        if (!r) {
            log_error("failed to install {}: {}", p.raw, r.error().message());
            return r;
        }
        if (!opts.keep_cache) {
            std::error_code ignored;
            fs::remove(file, ignored);
        }
    }

    return {};
}

Result<void> Installer::reinstall(const std::string& name,
                                   std::string_view base_url,
                                   const InstallOptions& opts) {
    if (!db_.is_installed(name)) {
        log_info("{} is not installed, installing instead", name);
        InstallOptions o = opts;
        o.reinstall = false;
        return install({name}, base_url, o);
    }

    auto pkg = repo_.find_best(name);
    if (!pkg) {
        return fail(Errc::NotFound,
                    std::format("package '{}' not found in repository", name));
    }

    if (!confirm(std::format("Reinstall {}", pkg->raw), opts.assume_yes)) {
        return fail(Errc::Cancelled, "");
    }

    if (auto r = download(*pkg, base_url); !r) return r;

    InstallOptions o = opts;
    o.reinstall = true;
    return install_local(cache_dir_ / pkg->filename(), o);
}

Result<void> Installer::remove(const std::string& name,
                                const InstallOptions& opts,
                                bool show_files) {
    if (!db_.is_installed(name)) {
        return fail(Errc::NotInstalled, name);
    }

    // Warn about reverse deps.
    auto ideps = db_.read_ideps(name);
    if (ideps && !ideps->empty()) {
        log_warn("{} is required by:", name);
        for (const auto& d : *ideps) log_warn("  {}", d);
    }

    if (!confirm(std::format("Remove {}", name), opts.assume_yes)) {
        return fail(Errc::Cancelled, "");
    }

    auto files = db_.read_tracking(name);
    if (!files) return std::unexpected(files.error());

    if (show_files) {
        for (const auto& f : *files) std::printf("%s\n", f.c_str());
    }

    // Delete files.
    for (const auto& f : *files) {
        std::error_code ec;
        if (!fs::exists(f, ec)) continue;
        if (fs::is_regular_file(f, ec) || fs::is_symlink(f, ec)) {
            fs::remove(f, ec);
            if (ec) log_warn("cannot remove {}: {}", f, ec.message());
        }
    }

    // Remove empty directories, deepest first.
    std::vector<fs::path> dirs;
    for (const auto& f : *files) {
        auto parent = fs::path{f}.parent_path();
        while (!parent.empty() && parent != "/") {
            dirs.push_back(parent);
            parent = parent.parent_path();
        }
    }
    std::sort(dirs.begin(), dirs.end(),
              [](const fs::path& a, const fs::path& b) {
                  return a.native().size() > b.native().size();
              });
    dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());
    for (const auto& d : dirs) {
        std::error_code ec;
        if (fs::is_directory(d, ec) && fs::is_empty(d, ec)) {
            fs::remove(d, ec);
        }
    }

    // Run desinst.
    if (!opts.skip_scripts && db_.has_desinst(name)) {
        auto d = db_.desinst_path(name);
        if (auto r = run_desinst(d); !r) {
            log_warn("desinst failed for {}: {}", name, r.error().message());
        }
        if (auto r = db_.remove_desinst(name); !r) {
            log_warn("cannot remove desinst for {}: {}", name, r.error().message());
        }
    }

    if (auto r = db_.remove_ideps(name); !r) {
        log_warn("cannot remove ideps for {}: {}", name, r.error().message());
    }
    if (auto r = db_.remove_tracking(name); !r) {
        log_warn("cannot remove tracking for {}: {}", name, r.error().message());
    }
    if (auto r = db_.set_status(name, "No instalado"); !r) {
        log_warn("cannot update status for {}: {}", name, r.error().message());
    }
    log_info("{} removed", name);
    return {};
}

Result<void> Installer::clean_cache() {
    std::error_code ec;
    if (!fs::is_directory(cache_dir_, ec) || ec) {
        log_info("cache {} does not exist", cache_dir_.string());
        return {};
    }
    for (const auto& e : fs::directory_iterator(cache_dir_, ec)) {
        if (ec) break;
        if (e.is_regular_file() && e.path().extension() == ".lof") {
            fs::remove(e.path(), ec);
        }
    }
    log_info("cache cleaned");
    return {};
}

Result<void> Installer::run_postinst(const fs::path& script) {
    return spawn_script(script);
}

Result<void> Installer::run_desinst(const fs::path& script) {
    return spawn_script(script);
}

Result<void> Installer::copy_tree(const fs::path& src,
                                   const fs::path& dst,
                                   bool overwrite,
                                   std::vector<std::string>& installed_files) {
    std::error_code ec;
    for (const auto& e : fs::recursive_directory_iterator(src, ec)) {
        if (ec) break;
        auto rel = fs::relative(e.path(), src);
        std::string rel_s = rel.generic_string();
        if (rel_s == "scripts" || rel_s.starts_with("scripts/")) continue;

        fs::path target = dst / rel;
        if (e.is_directory()) {
            if (auto r = ensure_dir(target); !r) return r;
        } else if (e.is_symlink()) {
            if (overwrite) fs::remove(target, ec);
            if (auto r = ensure_dir(target.parent_path()); !r) return r;
            auto link_target = fs::read_symlink(e.path(), ec);
            if (ec) {
                return fail(Errc::IoError,
                            std::format("readlink {}: {}", e.path().string(), ec.message()));
            }
            fs::create_symlink(link_target, target, ec);
            if (ec) {
                return fail(Errc::IoError,
                            std::format("symlink {}: {}", target.string(), ec.message()));
            }
            installed_files.push_back("/" + rel_s);
        } else if (e.is_regular_file()) {
            if (!overwrite) {
                std::error_code e2;
                if (fs::exists(target, e2) && !e2) {
                    log_warn("skipping existing {}", target.string());
                    continue;
                }
            }
            if (auto r = ensure_dir(target.parent_path()); !r) return r;
            fs::copy_file(e.path(), target,
                          fs::copy_options::overwrite_existing, ec);
            if (ec) {
                return fail(Errc::IoError,
                            std::format("copy {} -> {}: {}",
                                        e.path().string(), target.string(), ec.message()));
            }
            installed_files.push_back("/" + rel_s);
        }
    }
    return {};
}

} // namespace lapm
