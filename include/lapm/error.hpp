#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace lapm {

// Error codes used across the whole program.
enum class Errc {
    Ok = 0,
    NotFound,
    PermissionDenied,
    IoError,
    NetworkError,
    GpgInvalid,
    CorruptPackage,
    PathTraversal,
    NotInstalled,
    AlreadyInstalled,
    UnsatisfiedDeps,
    InvalidManifest,
    InvalidPackageName,
    Cancelled,
    NotImplemented,
    LockBusy,
    EmptyFile,
    ExternalToolFailed,
};

[[nodiscard]] constexpr std::string_view to_string(Errc e) noexcept {
    switch (e) {
        case Errc::Ok:                  return "ok";
        case Errc::NotFound:            return "not found";
        case Errc::PermissionDenied:    return "permission denied";
        case Errc::IoError:             return "i/o error";
        case Errc::NetworkError:        return "network error";
        case Errc::GpgInvalid:          return "gpg signature invalid";
        case Errc::CorruptPackage:      return "corrupt package";
        case Errc::PathTraversal:       return "path traversal detected";
        case Errc::NotInstalled:        return "not installed";
        case Errc::AlreadyInstalled:    return "already installed";
        case Errc::UnsatisfiedDeps:     return "unsatisfied dependencies";
        case Errc::InvalidManifest:     return "invalid manifest";
        case Errc::InvalidPackageName:  return "invalid package name";
        case Errc::Cancelled:           return "cancelled by user";
        case Errc::NotImplemented:      return "not implemented";
        case Errc::LockBusy:            return "another instance is running";
        case Errc::EmptyFile:           return "empty file";
        case Errc::ExternalToolFailed:  return "external tool failed";
    }
    return "unknown error";
}

// Rich error carrying a code and an optional human-readable context
// (path, URL, package name, etc.).
struct Error {
    Errc code = Errc::Ok;
    std::string context;

    Error() = default;
    Error(Errc c, std::string ctx = {})
        : code(c), context(std::move(ctx)) {}

    [[nodiscard]] std::string message() const {
        std::string msg{to_string(code)};
        if (!context.empty()) {
            msg += ": ";
            msg += context;
        }
        return msg;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return code != Errc::Ok;
    }
};

template <class T>
using Result = std::expected<T, Error>;

template <class T>
[[nodiscard]] inline std::unexpected<Error> fail(Errc c, std::string ctx = {}) {
    return std::unexpected(Error{c, std::move(ctx)});
}

[[nodiscard]] inline std::unexpected<Error> fail(Errc c, std::string ctx = {}) {
    return std::unexpected(Error{c, std::move(ctx)});
}

} // namespace lapm
