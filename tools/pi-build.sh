#!/usr/bin/env bash
# Build and test Fanta on the Raspberry Pi (Linux, GCC, aarch64), from the Mac.
#   tools/pi-build.sh [--clean] [--host user@host]
# Snapshots the working tree as it is now (uncommitted and untracked files included,
# ignored ones not) without touching HEAD or the index, pushes it to ~/fanta on the
# Pi and builds there. The Pi has no GitHub access, so the code always comes from this Mac.
# The Pi also runs the clingy web app: the build is capped at 2 cores and lowest CPU/IO
# priority, and the OOM killer picks it before anything else.
# --clean rebuilds from scratch, so warnings from every file are shown, not only changed ones.
# One-time setup on the Pi: sudo apt install cmake ninja-build libsdl2-dev libncurses-dev
set -euo pipefail
cd "$(dirname "$0")/.."
HOST=pjmicolet@clingy.local
CLEAN=0
while (($#)); do
  case $1 in
    --host) HOST=$2; shift 2 ;;
    --clean) CLEAN=1; shift ;;
    *) echo "usage: $0 [--clean] [--host user@host]" >&2; exit 2 ;;
  esac
done

# Snapshot commit built from a throwaway index (a copy of the real one, to reuse its stat cache).
idx=$(mktemp)
trap 'rm -f "$idx"' EXIT
cp "$(git rev-parse --git-path index)" "$idx"
GIT_INDEX_FILE=$idx git add -A
snap=$(git commit-tree "$(GIT_INDEX_FILE=$idx git write-tree)" -p HEAD -m "pi-build snapshot")
echo "snapshot of $(git branch --show-current) @ $(git rev-parse --short HEAD) + working tree"

ssh -o ConnectTimeout=5 "$HOST" '[ -d ~/fanta/.git ] || git init -q ~/fanta'
git push -q -f "ssh://$HOST/~/fanta" "$snap:refs/heads/snapshot"

ssh "$HOST" CLEAN=$CLEAN bash -s <<'EOF'
set -euo pipefail
cd ~/fanta
git checkout -q -f --detach snapshot
git clean -q -fdx -e build-pi
[ "$CLEAN" = 1 ] && rm -rf build-pi
capped() { systemd-run --user --scope --quiet -p CPUQuota=200% choom -n 1000 -- nice -n 19 ionice -c3 "$@"; }
log=build-pi/build.log
mkdir -p build-pi
status=0
{ capped cmake -S . -B build-pi -G Ninja -DCMAKE_BUILD_TYPE=Release -Wno-dev >/dev/null &&
  capped cmake --build build-pi -j2; } >"$log" 2>&1 || status=$?
warnings=$(grep -E ': warning:' "$log" | sort -u || true)
echo "$(g++ --version | head -1): $(grep -c . <<<"$warnings" || true) unique warnings (full log: ~/fanta/$log)"
[ -n "$warnings" ] && head -40 <<<"$warnings"
if ((status)); then
  echo "✗ build failed:"
  grep -q -E ': error:|FAILED:' "$log" && grep -E -A3 ': error:|FAILED:' "$log" | head -60 || tail -15 "$log"
  exit 1
fi
capped ctest --test-dir build-pi --output-on-failure -j2 2>&1 | tail -25
EOF
