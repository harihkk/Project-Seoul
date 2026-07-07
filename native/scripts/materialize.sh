#!/usr/bin/env bash
# Materialize repository-owned Seoul source (native/seoul/) into the external
# Chromium checkout at src/seoul/. Deterministic and reversible. Only ever writes
# under the Seoul-owned overlay directory; never touches unmodified Chromium files.
#
#   materialize.sh apply     mirror native/seoul/ -> src/seoul/ and the canonical
#                            protocol/ contract -> src/seoul/protocol/  (default)
#   materialize.sh verify    read-only: report whether src/seoul/ matches the source
#   materialize.sh reverse   remove the materialized src/seoul/ overlay
#
# protocol/ (schemas + shared conformance fixtures) is mirrored INTO the
# overlay so native conformance tests read the identical corpus the
# TypeScript tests read; the main mirror excludes /protocol so the two rsyncs
# never fight over it.
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
    rsync -a --omit-dir-times --delete --exclude='.DS_Store' --exclude='/protocol' "$SEOUL_SRC_DIR"/ "$SEOUL_OVERLAY_DEST"/
    stage "materialize protocol/ -> $SEOUL_OVERLAY_DEST/protocol"
    rsync -a --omit-dir-times --delete --exclude='.DS_Store' "$SEOUL_PROTOCOL_DIR"/ "$SEOUL_OVERLAY_DEST"/protocol/
    log "OK: Seoul source and canonical protocol materialized"
    ;;
  verify)
    stage "verify overlay matches native/seoul/ (read-only)"
    if [ ! -d "$SEOUL_OVERLAY_DEST" ]; then
      log "overlay not present at $SEOUL_OVERLAY_DEST (run: materialize.sh apply)"
      exit 1
    fi
    diff_out="$(rsync -a --omit-dir-times --delete --dry-run --itemize-changes --exclude='.DS_Store' --exclude='/protocol' "$SEOUL_SRC_DIR"/ "$SEOUL_OVERLAY_DEST"/)"
    if [ -n "$diff_out" ]; then
      warn "overlay differs from native/seoul/:"
      printf '%s\n' "$diff_out"
      exit 1
    fi
    proto_diff="$(rsync -a --omit-dir-times --delete --dry-run --itemize-changes --exclude='.DS_Store' "$SEOUL_PROTOCOL_DIR"/ "$SEOUL_OVERLAY_DEST"/protocol/)"
    if [ -n "$proto_diff" ]; then
      warn "overlay protocol/ differs from repository protocol/:"
      printf '%s\n' "$proto_diff"
      exit 1
    fi
    log "OK: overlay matches native/seoul/ and protocol/"
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
