#!/usr/bin/env bash
# Build-host readiness gate (distinct from checkout readiness). HARD-fails unless
# this host can safely run a full Chromium build. gen.sh and build.sh require this
# to pass first, so a build cannot accidentally start on an underpowered host such
# as an 8 GiB Mac. RAM is a hard requirement here, never a warning.
#
# Configurable minimums (env): SEOUL_MIN_RAM_GIB (default 16),
# SEOUL_MIN_BUILD_FREE_GIB (default 150).
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

fail=0
pass() { printf 'PASS  %s\n' "$*"; }
bad()  { printf 'FAIL  %s\n' "$*"; fail=1; }

stage "Seoul build-host readiness gate"
log "minimums: RAM >= ${SEOUL_MIN_RAM_GIB} GiB, free fast storage >= ${SEOUL_MIN_BUILD_FREE_GIB} GiB"

# Platform (HARD)
if [ "$(uname -s)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ]; then
  pass "host is macOS arm64"
else
  bad "host is $(uname -s)/$(uname -m); a macOS arm64 host is required"
fi

# RAM (HARD - not a warning)
ram="$(mem_gib)"
if [ "$ram" -ge "$SEOUL_MIN_RAM_GIB" ]; then
  pass "RAM ${ram} GiB (>= ${SEOUL_MIN_RAM_GIB})"
else
  bad "RAM ${ram} GiB is below the ${SEOUL_MIN_RAM_GIB} GiB minimum; the chrome link will thrash or OOM"
fi

# Free fast storage (HARD)
free="$(free_gib_for "$CHROMIUM_SRC")"
if [ "$free" -ge "$SEOUL_MIN_BUILD_FREE_GIB" ]; then
  pass "free storage ${free} GiB (>= ${SEOUL_MIN_BUILD_FREE_GIB})"
else
  bad "free storage ${free} GiB is below the ${SEOUL_MIN_BUILD_FREE_GIB} GiB minimum for a build"
fi

# Xcode + SDK (HARD)
dev="$(xcode-select -p 2>/dev/null || true)"
if [ -z "$dev" ]; then
  bad "xcode-select has no active developer directory"
elif printf '%s' "$dev" | grep -q "CommandLineTools"; then
  bad "active developer dir is Command Line Tools ($dev); full Xcode is required to build Chromium"
elif xcodebuild -version >/dev/null 2>&1; then
  pass "full Xcode active: $(xcodebuild -version 2>/dev/null | head -1)"
else
  bad "xcodebuild is not usable with the active developer dir ($dev)"
fi
if xcrun --show-sdk-path >/dev/null 2>&1; then
  pass "macOS SDK reachable ($(xcrun --show-sdk-version 2>/dev/null || echo '?'))"
else
  bad "no usable macOS SDK (xcrun --show-sdk-path failed)"
fi

# Checkout + lock verification (HARD)
if "$SEOUL_SCRIPT_DIR/verify-checkout.sh" >/dev/null 2>&1; then
  pass "checkout verification passed"
else
  bad "checkout verification failed (run native/scripts/verify-checkout.sh for detail)"
fi

stage "build-host result"
if [ "$fail" -ne 0 ]; then
  die "this host is NOT cleared to build Chromium; do not run gen.sh/build.sh here"
fi
log "build-host cleared; planned Ninja jobs: $(resolve_jobs) (override with SEOUL_NINJA_JOBS)"
