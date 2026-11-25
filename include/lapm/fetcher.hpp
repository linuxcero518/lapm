#pragma once

#include "lapm/error.hpp"

#include <concepts>
#include <filesystem>
#include <string_view>

namespace lapm {

namespace fs = std::filesystem;

// A Fetcher knows how to download a URL to a local file.
// The concept allows swapping implementations in tests.
template <class F>
concept Fetcher = requires(F f, std::string_view url, const fs::path& out) {
    { f.download(url, out) } -> std::same_as<Result<void>>;
};

// libcurl-backed fetcher. Handles http and https.
class CurlFetcher {
public:
    CurlFetcher();
    ~CurlFetcher();

    CurlFetcher(const CurlFetcher&)            = delete;
    CurlFetcher& operator=(const CurlFetcher&) = delete;

    // Download url into out. Uses atomic_write semantics: the file is
    // written to a temp path and renamed on success.
    Result<void> download(std::string_view url, const fs::path& out);
};

} // namespace lapm
