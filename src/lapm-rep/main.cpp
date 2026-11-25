#include "lapm/archive.hpp"
#include "lapm/config.hpp"
#include "lapm/error.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/logger.hpp"
#include "lapm/manifest.hpp"
#include "lapm/package.hpp"
#include "lapm/version.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <cstdio>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace {

int handle_error(const lapm::Error& e) {
    lapm::log_error("{}", e.message());
    return 1;
}

std::string human_size(std::uintmax_t bytes) {
    if (bytes == 0) return "0 KB";
    if (bytes < 1024ULL * 1024ULL) {
        auto kb = static_cast<double>(bytes) / 1024.0;
        return std::format("{:.1f} KB", kb);
    }
    if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        auto mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        return std::format("{:.1f} MB", mb);
    }
    auto gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return std::format("{:.1f} GB", gb);
}

struct InfoFlags {
    bool assume_yes = false;
    bool skip_existing = false;
};

lapm::Result<std::string> generate_info(const std::filesystem::path& lof_file,
                                        const std::filesystem::path& dist_path,
                                        const InfoFlags& flags) {
    using namespace lapm;

    auto pkg = Package::from_filename(lof_file);
    if (!pkg) {
        return fail(Errc::InvalidPackageName,
                    std::format("{}: {}", lof_file.filename().string(),
                                pkg.error().message()));
    }

    auto out = dist_path / "info" / (pkg->raw + ".info");

    std::error_code ec;
    if (std::filesystem::is_regular_file(out, ec) && !ec) {
        if (flags.skip_existing) {
            return std::string{"skipped"};
        }
        if (!flags.assume_yes) {
            std::printf("El archivo de info '%s' ya existe. ", out.string().c_str());
            std::fflush(stdout);
            if (!confirm("Desea sobreescribirlo", false)) {
                return std::string{"skipped"};
            }
        }
    }

    auto file_size = std::filesystem::file_size(lof_file, ec);
    if (ec) {
        return fail(Errc::IoError,
                    std::format("file_size {}: {}", lof_file.string(), ec.message()));
    }

    auto temp = std::filesystem::temp_directory_path() / pkg->raw;
    std::filesystem::remove_all(temp, ec);
    auto extracted = safe_extract(lof_file, temp);
    if (!extracted) {
        std::filesystem::remove_all(temp, ec);
        return std::unexpected(extracted.error());
    }

    std::uintmax_t disk_bytes = 0;
    for (const auto& rel : *extracted) {
        auto p = temp / rel;
        if (std::filesystem::is_regular_file(p, ec)) {
            disk_bytes += std::filesystem::file_size(p, ec);
        }
    }
    std::filesystem::remove_all(temp, ec);

    Manifest m;
    m.name          = pkg->name;
    m.version       = pkg->version.str();
    m.revision      = pkg->revision;
    m.status        = "No instalado";
    m.maintainer    = "linuxcero518@gmail.com";
    m.download_size = human_size(file_size);
    m.disk_size     = human_size(disk_bytes);

    if (auto r = ensure_dir(dist_path / "info"); !r) {
        return std::unexpected(r.error());
    }
    if (auto r = atomic_write(out, m.to_string()); !r) {
        return std::unexpected(r.error());
    }

    return std::format("wrote {}", out.filename().string());
}

} // namespace

