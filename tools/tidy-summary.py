#!/usr/bin/env python3
"""Summarise clang-tidy findings by check and list only those new since a saved baseline.

Usage:
  tools/tidy-summary.py [REGEX]             run tools/run-clang-tidy.sh and summarise
  tools/tidy-summary.py [REGEX] --from LOG  summarise saved clang-tidy output instead
  add --update to accept the current findings as the baseline

A finding's identity is file + check + message, so line shifts don't make it "new". The
baseline is local, in .cache/tidy-baseline.txt. Findings still go to the user for triage:
see CLAUDE.md §5.
"""
import argparse
import collections
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join(ROOT, ".cache", "tidy-baseline.txt")
FINDING = re.compile(r"^(/?[^\s:][^:]*):(\d+):\d+: (warning|error): (.*) \[([^\]]+)\]$")
MAX_NEW = 40


def relative(path):
    return os.path.relpath(path, ROOT) if os.path.isabs(path) else path


def parse(log):
    """Return {identity: first line number}; headers repeat once per includer."""
    found = {}
    for line in log.splitlines():
        m = FINDING.match(line)
        if m:
            path, number, _, message, check = m.groups()
            found.setdefault((relative(path), check, message), int(number))
    return found


def load_baseline():
    if not os.path.exists(BASELINE):
        return None
    with open(BASELINE) as f:
        return {tuple(line.rstrip("\n").split("\t")) for line in f if line.strip()}


def save_baseline(found, regex, old):
    # A partial run replaces only the files it analysed or reported on.
    seen = {k[0] for k in found}
    kept = {k for k in (old or set()) if regex and k[0] not in seen and not re.search(regex, k[0])}
    os.makedirs(os.path.dirname(BASELINE), exist_ok=True)
    with open(BASELINE, "w") as f:
        for key in sorted(kept | set(found)):
            f.write("\t".join(key) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("regex", nargs="?", default="")
    ap.add_argument("--from", dest="log")
    ap.add_argument("--update", action="store_true")
    args = ap.parse_args()

    if args.log:
        log = open(args.log, errors="replace").read()
    else:
        cmd = [os.path.join(ROOT, "tools", "run-clang-tidy.sh")] + ([args.regex] if args.regex else [])
        run = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
        log = run.stdout + run.stderr
        if not FINDING.search(log) and run.returncode not in (0, 1):
            print(log[-1500:], file=sys.stderr)
            return run.returncode

    found = parse(log)
    # REGEX picks the files to analyse; findings in the headers they include still count.
    found = {k: v for k, v in found.items() if not k[0].startswith("..")}
    old = load_baseline()

    by_check = collections.Counter(k[1] for k in found)
    print(f"{len(found)} findings across {len({k[0] for k in found})} files")
    for check, count in by_check.most_common():
        print(f"{count:>5} {check}")

    if old is None:
        print("\nno baseline yet; --update saves these as the baseline")
    else:
        new = sorted(k for k in found if k not in old)
        seen = {k[0] for k in found}
        scope = {k for k in old if not args.regex or k[0] in seen or re.search(args.regex, k[0])}
        print(f"\n{len(new)} new, {len(scope - set(found))} gone since the baseline")
        for path, check, message in new[:MAX_NEW]:
            print(f"{path}:{found[(path, check, message)]}: {message} [{check}]")
        if len(new) > MAX_NEW:
            print(f"... and {len(new) - MAX_NEW} more")

    if args.update:
        save_baseline(found, args.regex, old)
        print("baseline updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
