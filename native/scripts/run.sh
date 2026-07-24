#!/usr/bin/env bash
# Launch the locally built baseline Chromium with a DISPOSABLE temporary profile.
# Any extra arguments are passed through to Chromium.
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

[ -x "$CHROMIUM_BINARY" ] || die "built Chromium not found at $CHROMIUM_BINARY (run build.sh first)"

PROFILE="$(mktemp -d "${TMPDIR:-/tmp}/seoul-baseline-profile.XXXXXX")"
cleanup() { [ -n "${PROFILE:-}" ] && rm -rf "$PROFILE"; }
trap cleanup EXIT

# Local development-only convenience flags. These are NOT production defaults:
#   --use-mock-keychain                 avoid macOS keychain prompts in a throwaway run
#   --disable-features=DialMediaRouteProvider  silence cast discovery noise locally
#   --no-first-run / --no-default-browser-check keep every disposable-profile
#        run focused on Seoul instead of Chromium onboarding.
DEV_FLAGS=(
  --use-mock-keychain
  --disable-features=DialMediaRouteProvider
  --no-first-run
  --no-default-browser-check
)

stage "launch baseline Chromium"
log "binary:  $CHROMIUM_BINARY"
log "profile: $PROFILE (removed on exit)"
log "dev flags (local only): ${DEV_FLAGS[*]}"
"$CHROMIUM_BINARY" --user-data-dir="$PROFILE" "${DEV_FLAGS[@]}" "$@"
