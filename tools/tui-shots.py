#!/usr/bin/env python3
"""Render standard TUI workflows headlessly and print only what changed since a baseline.

Usage:
  tools/tui-shots.py [--only TEXT] [--size COLSxROWS]   compare with the baseline
  tools/tui-shots.py --update                           accept the current screens
  tools/tui-shots.py --list                             list workflows

Run it after building fanta-tui and fanta-compiler. Fixtures are generated here because
test/*.asm is untracked. Screens and baseline are local, under .cache/tui-shots/; read a
full screen from the path printed for it. FANTA_BUILD_DIR overrides the build directory.
"""
import argparse
import difflib
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, ".cache", "tui-shots")
FIXTURES = os.path.join(CACHE, "fixtures")
CURRENT = os.path.join(CACHE, "current")
BASELINE = os.path.join(CACHE, "baseline")
SIZES = ("80x24", "120x40", "200x60")
MAX_DIFF_LINES = 30

ASM = """; Sends "Hi" to the serial port, then halts.
START:
    MOV R1, #200
    LSH R1, R1, #10
    MOV R2, #48
    STORE R2, 0(R1)
    MOV R2, #69
    STORE R2, 0(R1)
    HALT
"""
FN = """fn main() -> int {
    let b : int = 10;
    let c : int = 20;
    return b + c;
}
"""
# Not a missing initialiser: that reads out of bounds in the compiler (tui_friction.md).
BAD_FN = """fn main() -> int {
    return nope(1);
}
"""
FILES = {"hello.asm": ASM, "sum.fn": FN, "broken.fn": BAD_FN}

# name, file, keys. Workflows follow .claude/agents/fanta-tui-reviewer.md.
WORKFLOWS = [
    ("empty", "", ""),
    ("help", "hello.asm", "<F1>"),
    ("asm-open", "hello.asm", ""),
    ("asm-step", "hello.asm", "<F7><F7><F7>"),
    ("asm-run-serial", "hello.asm", "<F5><FRAME><FRAME><FRAME>"),
    ("asm-edit", "hello.asm", "jjiNOP <ESC>"),
    ("fn-open", "sum.fn", ""),
    ("fn-build", "sum.fn", "<F9><FRAME>"),
    ("fn-run", "sum.fn", "<F9><FRAME><F5><FRAME><FRAME><FRAME>"),
    ("fn-stale", "sum.fn", "<F9><FRAME>ix<ESC>"),
    ("fn-error", "broken.fn", "<F9><FRAME>"),
]


def render(binary, name, file, keys, size):
    out = os.path.join(CURRENT, f"{name}-{size}.txt")
    cmd = [binary] + ([file] if file else []) + ["--screen", out, "--size", size, "--keys", keys]
    # Relative names keep the paths on screen the same in every checkout.
    run = subprocess.run(cmd, cwd=FIXTURES, capture_output=True, text=True, timeout=120)
    if run.returncode != 0 or not os.path.exists(out):
        detail = (run.stderr or "no output").strip().splitlines()[-1]
        return None, f"exit {run.returncode}, {detail}"
    return out, None


def shown_diff(old, new):
    lines = list(difflib.unified_diff(old, new, lineterm="", n=0))[2:]
    lines = [line.rstrip()[:130] for line in lines if not line.startswith("@@")]
    extra = len(lines) - MAX_DIFF_LINES
    return lines[:MAX_DIFF_LINES] + ([f"... and {extra} more lines"] if extra > 0 else [])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--only", default="")
    ap.add_argument("--size", default="")
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    workflows = [w for w in WORKFLOWS if args.only in w[0]]
    sizes = [args.size] if args.size else list(SIZES)
    if args.list:
        for name, file, keys in WORKFLOWS:
            print(f"{name:16} {file or '-':10} {keys}")
        return 0

    build = os.environ.get("FANTA_BUILD_DIR", "build")
    build = build if os.path.isabs(build) else os.path.join(ROOT, build)
    binary = os.path.join(build, "fanta-tui")
    if not os.path.exists(binary):
        print(f"{binary} not found; build fanta-tui and fanta-compiler first", file=sys.stderr)
        return 2

    shutil.rmtree(FIXTURES, ignore_errors=True)
    os.makedirs(FIXTURES)
    os.makedirs(CURRENT, exist_ok=True)
    for filename, content in FILES.items():
        with open(os.path.join(FIXTURES, filename), "w") as f:
            f.write(content)
    same, changed, new, failed = 0, 0, [], 0
    for name, file, keys in workflows:
        for size in sizes:
            out, error = render(binary, name, file, keys, size)
            if out is None:
                print(f"FAILED {name}-{size}: {error}")
                failed += 1
                continue
            base = os.path.join(BASELINE, os.path.basename(out))
            if args.update:
                os.makedirs(BASELINE, exist_ok=True)
                shutil.copyfile(out, base)
            elif not os.path.exists(base):
                new.append(f"{name}-{size}")
            else:
                old_lines = open(base, errors="replace").read().splitlines()
                new_lines = open(out, errors="replace").read().splitlines()
                if old_lines == new_lines:
                    same += 1
                else:
                    changed += 1
                    print(f"CHANGED {name}-{size}: {os.path.relpath(out, ROOT)}")
                    print("\n".join(shown_diff(old_lines, new_lines)))

    if args.update:
        print(f"baseline updated: {len(workflows) * len(sizes) - failed} screens, {failed} failed")
    else:
        print(f"{same} same, {changed} changed, {len(new)} without a baseline, {failed} failed")
        if new:
            print("run --update to accept: " + " ".join(new[:6]) + (" ..." if len(new) > 6 else ""))
    return 1 if failed or changed else 0


if __name__ == "__main__":
    sys.exit(main())
