#include "lapm/archive.hpp"
#include "lapm/config.hpp"
#include "lapm/error.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/logger.hpp"
#include "lapm/package.hpp"
#include "lapm/version.hpp"

#include <CLI/CLI.hpp>

#include <cstdio>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>

namespace {

int handle_error(const lapm::Error& e) {
    lapm::log_error("{}", e.message());
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    using namespace lapm;

    CLI::App app{std::format("lapm-mk {} - Lofros package builder", kVersion)};
    app.set_version_flag("-v,--version", std::string{kVersion});

    std::string name;
    auto* create_cmd = app.add_subcommand("create", "Create a .lof package");
    create_cmd->add_option("name", name,
        "Package name in format <name>_<version>-<rev>_<arch>")->required();

    std::string dir = ".";
    create_cmd->add_option("-C,--dir", dir, "Source directory (default .)");

    auto* help_cmd = app.add_subcommand("help-packaging",
        "Show instructions for packaging");

    CLI11_PARSE(app, argc, argv);

    if (*help_cmd) {
        std::printf(
            "To build a package:\n"
            "  1. Compile the software into a fake root (e.g. ./usr/bin/foo)\n"
            "  2. cd into that directory\n"
            "  3. Run: lapm-mk create <name>_<version>-<rev>_<arch>\n"
            "\n"
            "Example: lapm-mk create nano_8.7-1_i686\n");
        return 0;
    }

    if (*create_cmd) {
        auto pkg = Package::parse(name);
        if (!pkg) return handle_error(pkg.error());

        auto source = std::filesystem::path{dir};
        std::error_code ec;
        if (!std::filesystem::is_directory(source, ec) || ec) {
            log_error("source directory does not exist: {}", dir);
            return 1;
        }

        if (!std::filesystem::is_directory(source / "usr", ec)) {
            log_warn("source does not contain a 'usr' directory");
            if (!confirm("Continue anyway", false)) {
                log_info("cancelled");
                return 1;
            }
        }

        auto out = source / ".." / pkg->filename();
        log_info("creating {} from {}", out.string(), source.string());
        auto r = create_package(source, out);
        if (!r) return handle_error(r.error());
        log_info("{} created", out.string());
        return 0;
    }

    std::cerr << app.help() << '\n';
    return 1;
}
