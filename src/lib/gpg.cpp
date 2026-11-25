#include "lapm/gpg.hpp"
#include "lapm/fs_utils.hpp"

namespace lapm {

Result<void> verify_signature(const fs::path& keyring,
                              const fs::path& sig,
                              const fs::path& data) {
    return verify_gpg(keyring, sig, data);
}

} // namespace lapm
