#include "lapm/error.hpp"
#include "lapm/logger.hpp"

#include <format>
#include <string_view>

namespace lapm {

// Placeholder for commands not implemented in this phase.
Result<void> not_implemented(std::string_view cmd) {
    log_error("command '{}' not implemented yet", cmd);
    return fail(Errc::NotImplemented, std::string{cmd});
}

} // namespace lapm
