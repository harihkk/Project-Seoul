#!/usr/bin/env bash
# Preflight doctor: report whether this host can fetch and build the pinned
# Chromium baseline. Read-only. Never installs, upgrades or uses sudo.
#
# Exit nonzero if any HARD prerequisite is missing. WARN-level findings (e.g. low
# RAM, tight disk) do not fail, but are reported plainly.
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

fail=0
ok()   { printf 'PASS  %s\n' "$*"; }
bad()  { printf 'FAIL  %s\n' "$*"; fail=1; }
note() { printf 'WARN  %s\n' "$*"; }

stage "Seoul native baseline doctor"
log "checkout root: $SEOUL_CHROMIUM_ROOT_ABS"
log "output dir:    $OUT_DIR"

# Architecture
if [ "$(uname -m)" = "arm64" ]; then ok "architecture is arm64"; else bad "architecture is $(uname -m), expected arm64"; fi

# macOS
if command -v sw_vers >/dev/null 2>&1; then ok "macOS $(sw_vers -productVersion) ($(sw_vers -buildVersion))"; else bad "sw_vers not available (not macOS?)"; fi

# Checkout path safety (already validated in common.sh; restate for the report)
case "$SEOUL_CHROMIUM_ROOT_ABS" in *" "*) bad "checkout path contains a space";; *) ok "checkout path has no spaces";; esac
case "$SEOUL_CHROMIUM_ROOT_ABS/" in "$SEOUL_REPO_ROOT"/*) bad "checkout root is inside the repo";; *) ok "checkout root is outside the Project Seoul repo";; esac

# Filesystem of the checkout volume must be APFS. diskutil needs a device, not an
# arbitrary directory, so resolve the device via df first; guard so a probe failure
# never aborts this diagnostic.
probe="$SEOUL_CHROMIUM_ROOT_ABS"; while [ ! -e "$probe" ] && [ "$probe" != "/" ]; do probe="$(dirname "$probe")"; done
dev="$(df "$probe" 2>/dev/null | awk 'NR==2 {print $1}' || true)"
fs="$(diskutil info "$dev" 2>/dev/null | awk -F': *' '/File System Personality/ {print $2}' || true)"
case "$fs" in *APFS*) ok "checkout volume is APFS ($fs)";; "") note "could not determine filesystem of $probe";; *) note "checkout volume is $fs, not APFS";; esac

# Full Xcode (Command Line Tools alone are not sufficient for a Chromium build)
dev="$(xcode-select -p 2>/dev/null || true)"
if [ -z "$dev" ]; then
  bad "xcode-select has no active developer directory"
elif printf '%s' "$dev" | grep -q "CommandLineTools"; then
  bad "active developer dir is Command Line Tools ($dev); Chromium's macOS build requires full Xcode"
elif xcodebuild -version >/dev/null 2>&1; then
  ok "full Xcode active: $(xcodebuild -version 2>/dev/null | head -1) at $dev"
else
  bad "xcodebuild is not usable with the active developer dir ($dev)"
fi

# macOS SDK reachable through the active toolchain
if sdk="$(xcrun --show-sdk-path 2>/dev/null)" && [ -n "$sdk" ]; then
  ok "macOS SDK: $sdk ($(xcrun --show-sdk-version 2>/dev/null || echo '?'))"
else
  bad "no usable macOS SDK (xcrun --show-sdk-path failed)"
fi

# Tooling
for c in git python3 curl; do command -v "$c" >/dev/null 2>&1 && ok "$c present ($($c --version 2>&1 | head -1))" || bad "$c not found"; done

# Memory (Chromium recommends >= 16 GiB; the chrome link is memory-hungry)
mem_gib=$(( $(sysctl -n hw.memsize) / 1024 / 1024 / 1024 ))
if [ "$mem_gib" -ge 16 ]; then ok "RAM ${mem_gib} GiB"; else note "RAM ${mem_gib} GiB is below Chromium's practical minimum (16 GiB); the chrome link may thrash or OOM even at -j2"; fi

# Disk (a no-history checkout plus a component build typically needs ~100-150 GiB)
free_gib="$(free_gib_for "$SEOUL_CHROMIUM_ROOT_ABS")"
if   [ "$free_gib" -lt 120 ]; then bad  "free space ${free_gib} GiB is insufficient for a Chromium checkout + build (need ~120-150 GiB)"
elif [ "$free_gib" -lt 180 ]; then note "free space ${free_gib} GiB is tight for a Chromium checkout + build (recommend >= 180 GiB headroom)"
else ok "free space ${free_gib} GiB"; fi

# depot_tools / checkout presence (informational; fetch.sh creates them)
[ -d "$DEPOT_TOOLS_DIR" ] && log "depot_tools present at $DEPOT_TOOLS_DIR" || log "depot_tools not yet cloned (fetch.sh will clone it)"
[ -d "$CHROMIUM_SRC" ]    && log "chromium src present at $CHROMIUM_SRC"   || log "chromium src not yet fetched (fetch.sh will fetch it)"

stage "doctor result"
if [ "$fail" -ne 0 ]; then
  die "one or more HARD prerequisites are missing; resolve them before running fetch.sh/build.sh"
fi
log "all hard prerequisites satisfied (review any WARN lines above)"
