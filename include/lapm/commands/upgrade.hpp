#pragma once

#include "lapm/error.hpp"

namespace lapm {

struct UpgradeOptions {
    bool assume_yes = false;
    bool dry_run    = false;
    bool verbose    = false;
};

// Upgrade pipeline:
//   1. Iterate every installed package (tracking/*.tracking).
//   2. For each one, find the best version in the repository.
//   3. Compare with the installed version using Version::operator<=>.
//   4. If newer, add it to the plan (unless --dry-run).
//   5. Run the resolver over all candidates so new deps are pulled in.
//   6. Install each upgraded package with reinstall semantics.
Result<void> cmd_upgrade(const UpgradeOptions& opts);

} // namespace lapm
