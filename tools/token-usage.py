#!/usr/bin/env python3
"""Summarise Claude Code token usage for this project from its local transcripts.

Usage: tools/token-usage.py [--since YYYY-MM-DD] [--last N] [--tools]

"context" sums the tokens sent over all turns; most are cheap cache reads. "first" is the
context at the first turn, which is the session baseline. --tools adds a breakdown of tool
output by kind, which is where most context growth comes from.
"""
import argparse
import collections
import glob
import json
import os
import re
import subprocess
import sys

KINDS = [
    ("tui screens", r"--screen|tui-shots"),
    ("build/test", r"\bcmake\b|\bctest\b|\bmake\b|_test\b"),
    ("clang-tidy", r"tidy"),
    ("git/gh", r"\bgit\b|\bgh\b"),
    ("read code", r"\bcat\b|\bsed -n|\bhead\b|\btail\b|\bgrep\b|\bawk\b|outline"),
]


def project_dir():
    # Use the main checkout's path, so this also works from a linked worktree.
    top = subprocess.run(
        ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
        capture_output=True, text=True,
    ).stdout.strip()
    root = os.path.dirname(top) if top else os.getcwd()
    return os.path.expanduser("~/.claude/projects/" + re.sub(r"[^A-Za-z0-9]", "-", root))


def records(path):
    with open(path, errors="ignore") as f:
        for line in f:
            try:
                yield json.loads(line)
            except ValueError:
                continue


def text_len(content):
    if isinstance(content, str):
        return len(content)
    if isinstance(content, list):
        return sum(
            text_len(x.get("text") or x.get("content") or "") if isinstance(x, dict) else 0
            for x in content
        )
    return 0


def kind_of(name, tool_input):
    if name != "Bash":
        return name
    command = tool_input.get("command", "")
    return next((k for k, pattern in KINDS if re.search(pattern, command)), "other bash")


def summarise(path, kinds):
    turns, start, tool_names = {}, None, {}
    for r in records(path):
        start = start or r.get("timestamp")
        message = r.get("message") or {}
        usage = message.get("usage")
        if r.get("type") == "assistant" and usage and message.get("id"):
            # A streamed message has several records; keep its largest output count.
            old = turns.get(message["id"], (0, 0))
            context = sum(
                usage.get(k, 0)
                for k in ("input_tokens", "cache_read_input_tokens", "cache_creation_input_tokens")
            )
            turns[message["id"]] = (context, max(old[1], usage.get("output_tokens", 0)))
        content = message.get("content")
        for c in content if isinstance(content, list) else []:
            if not isinstance(c, dict):
                continue
            if c.get("type") == "tool_use":
                tool_names[c["id"]] = kind_of(c["name"], c.get("input") or {})
            elif c.get("type") == "tool_result" and c.get("tool_use_id") in tool_names:
                kinds[tool_names[c["tool_use_id"]]] += text_len(c.get("content"))
    values = list(turns.values())
    return {
        "start": (start or "")[:10],
        "turns": len(values),
        "first": values[0][0] if values else 0,
        "peak": max((v[0] for v in values), default=0),
        "context": sum(v[0] for v in values),
        "output": sum(v[1] for v in values),
    }


def compact(n):
    if n >= 1_000_000:
        return f"{n / 1_000_000:.1f}M"
    return f"{n / 1000:.0f}k" if n >= 1000 else str(n)


def table(rows):
    print(f"{'date':10} {'turns':>5} {'first':>6} {'peak':>6} {'context':>8} {'output':>7}  name")
    for s in rows:
        print(
            f"{s['start']:10} {s['turns']:>5} {compact(s['first']):>6} {compact(s['peak']):>6} "
            f"{compact(s['context']):>8} {compact(s['output']):>7}  {s['name']}"
        )


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--since", default="")
    ap.add_argument("--last", type=int, default=10)
    ap.add_argument("--tools", action="store_true")
    args = ap.parse_args()

    base = project_dir()
    if not os.path.isdir(base):
        print(f"no transcripts at {base}", file=sys.stderr)
        return 1

    kinds = collections.Counter()
    sessions = []
    for path in glob.glob(base + "/*.jsonl"):
        s = summarise(path, collections.Counter())
        s["name"] = os.path.basename(path)[:8]
        sessions.append(s)
    agents = []
    for path in glob.glob(base + "/*/subagents/*.jsonl"):
        s = summarise(path, collections.Counter())
        meta = path.replace(".jsonl", ".meta.json")
        info = json.load(open(meta)) if os.path.exists(meta) else {}
        s["name"] = f"{info.get('agentType', '?')}: {info.get('description', '')}"
        agents.append(s)

    keep = lambda rows: sorted(
        (s for s in rows if s["turns"] and s["start"] >= args.since), key=lambda s: s["start"]
    )[-args.last :]
    sessions, agents = keep(sessions), keep(agents)
    table(sessions)
    if agents:
        print("\nsubagents")
        table(agents)

    if args.tools:
        names = {s["name"] for s in sessions}
        paths = [p for p in glob.glob(base + "/*.jsonl") if os.path.basename(p)[:8] in names]
        paths += glob.glob(base + "/*/subagents/*.jsonl")
        for path in paths:
            summarise(path, kinds)
        total = sum(kinds.values()) or 1
        print("\ntool output (subagents not filtered by date)")
        for kind, chars in kinds.most_common(8):
            print(f"{kind:12} {compact(chars):>6} chars {100 * chars // total:>3}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
