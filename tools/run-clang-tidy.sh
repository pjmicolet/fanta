#!/usr/bin/env bash
#
# Runs clang-tidy over Project Fanta using the repo's .clang-tidy config
# and build/compile_commands.json. Thin wrapper around LLVM's
# `run-clang-tidy` -- see CLAUDE.md "Static Analysis & Warnings Workflow"
# for how findings from this get triaged.
#
# Usage:
#   tools/run-clang-tidy.sh                 # whole project
#   tools/run-clang-tidy.sh 'vm/.*'         # only files matching this regex
#   tools/run-clang-tidy.sh -fix 'tui/.*'   # apply auto-fixes (tui/test only, please)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${FANTA_BUILD_DIR:-build}"
COMPILE_DB="$BUILD_DIR/compile_commands.json"

if [[ ! -f "$COMPILE_DB" ]]; then
  echo "error: $COMPILE_DB not found. Configure the project first, e.g.:" >&2
  echo "  cmake -S . -B $BUILD_DIR" >&2
  exit 1
fi

find_tool() {
  local name="$1"
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return
  fi
  for prefix in /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin; do
    if [[ -x "$prefix/$name" ]]; then
      echo "$prefix/$name"
      return
    fi
  done
  return 1
}

CLANG_TIDY_BIN="$(find_tool clang-tidy)" || {
  echo "error: clang-tidy not found on PATH or in a known brew LLVM prefix." >&2
  echo "  brew install llvm" >&2
  exit 1
}
RUN_CLANG_TIDY_BIN="$(find_tool run-clang-tidy)" || {
  echo "error: run-clang-tidy not found alongside clang-tidy at $CLANG_TIDY_BIN" >&2
  exit 1
}

# On macOS, LLVM's clang-tidy doesn't auto-detect the SDK sysroot the way
# Xcode's driver (/usr/bin/c++) does, so <cstdint>/<curses.h>/etc. resolve
# to nothing without this. No-op on Linux.
SYSROOT_ARGS=()
if [[ "$(uname -s)" == "Darwin" ]] && command -v xcrun >/dev/null 2>&1; then
  SDK_PATH="$(xcrun --show-sdk-path 2>/dev/null || true)"
  if [[ -n "$SDK_PATH" ]]; then
    SYSROOT_ARGS=(-extra-arg=-isysroot -extra-arg="$SDK_PATH")
  fi
fi

LOG_FILE="$(mktemp)"
trap 'rm -f "$LOG_FILE"' EXIT

# Default scope: project sources only, never the build directories or
# vendored/system headers pulled in via compile_commands.json.
if [[ $# -gt 0 ]]; then
  ARGS=("$@")
else
  ARGS=('.*/(compiler|vm|common|tui|test)/.*\.(cpp|hpp|h)$')
fi

"$RUN_CLANG_TIDY_BIN" \
  -p "$BUILD_DIR" \
  -clang-tidy-binary "$CLANG_TIDY_BIN" \
  -quiet \
  "${SYSROOT_ARGS[@]}" \
  "${ARGS[@]}" \
  2>&1 | tee "$LOG_FILE"

echo
echo "== Summary by check =========================================="
grep -oE '\[[a-zA-Z0-9.,-]+\]$' "$LOG_FILE" | sort | uniq -c | sort -rn || true
TOTAL=$(grep -c 'warning:\|error:' "$LOG_FILE" || true)
echo "================================================================"
echo "Total findings: ${TOTAL:-0}"
