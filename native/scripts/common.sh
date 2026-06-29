#!/usr/bin/env bash
# Shared helpers for the Seoul native-bootstrap scripts.
#
# Source this file; do not execute it directly. It resolves the external Chromium
# checkout root, defines logging/guard helpers, and reads the pinned revision from
# native/chromium.lock.json. It never deletes or resets anything.
set -euo pipefail

SEOUL_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SEOUL_REPO_ROOT="$(cd "$SEOUL_SCRIPT_DIR/../.." && pwd)"
SEOUL_NATIVE_DIR="$SEOUL_REPO_ROOT/native"
LOCK_FILE="$SEOUL_NATIVE_DIR/chromium.lock.json"

log()   { printf '[seoul-native] %s\n' "$*"; }
stage() { printf '\n==> %s\n' "$*"; }
warn()  { printf '[warn] %s\n' "$*" >&2; }
die()   { printf '[error] %s\n' "$*" >&2; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"; }

# Reject empty, relative, space-containing or shell-unsafe paths.
assert_safe_path() {
  local p="${1:-}" what="${2:-path}"
  [ -n "$p" ] || die "$what is empty"
  case "$p" in
    /*) : ;;
    *)  die "$what must be an absolute path: $p" ;;
  esac
  case "$p" in
    *" "*) die "$what contains a space (the Chromium build does not tolerate this): $p" ;;
  esac
  case "$p" in
    *'$'* | *'`'* | *';'* | *'|'* | *'&'* | *'*'* | *'?'* | *'<'* | *'>'* | *'('* | *')'* | *'!'*)
      die "$what contains unsafe shell characters: $p" ;;
  esac
}

# Resolve the external checkout root. SEOUL_CHROMIUM_ROOT overrides; otherwise a
# sibling of the Project Seoul repo named seoul-chromium. Never inside the repo.
resolve_root() {
  local root="${SEOUL_CHROMIUM_ROOT:-}"
  if [ -z "$root" ]; then
    root="$(cd "$SEOUL_REPO_ROOT/.." && pwd)/seoul-chromium"
  fi
  case "$root" in /*) : ;; *) root="$(pwd)/$root" ;; esac
  assert_safe_path "$root" "SEOUL_CHROMIUM_ROOT"
  case "$root/" in
    "$SEOUL_REPO_ROOT"/*) die "checkout root must be OUTSIDE the Project Seoul repo: $root" ;;
  esac
  printf '%s\n' "$root"
}

SEOUL_CHROMIUM_ROOT_ABS="$(resolve_root)"
DEPOT_TOOLS_DIR="$SEOUL_CHROMIUM_ROOT_ABS/depot_tools"
CHROMIUM_SRC="$SEOUL_CHROMIUM_ROOT_ABS/src"
OUT_DIR_NAME="SeoulBaseline"
OUT_DIR="$CHROMIUM_SRC/out/$OUT_DIR_NAME"
CHROMIUM_BINARY="$OUT_DIR/Chromium.app/Contents/MacOS/Chromium"
EVIDENCE_DIR="$SEOUL_NATIVE_DIR/evidence"

# Read a dotted field from the lock file (no jq dependency).
lock_field() {
  local path="$1"
  [ -f "$LOCK_FILE" ] || die "lock file not found: $LOCK_FILE"
  python3 - "$LOCK_FILE" "$path" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
cur = data
for part in sys.argv[2].split('.'):
    if isinstance(cur, dict) and part in cur:
        cur = cur[part]
    else:
        cur = None
        break
print('' if cur is None else cur)
PY
}

lock_chromium_revision() { lock_field chromium.revision; }
lock_chromium_version()  { lock_field chromium.version; }
lock_depot_revision()    { lock_field depotTools.revision; }

# Prepend depot_tools to PATH and disable its self-update so the pinned revision
# is honored.
use_depot_tools() {
  [ -d "$DEPOT_TOOLS_DIR" ] || die "depot_tools not present at $DEPOT_TOOLS_DIR (run fetch.sh first)"
  export PATH="$DEPOT_TOOLS_DIR:$PATH"
  export DEPOT_TOOLS_UPDATE=0
}

# Current detached/checked-out SHA of a git tree (empty if not a repo).
git_head_sha() { git -C "$1" rev-parse HEAD 2>/dev/null || true; }

# True if the working tree at $1 has no uncommitted changes.
git_is_clean() { [ -z "$(git -C "$1" status --porcelain 2>/dev/null)" ]; }

# Free space (GiB, integer) on the volume holding the nearest existing ancestor.
free_gib_for() {
  local p="$1"
  while [ ! -e "$p" ] && [ "$p" != "/" ]; do p="$(dirname "$p")"; done
  df -k "$p" | awk 'NR==2 { printf "%d", $4/1024/1024 }'
}
