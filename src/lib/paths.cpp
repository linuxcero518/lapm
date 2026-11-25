#include "lapm/paths.hpp"

namespace lapm {

namespace {

std::string ensure_trailing_slash(std::string_view s) {
    std::string out{s};
    if (out.empty() || out.back() != '/') out.push_back('/');
    return out;
}

} // namespace

std::string dist_url(std::string_view base_url,
                     std::string_view distro,
                     std::string_view arch) {
    std::string url = ensure_trailing_slash(base_url);
    url += "dists/";
    url += distro;
    url += '/';
    url += arch;
    url += '/';
    return url;
}

} // namespace lapm
