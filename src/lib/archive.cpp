#include "lapm/archive.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/logger.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <vector>

namespace lapm {

namespace {

// Rejects entries whose normalized path is absolute or contains "..".
Result<void> validate_entry_path(std::string_view pathname,
                                 const fs::path& dest) {
    if (pathname.empty()) {
        return fail(Errc::CorruptPackage, "empty entry name");
    }
    fs::path p{std::string{pathname}};
    if (p.is_absolute()) {
        return fail(Errc::PathTraversal,
                    std::format("absolute path in archive: {}", pathname));
    }
    for (const auto& part : p) {
        if (part == "..") {
            return fail(Errc::PathTraversal,
                        std::format("parent traversal in archive: {}", pathname));
        }
    }

    auto target = fs::weakly_canonical(dest / p);
    auto base   = fs::weakly_canonical(dest);
    auto rel    = target.lexically_relative(base);
    if (rel.empty() || rel.native().starts_with("..")) {
        return fail(Errc::PathTraversal,
                    std::format("path escapes destination: {}", pathname));
    }
    return {};
}

bool is_scripts_entry(std::string_view pathname) {
    return pathname == "scripts" || pathname.starts_with("scripts/");
}

Result<std::vector<std::string>> do_extract(const fs::path& archive_path,
                                            const fs::path& dest,
                                            bool only_scripts) {
    if (auto r = ensure_dir(dest); !r) return std::unexpected(r.error());

    struct archive* a = archive_read_new();
    if (a == nullptr) {
        return fail(Errc::IoError, "archive_read_new failed");
    }
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);

    struct archive* disk = archive_write_disk_new();
    if (disk == nullptr) {
        archive_read_free(a);
        return fail(Errc::IoError, "archive_write_disk_new failed");
    }
    int flags = ARCHIVE_EXTRACT_TIME
              | ARCHIVE_EXTRACT_PERM
              | ARCHIVE_EXTRACT_ACL
              | ARCHIVE_EXTRACT_FFLAGS
              | ARCHIVE_EXTRACT_SECURE_NODOTDOT
              | ARCHIVE_EXTRACT_SECURE_SYMLINKS;
    archive_write_disk_set_options(disk, flags);
    archive_write_disk_set_standard_lookup(disk);

    auto cleanup = [&] {
        archive_read_free(a);
        archive_write_free(disk);
    };

    if (archive_read_open_filename(a, archive_path.c_str(), 10240) != ARCHIVE_OK) {
        auto msg = std::format("open {}: {}", archive_path.string(),
                               archive_error_string(a));
        cleanup();
        return fail(Errc::CorruptPackage, msg);
    }

    std::vector<std::string> extracted;
    struct archive_entry* entry = nullptr;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* pn = archive_entry_pathname(entry);
        if (pn == nullptr) continue;

        // IMPORTANT: copy the path NOW. libarchive may reuse or free the
        // buffer returned by archive_entry_pathname() as soon as we touch
        // the archive again (including archive_read_data_block or
        // archive_read_next_header). Holding a string_view here causes a
        // use-after-free.
        std::string name{pn};
        if (name.starts_with("./")) name.erase(0, 2);
        if (name.empty()) continue;

        bool scripts = is_scripts_entry(name);
        if ((only_scripts && !scripts) || (!only_scripts && scripts)) {
            archive_read_data_skip(a);
            continue;
        }

        if (auto r = validate_entry_path(name, dest); !r) {
            cleanup();
            return std::unexpected(r.error());
        }

        fs::path target = dest / fs::path{name};
        archive_entry_set_pathname(entry, target.c_str());

        auto mode = archive_entry_mode(entry);
        mode &= ~(S_ISUID | S_ISGID);
        archive_entry_set_mode(entry, mode);

        int r = archive_write_header(disk, entry);
        if (r != ARCHIVE_OK) {
            auto msg = std::format("write header {}: {}", name,
                                   archive_error_string(disk));
            cleanup();
            return fail(Errc::CorruptPackage, msg);
        }

