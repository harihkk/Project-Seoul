#!/usr/bin/env bash
# Pin the existing checkout to the locked Chromium revision and sync dependencies.
# Verifies the lock BEFORE and AFTER. Never resets a dirty checkout.
#
#   sync.sh                 pin + gclient sync to the locked revision
#   sync.sh --verify-only   only verify the current checkout matches the lock
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

VERIFY_ONLY=0
[ "${1:-}" = "--verify-only" ] && VERIFY_ONLY=1

# A gclient checkout is never "git clean": it always shows modified submodule
# pointers and untracked dependency directories. "Dirty" for our safety purposes
# means modified/deleted TRACKED, non-submodule files (i.e. real user edits).
src_has_user_edits() {
  [ -n "$(git -C "$CHROMIUM_SRC" status --porcelain --ignore-submodules=all --untracked-files=no 2>/dev/null)" ]
}

REV="$(lock_chromium_revision)"; [ -n "$REV" ] || die "no chromium.revision in lock file"
log "locked chromium revision: $REV"

[ -d "$CHROMIUM_SRC/.git" ] || die "no Chromium checkout at $CHROMIUM_SRC (run fetch.sh first)"

current="$(git_head_sha "$CHROMIUM_SRC")"
log "checkout HEAD: ${current:-<unknown>}"

if [ "$VERIFY_ONLY" -eq 1 ]; then
  stage "verify-only"
  if [ "$current" = "$REV" ]; then
    ! src_has_user_edits || warn "checkout is at the locked revision but has local edits to tracked files"
    log "OK: checkout is at the locked revision"
    exit 0
  fi
  die "checkout HEAD ($current) does not match locked revision ($REV); run sync.sh to pin it"
fi

if src_has_user_edits; then
  die "checkout has uncommitted edits to tracked files; refusing to sync (will not reset your work)"
fi

use_depot_tools
stage "pin to locked revision"
if ! git -C "$CHROMIUM_SRC" cat-file -e "${REV}^{commit}" 2>/dev/null; then
  log "fetching locked revision object (shallow)"
  git -C "$CHROMIUM_SRC" fetch --depth 1 origin "$REV"
fi
git -C "$CHROMIUM_SRC" checkout --detach "$REV"

stage "gclient sync (deps for the locked revision; shallow, no patches)"
CAFF=""; command -v caffeinate >/dev/null 2>&1 && CAFF="caffeinate -dimsu"
( cd "$CHROMIUM_SRC" && $CAFF gclient sync --no-history --with_branch_heads --with_tags --revision "src@$REV" -D )

stage "gclient runhooks"
( cd "$CHROMIUM_SRC" && $CAFF gclient runhooks )

stage "verify after sync"
after="$(git_head_sha "$CHROMIUM_SRC")"
[ "$after" = "$REV" ] || die "post-sync HEAD ($after) does not match locked revision ($REV)"
! src_has_user_edits || warn "checkout has local edits to tracked files after sync"
( cd "$CHROMIUM_SRC" && gclient validate )
log "OK: checkout pinned and synced at $REV"
