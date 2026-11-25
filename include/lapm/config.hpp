#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace lapm {

namespace fs = std::filesystem;

class Config {
public:
    static Config& instance();

    void set_root(fs::path p) {
        root_ = std::move(p);
        root_detected_ = true;
    }
    [[nodiscard]] const fs::path& root() const;

    void set_arch(std::string a) {
        arch_ = std::move(a);
        arch_detected_ = true;
    }
    [[nodiscard]] const std::string& architecture() const;

    [[nodiscard]] fs::path repository_dir() const;
    [[nodiscard]] fs::path repository_list() const;
    [[nodiscard]] fs::path lofpack_list() const;
    [[nodiscard]] fs::path trusted_dir() const;
    [[nodiscard]] fs::path keyring() const;
    [[nodiscard]] fs::path info_dir() const;
    [[nodiscard]] fs::path tracking_dir() const;
    [[nodiscard]] fs::path ideps_dir() const;
    [[nodiscard]] fs::path desinst_dir() const;
    [[nodiscard]] fs::path packs_cache() const;
    [[nodiscard]] fs::path extrepos_dir() const;

    [[nodiscard]] std::optional<std::string> active_repo() const;
    [[nodiscard]] std::string distro() const;

private:
    Config() = default;
    void detect_root();
    void detect_arch();

    fs::path root_;
    bool root_detected_ = false;

    std::string arch_;
    bool arch_detected_ = false;
};

} // namespace lapm
