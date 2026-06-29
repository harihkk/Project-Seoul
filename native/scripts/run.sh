#!/usr/bin/env bash
# Launch the locally built baseline Chromium with a DISPOSABLE temporary profile.
# Any extra arguments (e.g. an upstream feature flag for the audit) are passed
# through to Chromium.
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

[ -x "$CHROMIUM_BINARY" ] || die "built Chromium not found at $CHROMIUM_BINARY (run build.sh first)"

PROFILE="$(mktemp -d "${TMPDIR:-/tmp}/seoul-baseline-profile.XXXXXX")"
cleanup() { [ -n "${PROFILE:-}" ] && rm -rf "$PROFILE"; }
trap cleanup EXIT

# Local development-only convenience flags. These are NOT production defaults:
#   --use-mock-keychain                 avoid macOS keychain prompts in a throwaway run
#   --disable-features=DialMediaRouteProvider  silence cast discovery noise locally
DEV_FLAGS=(--use-mock-keychain --disable-features=DialMediaRouteProvider)

stage "launch baseline Chromium"
log "binary:  $CHROMIUM_BINARY"
log "profile: $PROFILE (removed on exit)"
log "dev flags (local only): ${DEV_FLAGS[*]}"
"$CHROMIUM_BINARY" --user-data-dir="$PROFILE" "${DEV_FLAGS[@]}" "$@"
