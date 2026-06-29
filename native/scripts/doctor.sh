#!/usr/bin/env bash
# Checkout-readiness preflight (distinct from build-host readiness). Reports
# whether this host can FETCH and SYNC the pinned Chromium checkout. Read-only;
# never installs, upgrades, or uses sudo.
#
# This does NOT gate building. Build readiness (hard RAM/disk/Xcode requirements)
# is enforced separately by build-host-check.sh, which gen.sh and build.sh require.
# Exit nonzero only if a checkout-blocking prerequisite is missing.
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

fail=0
ok()   { printf 'PASS  %s\n' "$*"; }
bad()  { printf 'FAIL  %s\n' "$*"; fail=1; }
note() { printf 'WARN  %s\n' "$*"; }

stage "Seoul checkout-readiness preflight"
log "checkout root: $SEOUL_CHROMIUM_ROOT_ABS"

# Architecture / OS (hard for this project)
if [ "$(uname -m)" = "arm64" ]; then ok "architecture is arm64"; else bad "architecture is $(uname -m), expected arm64"; fi
if command -v sw_vers >/dev/null 2>&1; then ok "macOS $(sw_vers -productVersion) ($(sw_vers -buildVersion))"; else bad "sw_vers not available (not macOS?)"; fi

# Checkout path safety (hard)
case "$SEOUL_CHROMIUM_ROOT_ABS" in *" "*) bad "checkout path contains a space";; *) ok "checkout path has no spaces";; esac
case "$SEOUL_CHROMIUM_ROOT_ABS/" in "$SEOUL_REPO_ROOT"/*) bad "checkout root is inside the repo";; *) ok "checkout root is outside the Project Seoul repo";; esac

# Filesystem of the checkout volume should be APFS. diskutil needs a device, not an
# arbitrary directory, so resolve via df first; guard so a probe failure never
# aborts this diagnostic.
probe="$SEOUL_CHROMIUM_ROOT_ABS"; while [ ! -e "$probe" ] && [ "$probe" != "/" ]; do probe="$(dirname "$probe")"; done
dev="$(df "$probe" 2>/dev/null | awk 'NR==2 {print $1}' || true)"
fs="$(diskutil info "$dev" 2>/dev/null | awk -F': *' '/File System Personality/ {print $2}' || true)"
case "$fs" in *APFS*) ok "checkout volume is APFS ($fs)";; "") note "could not determine filesystem of $probe";; *) bad "checkout volume is $fs, not APFS";; esac

# Tooling needed to fetch/sync (hard)
for c in git python3 curl; do command -v "$c" >/dev/null 2>&1 && ok "$c present ($($c --version 2>&1 | head -1))" || bad "$c not found"; done

# Disk: enough to FETCH the checkout (~50 GiB no-history + deps). The much larger
# build requirement is a build-host concern, reported here only as a heads-up.
free_gib="$(free_gib_for "$SEOUL_CHROMIUM_ROOT_ABS")"
if   [ "$free_gib" -lt 60 ]; then bad  "free space ${free_gib} GiB is insufficient to fetch the checkout (~50-60 GiB)"
else ok "free space ${free_gib} GiB (enough for the checkout)"; fi
[ "$free_gib" -lt "$SEOUL_MIN_BUILD_FREE_GIB" ] && note "free space ${free_gib} GiB is below the ${SEOUL_MIN_BUILD_FREE_GIB} GiB a build needs (build elsewhere; see build-host-check.sh)"

# RAM and Xcode are NOT required to fetch/sync; they are build-host requirements.
ram="$(mem_gib)"
[ "$ram" -ge "$SEOUL_MIN_RAM_GIB" ] && ok "RAM ${ram} GiB" \
  || note "RAM ${ram} GiB is below the ${SEOUL_MIN_RAM_GIB} GiB BUILD minimum (fine for checkout; build elsewhere)"
if xcodebuild -version >/dev/null 2>&1; then ok "full Xcode active ($(xcodebuild -version 2>/dev/null | head -1))"
else note "full Xcode not active (not required to fetch/sync; required by build-host-check.sh to build)"; fi

# Presence (informational)
[ -d "$DEPOT_TOOLS_DIR" ] && log "depot_tools present at $DEPOT_TOOLS_DIR" || log "depot_tools not yet cloned (fetch.sh will clone it)"
[ -d "$CHROMIUM_SRC" ]    && log "chromium src present at $CHROMIUM_SRC"   || log "chromium src not yet fetched (fetch.sh will fetch it)"

stage "result"
if [ "$fail" -ne 0 ]; then die "a checkout-blocking prerequisite is missing"; fi
log "checkout-ready. For BUILD readiness run: native/scripts/build-host-check.sh"
