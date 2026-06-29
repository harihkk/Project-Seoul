#!/usr/bin/env bash
# Build only the `chrome` target with capped parallelism (-j 2) because this host
# has limited memory. Records timing and output size. Does not build `all`,
# unit tests or browser tests.
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

[ -d "$CHROMIUM_SRC/.git" ] || die "no Chromium checkout at $CHROMIUM_SRC (run fetch.sh + sync.sh first)"
[ -f "$OUT_DIR/args.gn" ]   || die "build dir not generated; run gen.sh first"

REV="$(lock_chromium_revision)"
use_depot_tools

# -j 2 is a cautious LOCAL resource setting for an 8 GiB machine, not an upstream
# requirement. Do not raise it merely to look faster.
JOBS=2
CAFF=""; command -v caffeinate >/dev/null 2>&1 && CAFF="caffeinate -dimsu"

stage "build chrome (revision $REV, -j $JOBS)"
start_epoch="$(date +%s)"
log "start: $(date -Iseconds)"

set +e
( cd "$CHROMIUM_SRC" && $CAFF autoninja -C "out/$OUT_DIR_NAME" -j "$JOBS" chrome )
rc=$?
set -e

end_epoch="$(date +%s)"
elapsed=$(( end_epoch - start_epoch ))
log "end:   $(date -Iseconds)"
log "elapsed: $(( elapsed / 3600 ))h $(( (elapsed % 3600) / 60 ))m $(( elapsed % 60 ))s"

if [ "$rc" -ne 0 ]; then
  die "build failed (rc=$rc). Capture the exact error; permit at most two narrow corrections. Do not broaden GN flags or disable security."
fi

stage "build artifacts"
[ -x "$CHROMIUM_BINARY" ] || die "build reported success but binary not found at $CHROMIUM_BINARY"
log "binary: $CHROMIUM_BINARY"
log "output size: $(du -sh "$OUT_DIR" | awk '{print $1}')"
log "OK: chrome built at revision $REV"
