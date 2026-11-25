#include "lapm/commands/update.hpp"

#include "lapm/config.hpp"
#include "lapm/fetcher.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/gpg.hpp"
#include "lapm/logger.hpp"
#include "lapm/package.hpp"
#include "lapm/paths.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace lapm {

namespace {

// Small bounded thread pool: N workers pulling package names from a shared
// queue. Stops early if any worker hits a hard error.
class DownloadPool {
public:
    DownloadPool(unsigned jobs,
                 fs::path info_dir,
                 std::string base_url,
                 bool& failed)
        : jobs_{std::max(1u, jobs)},
          info_dir_{std::move(info_dir)},
          base_url_{std::move(base_url)},
          failed_{failed} {}

    void enqueue(std::string pkg_raw) {
        std::lock_guard lk{mu_};
        queue_.push_back(std::move(pkg_raw));
    }

    void run() {
        std::vector<std::jthread> workers;
        workers.reserve(jobs_);
        for (unsigned i = 0; i < jobs_; ++i) {
            workers.emplace_back([this](std::stop_token st) {
                worker_loop(st);
            });
        }
        // jthreads join on destruction.
    }

private:
    void worker_loop(std::stop_token st) {
        CurlFetcher fetcher;
        while (!st.stop_requested() && !failed_) {
            std::string job;
            {
                std::lock_guard lk{mu_};
                if (queue_.empty()) return;
                job = std::move(queue_.back());
                queue_.pop_back();
            }

            fs::path out = info_dir_ / (job + ".info");
            std::string url = std::format("{}info/{}.info", base_url_, job);
            auto r = fetcher.download(url, out);
            if (!r) {
                std::lock_guard lk{mu_};
                log_error("failed {}: {}", job, r.error().message());
                failed_ = true;
                return;
            }
            std::lock_guard lk{mu_};
            log_info("  {}", job);
        }
    }

    unsigned jobs_;
    fs::path info_dir_;
    std::string base_url_;
    bool& failed_;
    std::mutex mu_;
    std::vector<std::string> queue_;
};

Result<std::vector<std::string>> read_index_names(const fs::path& index) {
    std::ifstream in(index);
    if (!in) {
        return fail(Errc::NotFound, index.string());
    }
    std::vector<std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        out.push_back(std::move(line));
    }
    return out;
}

std::size_t prune_info_files(const fs::path& dir,
                             const std::set<std::string>& keep) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec) || ec) return 0;

    std::size_t removed = 0;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".info") continue;
        if (keep.contains(e.path().stem().string())) continue;
        fs::remove(e.path(), ec);
        if (!ec) {
            log_debug("pruned {}", e.path().string());
            ++removed;
        }
    }
    return removed;
}

} // namespace

Result<void> cmd_update(const UpdateOptions& opts) {
    auto& cfg = Config::instance();

    auto url = cfg.active_repo();
    if (!url) {
        return fail(Errc::NotFound,
                    std::format("no repositories in {}", cfg.repository_list().string()));
    }
    log_info("repository: {}", *url);

    auto arch   = cfg.architecture();
    auto distro = cfg.distro();
    log_info("target architecture: {} (override with --arch or LAPM_ARCH)", arch);

    auto base = dist_url(*url, distro, arch);
    log_info("dist URL: {}", base);

    fs::path tmp_list = cfg.repository_dir() / "lofpack.list.download";
    fs::path tmp_sig  = cfg.repository_dir() / "lofpack.list.gpg.download";

    {
        std::error_code ec;
        fs::remove(tmp_list, ec);
        fs::remove(tmp_sig, ec);
    }

    CurlFetcher fetcher;

    log_info("downloading lofpack.list");
    if (auto r = fetcher.download(base + "lofpack.list", tmp_list); !r) {
        return r;
    }

    if (!opts.insecure) {
        log_info("downloading lofpack.list.gpg");
        if (auto r = fetcher.download(base + "lofpack.list.gpg", tmp_sig); !r) {
            std::error_code ec;
            fs::remove(tmp_list, ec);
            return r;
        }

        if (!fs::is_regular_file(cfg.keyring())) {
            std::error_code ec;
            fs::remove(tmp_list, ec);
            fs::remove(tmp_sig, ec);
            return fail(Errc::NotFound,
                        std::format("keyring not found: {}", cfg.keyring().string()));
        }

        log_info("verifying GPG signature");
        if (auto r = verify_signature(cfg.keyring(), tmp_sig, tmp_list); !r) {
            std::error_code ec;
            fs::remove(tmp_list, ec);
            fs::remove(tmp_sig, ec);
            return r;
        }
        log_info("GPG signature OK");
    } else {
        log_warn("GPG verification skipped (--insecure)");
    }

    std::error_code ec;
    if (fs::file_size(tmp_list, ec) == 0 || ec) {
        fs::remove(tmp_list, ec);
        fs::remove(tmp_sig, ec);
        return fail(Errc::EmptyFile, tmp_list.string());
    }

    auto names = read_index_names(tmp_list);
    if (!names) {
        fs::remove(tmp_list, ec);
        fs::remove(tmp_sig, ec);
        return std::unexpected(names.error());
    }
    std::vector<std::string> valid_names;
    valid_names.reserve(names->size());
    for (const auto& n : *names) {
        auto p = Package::parse(n);
        if (!p) {
            log_warn("invalid entry in index, ignoring: {}", n);
            continue;
        }
        valid_names.push_back(n);
    }
    if (valid_names.empty()) {
        fs::remove(tmp_list, ec);
        fs::remove(tmp_sig, ec);
        return fail(Errc::EmptyFile,
                    "index has no valid packages after validation");
    }

    {
        std::ifstream in(tmp_list);
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        in.close();
        if (auto r = atomic_write(cfg.lofpack_list(), content); !r) {
            fs::remove(tmp_list, ec);
            fs::remove(tmp_sig, ec);
            return r;
        }
    }
    fs::remove(tmp_list, ec);
    fs::remove(tmp_sig, ec);

    log_info("index updated: {} packages", valid_names.size());

    if (auto r = ensure_dir(cfg.info_dir()); !r) return r;

    std::vector<std::string> to_download;
    for (const auto& name : valid_names) {
        fs::path info = cfg.info_dir() / (name + ".info");
        if (!fs::is_regular_file(info, ec)) {
            to_download.push_back(name);
        }
    }

    if (to_download.empty()) {
        log_info("all .info files already up to date");
    } else {
        log_info("downloading {} .info files ({} workers)",
                 to_download.size(), opts.jobs);
        bool failed = false;
        DownloadPool pool{opts.jobs, cfg.info_dir(), base, failed};
        for (auto& n : to_download) {
            pool.enqueue(std::move(n));
        }
        pool.run();
        if (failed) {
            return fail(Errc::NetworkError,
                        "some .info downloads failed; rerun update");
        }
    }

    std::set<std::string> keep{valid_names.begin(), valid_names.end()};
    auto pruned = prune_info_files(cfg.info_dir(), keep);
    if (pruned > 0) {
        log_info("pruned {} orphan .info files", pruned);
    }

    log_info("update complete: {} packages available", valid_names.size());
    return {};
}

} // namespace lapm
