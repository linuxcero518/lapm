#include "lapm/fetcher.hpp"
#include "lapm/fs_utils.hpp"
#include "lapm/logger.hpp"

#include <curl/curl.h>

#include <cstdio>
#include <format>
#include <mutex>

namespace lapm {

namespace {

// libcurl global init is process-wide and must run exactly once.
std::once_flag g_curl_init;

void ensure_curl_initialized() {
    std::call_once(g_curl_init, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* f = static_cast<std::FILE*>(userdata);
    return std::fwrite(ptr, size, nmemb, f);
}

} // namespace

CurlFetcher::CurlFetcher() {
    ensure_curl_initialized();
}

CurlFetcher::~CurlFetcher() = default;

Result<void> CurlFetcher::download(std::string_view url, const fs::path& out) {
    if (auto r = ensure_dir(out.parent_path()); !r) return r;

    fs::path tmp = out;
    tmp += ".part";

    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        return fail(Errc::IoError,
                    std::format("fopen {} for writing", tmp.string()));
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        std::fclose(f);
        return fail(Errc::IoError, "curl_easy_init failed");
    }

    std::string url_str{url};

    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "lapm/0.1");

    CURLcode rc = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    int close_rc = std::fclose(f);
    if (close_rc != 0) {
        std::error_code ignored;
        fs::remove(tmp, ignored);
        return fail(Errc::IoError,
                    std::format("fclose {} failed", tmp.string()));
    }

    if (rc != CURLE_OK) {
        std::error_code ignored;
        fs::remove(tmp, ignored);
        return fail(Errc::NetworkError,
                    std::format("{}: {} (HTTP {})", url_str,
                                curl_easy_strerror(rc), http_code));
    }

    std::error_code ec;
    fs::rename(tmp, out, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(tmp, ignored);
        return fail(Errc::IoError,
                    std::format("rename {} -> {}: {}",
                                tmp.string(), out.string(), ec.message()));
    }
    return {};
}

static_assert(Fetcher<CurlFetcher>);

} // namespace lapm
