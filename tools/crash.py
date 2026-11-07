#!/usr/bin/env python3
"""Run a Fanta target under sanitizers and print a short crash report.

Usage: tools/crash.py [--runs N] [--timeout SECS] [--no-build] TARGET [ARGS...]
  e.g. tools/crash.py fanta-compiler bad.fn -o /dev/null

TARGET is a CMake target, built in build-sanitize/ (ASan, UBSan, libc++ hardening). The
report has the error, where the bad memory is, and only this project's stack frames with
their source lines. Exit status: 0 no crash, 1 crash, 2 setup failure, 124 hang.
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build-sanitize")
SOURCE_DIRS = ("compiler", "vm", "common", "tui", "test")
MAX_FRAMES = 8
ENV = {
    # LeakSanitizer isn't supported on every platform, and leaks aren't crashes.
    "ASAN_OPTIONS": "detect_leaks=0:handle_abort=1:handle_sigill=1:handle_sigtrap=1",
    "UBSAN_OPTIONS": "print_stacktrace=1:halt_on_error=1",
}
ERROR_RE = re.compile(
    r"ERROR: AddressSanitizer: .*|.*runtime error: .*|.*[Hh]ardening assertion.*"
    r"|.*assertion .* failed.*|.*libc\+\+.*(assert|abort).*|.*terminating (due to|with) uncaught exception.*"
)
LOCATION_RE = re.compile(r"(\S+?):(\d+)(?::\d+)?(?:\)|$)")


def project_files():
    """Map basenames to paths, because some symbolizers print only the basename."""
    files = {}
    for directory in SOURCE_DIRS:
        for base, _, names in os.walk(os.path.join(ROOT, directory)):
            for name in names:
                files.setdefault(name, os.path.relpath(os.path.join(base, name), ROOT))
    return files


def shorten(function):
    """Reduce a demangled name to something readable: drop templates and arguments."""
    previous = None
    while previous != function:
        previous = function
        function = re.sub(r"<[^<>]*>", "", function)
    function = re.sub(r"\(.*$", "", function)
    function = re.sub(r"\s+\S+:\d+(:\d+)?$", "", function)
    return function.replace("std::__1::", "std::").strip()[:70]


def project_frames(lines, files):
    frames = []
    for line in lines:
        if not re.match(r"^\s*#\d+ 0x", line):
            continue
        located = LOCATION_RE.search(line.rstrip())
        name = line.split(" in ", 1)[1] if " in " in line else ""
        if not located or os.path.basename(located.group(1)) not in files:
            continue
        path = files[os.path.basename(located.group(1))]
        frame = (shorten(name), path, int(located.group(2)))
        if not frames or frames[-1][1:] != frame[1:]:
            frames.append(frame)
    return frames


def source_line(path, number):
    try:
        with open(os.path.join(ROOT, path), errors="replace") as f:
            return f.read().splitlines()[number - 1].strip()[:90]
    except (OSError, IndexError):
        return ""


def report(run, log):
    lines = log.splitlines()
    files = project_files()
    # Skip harmless startup noise by reporting the first real error.
    error = next((l.strip() for l in lines if ERROR_RE.search(l)), "")
    error = re.sub(r"^==\d+==", "", error)
    error = re.sub(r" on (unknown )?address.*", "", error)
    error = re.sub(r"^\S*/([^/\s]+:\d+:)", r"\1", error)
    print(f"CRASH on run {run}: {error[:170] or 'no sanitizer message, see the output below'}")

    start = next((i for i, l in enumerate(lines) if ERROR_RE.search(l)), 0)
    rest = lines[start:]
    # ASan prints "allocated by" stacks after the crash stack; keep them apart.
    allocated = next((i for i, l in enumerate(rest) if "allocated by" in l), len(rest))
    region = next((l.strip() for l in rest if "is located" in l), "")
    if region:
        print(re.sub(r"^0x[0-9a-f]+ ", "", region)[:150])

    frames = project_frames(rest[:allocated], files)
    if not frames:
        print("no project frames; last output:")
        print("\n".join(line[:150] for line in lines[-8:]))
    for function, path, number in frames[:MAX_FRAMES]:
        print(f"  {path}:{number}  {function}\n      {source_line(path, number)}")
    if len(frames) > MAX_FRAMES:
        print(f"  ... and {len(frames) - MAX_FRAMES} more frames")

    origin = project_frames(rest[allocated:], files)
    if origin:
        function, path, number = origin[0]
        print(f"allocated at {path}:{number}  {function}")


def build(target):
    if not os.path.exists(os.path.join(BUILD, "CMakeCache.txt")):
        configure = ["cmake", "-S", ROOT, "-B", BUILD, "-DFANTA_SANITIZE=ON", "-DCMAKE_BUILD_TYPE=Debug"]
        done = subprocess.run(configure, capture_output=True, text=True)
        if done.returncode:
            print(done.stdout[-800:] + done.stderr[-800:], file=sys.stderr)
            return False
    done = subprocess.run(
        ["cmake", "--build", BUILD, "-j8", "--target", target], capture_output=True, text=True
    )
    if done.returncode:
        errors = [l for l in (done.stdout + done.stderr).splitlines() if "error" in l.lower()]
        print("\n".join(errors[:10]) or "build failed", file=sys.stderr)
        return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("target")
    ap.add_argument("args", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    if not args.no_build and not build(args.target):
        return 2
    binary = os.path.join(BUILD, args.target)
    if not os.path.exists(binary):
        print(f"{binary} not found", file=sys.stderr)
        return 2

    env = {**os.environ, **ENV, "PATH": BUILD + os.pathsep + os.environ.get("PATH", "")}
    last = None
    for run in range(1, args.runs + 1):
        try:
            last = subprocess.run(
                [binary] + args.args, env=env, stdin=subprocess.DEVNULL,
                capture_output=True, text=True, errors="replace", timeout=args.timeout,
            )
        except subprocess.TimeoutExpired:
            print(f"HANG on run {run}: no exit after {args.timeout}s")
            return 124
        log = last.stdout + "\n" + last.stderr
        if last.returncode < 0 or ERROR_RE.search(log):
            report(run, log)
            return 1

    print(f"no crash in {args.runs} runs, last exit {last.returncode}")
    if last.returncode:
        print("\n".join(line[:150] for line in last.stderr.strip().splitlines()[-5:]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
