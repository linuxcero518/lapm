#include "lapm/version_cmp.hpp"

#include <algorithm>
#include <charconv>
#include <format>

namespace lapm {

namespace {

bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

} // namespace

Result<Version> Version::parse(std::string_view s) {
    if (s.empty()) {
        return fail(Errc::InvalidManifest, "empty version");
    }

    Version v;

    std::size_t i = 0;
    while (i < s.size() && is_digit(s[i])) {
        std::size_t j = i;
        while (j < s.size() && is_digit(s[j])) ++j;

        std::uint64_t n = 0;
        auto [ptr, ec] = std::from_chars(s.data() + i, s.data() + j, n);
        if (ec != std::errc{}) {
            return fail(Errc::InvalidManifest,
                        std::format("bad numeric version in '{}'", s));
        }
        v.numbers.push_back(n);
        i = j;

        if (i < s.size() && s[i] == '.') {
            ++i;
        } else if (i < s.size() && s[i] == '-') {
            // '-' starts the suffix ("1.0-rc1").
            v.suffix = std::string{s.substr(i + 1)};
            break;
        } else if (i < s.size() && !is_digit(s[i])) {
            // Letters glued to numbers without separator ("1.0rc1").
            v.suffix = std::string{s.substr(i)};
            break;
        }
    }

    if (v.numbers.empty()) {
        return fail(Errc::InvalidManifest,
                    std::format("no numeric component in '{}'", s));
    }
    return v;
}

std::strong_ordering Version::operator<=>(const Version& other) const {
    // Compare numeric parts, zero-padding the shorter one.
    const auto n = std::max(numbers.size(), other.numbers.size());
    for (std::size_t i = 0; i < n; ++i) {
        auto a = i < numbers.size() ? numbers[i] : 0;
        auto b = i < other.numbers.size() ? other.numbers[i] : 0;
        if (a < b) return std::strong_ordering::less;
        if (a > b) return std::strong_ordering::greater;
    }
    // Empty suffix (stable) outranks any suffix (pre-release).
    if (suffix.empty() && !other.suffix.empty()) return std::strong_ordering::greater;
    if (!suffix.empty() && other.suffix.empty()) return std::strong_ordering::less;
    if (suffix < other.suffix) return std::strong_ordering::less;
    if (suffix > other.suffix) return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}

bool Version::operator==(const Version& other) const {
    return (*this <=> other) == std::strong_ordering::equal;
}

std::string Version::str() const {
    std::string out;
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        if (i) out += '.';
        out += std::to_string(numbers[i]);
    }
    if (!suffix.empty()) {
        out += '-';
        out += suffix;
    }
    return out;
}

} // namespace lapm