int main(int argc, char** argv) {
    using namespace lapm;

    CLI::App app{std::format("lapm-rep {} - Lofros repository tool", kVersion)};
    app.set_version_flag("-v,--version", std::string{kVersion});

    std::string base = ".";
    app.add_option("-b,--base", base, "Repository root (default .)");

    std::string distro = std::string{kDefaultDistro};
    app.add_option("-d,--distro", distro, "Distribution name");

    std::string arch = Config::instance().architecture();
    app.add_option("-a,--arch", arch, "Architecture (default: <root>/arch or uname -m)");

    auto* gen_cmd = app.add_subcommand("gen", "Generate repository directory structure");
    auto* list_cmd = app.add_subcommand("list", "Generate lofpack.list");

    InfoFlags info_flags;
    std::string info_file;
    auto* info_cmd = app.add_subcommand("info", "Generate .info for one package");
    info_cmd->add_option("file", info_file, "Path to .lof file");
    info_cmd->add_flag("-y,--yes", info_flags.assume_yes,
                       "Overwrite existing .info without asking");
    info_cmd->add_flag("--skip-existing", info_flags.skip_existing,
                       "Never overwrite existing .info");

    std::string info_all_dir;
    auto* info_all_cmd = app.add_subcommand("info-all",
                                            "Generate .info for all packages in a directory");
    info_all_cmd->add_option("dir", info_all_dir, "Directory containing .lof files");
    info_all_cmd->add_flag("-y,--yes", info_flags.assume_yes,
                           "Overwrite existing .info without asking");
    info_all_cmd->add_flag("--skip-existing", info_flags.skip_existing,
                           "Never overwrite existing .info");

    CLI11_PARSE(app, argc, argv);

    std::filesystem::path base_p{base};
    std::filesystem::path dist_path = base_p / "dists" / distro / arch;

    if (*gen_cmd) {
        for (auto& p : {
                base_p / "packs",
                base_p / "dists" / distro / "dists_files",
                dist_path / "info",
            }) {
            if (auto r = ensure_dir(p); !r) return handle_error(r.error());
            std::printf("created %s\n", p.string().c_str());
        }
        return 0;
    }

    if (*list_cmd) {
        std::error_code ec;
        if (!std::filesystem::is_directory(base_p / "packs", ec)) {
            log_error("{} does not exist", (base_p / "packs").string());
            return 1;
        }
        if (auto r = ensure_dir(dist_path); !r) return handle_error(r.error());

        std::vector<std::string> names;
        for (const auto& e : std::filesystem::directory_iterator(base_p / "packs", ec)) {
            if (ec) break;
            if (!e.is_regular_file()) continue;
            if (e.path().extension() != ".lof") continue;
            names.push_back(e.path().stem().string());
        }
        std::sort(names.begin(), names.end());

        std::string content;
        for (const auto& n : names) {
            content += n;
            content += '\n';
        }
        auto out = dist_path / "lofpack.list";
        if (auto r = atomic_write(out, content); !r) return handle_error(r.error());
        std::printf("wrote %s (%zu packages)\n", out.string().c_str(), names.size());
        return 0;
    }

    if (*info_cmd) {
        auto r = generate_info(info_file, dist_path, info_flags);
        if (!r) return handle_error(r.error());
        std::printf("%s\n", r->c_str());
        return 0;
    }

    if (*info_all_cmd) {
        std::error_code ec;
        if (!std::filesystem::is_directory(info_all_dir, ec)) {
            return handle_error(Error{Errc::NotFound, info_all_dir});
        }

        std::vector<std::filesystem::path> files;
        for (const auto& e : std::filesystem::directory_iterator(info_all_dir, ec)) {
            if (ec) break;
            if (!e.is_regular_file()) continue;
            if (e.path().extension() != ".lof") continue;
            files.push_back(e.path());
        }
        std::sort(files.begin(), files.end());

        if (files.empty()) {
            std::printf("no .lof files found in %s\n", info_all_dir.c_str());
            return 0;
        }

        std::size_t created = 0;
        std::size_t skipped = 0;
        std::size_t failed  = 0;

        for (const auto& f : files) {
            auto r = generate_info(f, dist_path, info_flags);
            if (!r) {
                log_error("{}: {}", f.filename().string(), r.error().message());
                ++failed;
                continue;
            }
            if (*r == "skipped") {
                std::printf("%s: skipped (already exists)\n",
                            f.filename().string().c_str());
                ++skipped;
            } else {
                std::printf("%s\n", r->c_str());
                ++created;
            }
        }

        std::printf("\ninfo-all: %zu written, %zu skipped, %zu failed\n",
                    created, skipped, failed);
        return failed == 0 ? 0 : 1;
    }

    std::cerr << app.help() << '\n';
    return 1;
}
