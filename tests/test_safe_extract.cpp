#include <catch2/catch_test_macros.hpp>

#include "lapm/archive.hpp"
#include "lapm/fs_utils.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <filesystem>

using namespace lapm;
namespace fs = std::filesystem;

namespace {

void write_tar_with_entry(const fs::path& path,
                          const std::string& entry_name,
                          const std::string& content) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());

    struct archive_entry* e = archive_entry_new();
    archive_entry_set_pathname(e, entry_name.c_str());
    archive_entry_set_size(e, static_cast<la_int64_t>(content.size()));
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_perm(e, 0644);
    archive_write_header(a, e);
    archive_write_data(a, content.data(), content.size());
    archive_entry_free(e);
    archive_write_close(a);
    archive_write_free(a);
}

} // namespace

TEST_CASE("safe_extract rejects ../", "[archive]") {
    auto tmp = fs::temp_directory_path() / "lapm_test_pt";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    auto archive = tmp / "evil.lof";
    write_tar_with_entry(archive, "../etc/passwd", "owned");

    auto dest = tmp / "out";
    auto r = safe_extract(archive, dest);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code == Errc::PathTraversal);

    fs::remove_all(tmp);
}

TEST_CASE("safe_extract extracts benign", "[archive]") {
    auto tmp = fs::temp_directory_path() / "lapm_test_ok";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    auto archive = tmp / "ok.lof";
    write_tar_with_entry(archive, "usr/bin/hello", "hi");

    auto dest = tmp / "out";
    auto r = safe_extract(archive, dest);
    REQUIRE(r.has_value());
    REQUIRE(fs::exists(dest / "usr/bin/hello"));

    fs::remove_all(tmp);
}
