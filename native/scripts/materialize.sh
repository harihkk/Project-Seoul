#!/usr/bin/env bash
# Materialize repository-owned Seoul source (native/seoul/) into the external
# Chromium checkout at src/seoul/. Deterministic and reversible. Only ever writes
# under the Seoul-owned overlay directory; never touches unmodified Chromium files.
#
#   materialize.sh apply     mirror native/seoul/ -> src/seoul/  (default)
#   materialize.sh verify    read-only: report whether src/seoul/ matches the source
#   materialize.sh reverse   remove the materialized src/seoul/ overlay
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

CMD="${1:-apply}"
need_cmd rsync

[ -d "$SEOUL_SRC_DIR" ] || die "Seoul source dir not found: $SEOUL_SRC_DIR"
[ -d "$CHROMIUM_SRC/.git" ] || die "no Chromium checkout at $CHROMIUM_SRC (run fetch.sh + sync.sh first)"

# Safety: the overlay destination must be the dedicated Seoul dir inside the
# checkout, never an upstream Chromium path.
case "$SEOUL_OVERLAY_DEST" in
  "$CHROMIUM_SRC"/seoul) : ;;
  *) die "overlay destination is not the Seoul overlay dir: $SEOUL_OVERLAY_DEST" ;;
esac

case "$CMD" in
  apply)
    stage "materialize native/seoul/ -> $SEOUL_OVERLAY_DEST"
    mkdir -p "$SEOUL_OVERLAY_DEST"
    rsync -a --delete "$SEOUL_SRC_DIR"/ "$SEOUL_OVERLAY_DEST"/
    log "OK: Seoul source materialized"
    ;;
  verify)
    stage "verify overlay matches native/seoul/ (read-only)"
    if [ ! -d "$SEOUL_OVERLAY_DEST" ]; then
      log "overlay not present at $SEOUL_OVERLAY_DEST (run: materialize.sh apply)"
      exit 1
    fi
    diff_out="$(rsync -a --delete --dry-run --itemize-changes "$SEOUL_SRC_DIR"/ "$SEOUL_OVERLAY_DEST"/)"
    if [ -n "$diff_out" ]; then
      warn "overlay differs from native/seoul/:"
      printf '%s\n' "$diff_out"
      exit 1
    fi
    log "OK: overlay matches native/seoul/"
    ;;
  reverse)
    stage "remove materialized overlay $SEOUL_OVERLAY_DEST"
    if [ -d "$SEOUL_OVERLAY_DEST" ]; then
      rm -rf "$SEOUL_OVERLAY_DEST"
      log "OK: overlay removed"
    else
      log "overlay not present; nothing to remove"
    fi
    ;;
  *)
    die "usage: materialize.sh [apply|verify|reverse]"
    ;;
esac