        if (archive_entry_size(entry) > 0) {
            const void* buff = nullptr;
            size_t size = 0;
            la_int64_t offset = 0;
            while (true) {
                r = archive_read_data_block(a, &buff, &size, &offset);
                if (r == ARCHIVE_EOF) break;
                if (r < ARCHIVE_OK) {
                    auto msg = std::format("read data {}: {}", name,
                                           archive_error_string(a));
                    cleanup();
                    return fail(Errc::CorruptPackage, msg);
                }
                if (archive_write_data_block(disk, buff, size, offset) < ARCHIVE_OK) {
                    auto msg = std::format("write data {}: {}", name,
                                           archive_error_string(disk));
                    cleanup();
                    return fail(Errc::CorruptPackage, msg);
                }
            }
        }
        archive_write_finish_entry(disk);
        extracted.emplace_back(std::move(name));
    }

    cleanup();
    return extracted;
}

} // namespace

Result<std::vector<std::string>> safe_extract(const fs::path& archive,
                                              const fs::path& dest) {
    return do_extract(archive, dest, /*only_scripts=*/false);
}

Result<std::vector<std::string>> extract_scripts(const fs::path& archive,
                                                 const fs::path& dest) {
    return do_extract(archive, dest, /*only_scripts=*/true);
}

Result<void> create_package(const fs::path& source_dir,
                            const fs::path& output_file) {
    std::error_code ec;
    if (!fs::is_directory(source_dir, ec) || ec) {
        return fail(Errc::NotFound, source_dir.string());
    }

    if (auto r = ensure_dir(output_file.parent_path()); !r) return r;

    struct archive* a = archive_write_new();
    if (a == nullptr) {
        return fail(Errc::IoError, "archive_write_new failed");
    }
    archive_write_set_format_pax_restricted(a);
    archive_write_add_filter_gzip(a);

    if (archive_write_open_filename(a, output_file.c_str()) != ARCHIVE_OK) {
        auto msg = std::format("open {}: {}", output_file.string(),
                               archive_error_string(a));
        archive_write_free(a);
        return fail(Errc::IoError, msg);
    }

    time_t mtime = 0;
    if (const char* e = std::getenv("SOURCE_DATE_EPOCH"); e != nullptr) {
        mtime = static_cast<time_t>(std::strtoll(e, nullptr, 10));
    }

    for (const auto& entry : fs::recursive_directory_iterator(source_dir)) {
        if (!entry.is_regular_file() && !entry.is_symlink()) continue;

        auto rel = fs::relative(entry.path(), source_dir);
        std::string name = rel.generic_string();

        struct archive_entry* ae = archive_entry_new();
        if (ae == nullptr) {
            archive_write_free(a);
            return fail(Errc::IoError, "archive_entry_new failed");
        }

        archive_entry_set_pathname(ae, name.c_str());
        archive_entry_set_mtime(ae, mtime, 0);

        if (entry.is_symlink()) {
            auto target = fs::read_symlink(entry.path());
            auto target_str = target.string();
            archive_entry_set_filetype(ae, AE_IFLNK);
            archive_entry_set_perm(ae, 0777);
            archive_entry_set_symlink(ae, target_str.c_str());
        } else {
            archive_entry_set_filetype(ae, AE_IFREG);
            auto perms = static_cast<mode_t>(fs::status(entry.path()).permissions()) & 0777;
            archive_entry_set_perm(ae, perms);
            auto size = fs::file_size(entry.path());
            archive_entry_set_size(ae, static_cast<la_int64_t>(size));
        }

        if (archive_write_header(a, ae) != ARCHIVE_OK) {
            auto msg = std::format("write header {}: {}", name,
                                   archive_error_string(a));
            archive_entry_free(ae);
            archive_write_free(a);
            return fail(Errc::IoError, msg);
        }

        if (entry.is_regular_file()) {
            std::FILE* f = std::fopen(entry.path().c_str(), "rb");
            if (f == nullptr) {
                auto msg = std::format("fopen {}: {}", entry.path().string(),
                                       std::strerror(errno));
                archive_entry_free(ae);
                archive_write_free(a);
                return fail(Errc::IoError, msg);
            }
            char buf[8192];
            std::size_t n = 0;
            while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
                if (archive_write_data(a, buf, n) < 0) {
                    std::fclose(f);
                    archive_entry_free(ae);
                    archive_write_free(a);
                    return fail(Errc::IoError, "archive_write_data failed");
                }
            }
            std::fclose(f);
        }

        archive_entry_free(ae);
    }

    archive_write_close(a);
    archive_write_free(a);
    return {};
}

} // namespace lapm
