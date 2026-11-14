#!/usr/bin/env python3
"""Compact digest of pending work, for the fanta-orchestrator agent and /fanta-triage.

Usage:
  tools/triage.py                      digest: open issues in board order + inbox + unlinked friction
  tools/triage.py --issue N [N ...]    full body of the given issues (drill-down)
  tools/triage.py --move N --after M   move issue N just after issue M on the Fanta board
  tools/triage.py --move N --top       move issue N to the top of the board

One line per issue: board position, number, status, milestone, labels, age/idle in days,
friction refs, links (parent, blocks / blocked-by from "blocked by #N" / "depends on #N" in
bodies), title. Bodies are left out; use --issue for them.
"""
import argparse
import datetime
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OWNER, REPO, PROJECT = "pjmicolet", "fanta", 1
DEP = re.compile(r"(?:blocked (?:by|on)|depends on|after)\s+#(\d+)", re.I)
REF = re.compile(r"#(\d+)")

QUERY = """
query($owner:String!,$n:Int!){ user(login:$owner){ projectV2(number:$n){ id
  items(first:100, orderBy:{field:POSITION,direction:ASC}){ nodes{ id
    status: fieldValueByName(name:"Status"){ ... on ProjectV2ItemFieldSingleSelectValue{ name } }
    content{ ... on Issue{ number state title body createdAt updatedAt
      milestone{ title } labels(first:10){ nodes{ name } } parent{ number } blockedBy(first:10){ nodes{ number } }
      repository{ name } } } } } } } }
"""


def gh(*args):
    r = subprocess.run(["gh", *args], capture_output=True, text=True, cwd=ROOT)
    if r.returncode:
        sys.exit(f"gh failed: {r.stderr.strip()}")
    return r.stdout


def board():
    out = gh("api", "graphql", "-f", f"query={QUERY}", "-F", f"owner={OWNER}", "-F", f"n={PROJECT}")
    proj = json.loads(out)["data"]["user"]["projectV2"]
    items = []
    for node in proj["items"]["nodes"]:
        c = node.get("content") or {}
        if c.get("repository", {}).get("name") != REPO:
            continue
        items.append({"item": node["id"], "status": (node.get("status") or {}).get("name", "-"), **c})
    return proj["id"], items


def off_board():
    """Open issues that aren't on the board at all."""
    out = gh("issue", "list", "-R", f"{OWNER}/{REPO}", "--state", "open", "--limit", "200",
             "--json", "number,title,labels,milestone,createdAt,updatedAt,body")
    return json.loads(out)


def friction():
    """Map issue number -> count of open friction entries pointing at it; plus unlinked entries."""
    path = os.path.join(ROOT, "docs", "tui_friction.md")
    counts, unlinked, section = {}, [], None
    for line in open(path, encoding="utf-8"):
        if line.startswith("## "):
            section = line[3:].strip()
            continue
        if section != "Open" or not re.match(r"\d{4}-\d\d-\d\d", line):
            continue
        if "fixed " in line or "wontfix" in line:
            continue
        if "→" in line:
            for n in REF.findall(line.split("→", 1)[1]):
                counts[int(n)] = counts.get(int(n), 0) + 1
        else:
            unlinked.append(" ".join(line.split()))
    return counts, unlinked


def inbox():
    path = os.path.join(ROOT, "docs", "inbox.md")
    if not os.path.exists(path):
        return []
    return [l.strip() for l in open(path, encoding="utf-8") if re.match(r"\d{4}-\d\d-\d\d", l)]


def days(ts, now):
    return (now - datetime.datetime.fromisoformat(ts.replace("Z", "+00:00"))).days


def digest():
    _, items = board()
    on_board = {i["number"] for i in items}
    extra = [i for i in off_board() if i["number"] not in on_board]
    opened = [i for i in items if i.get("state") == "OPEN"] + extra
    fric, unlinked = friction()
    now = datetime.datetime.now(datetime.timezone.utc)

    blocked_by = {i["number"]: sorted({int(n) for n in DEP.findall(i.get("body") or "")}
                                    | {b["number"] for b in (i.get("blockedBy") or {}).get("nodes", [])})
                  for i in opened}
    blocks = {}
    for n, deps in blocked_by.items():
        for d in deps:
            blocks.setdefault(d, []).append(n)

    print(f"# open issues: {len(opened)}  (pos # status ms labels age/idle fr links title)")
    for pos, i in enumerate(opened, 1):
        labels = i["labels"]["nodes"] if isinstance(i["labels"], dict) else i["labels"]
        lab = ",".join(l["name"] for l in labels if l["name"] not in ("enhancement",)) or "-"
        ms = (i.get("milestone") or {}).get("title", "-")
        n = i["number"]
        links = []
        if (i.get("parent") or {}).get("number"):
            links.append(f"parent#{i['parent']['number']}")
        links += [f"after#{d}" for d in blocked_by.get(n, [])]
        links += [f"blocks#{b}" for b in blocks.get(n, [])]
        pos_s = "off" if n not in on_board else str(pos)
        print(f"{pos_s:>3} #{n} {i.get('status', '-')} {ms} {lab} "
              f"{days(i['createdAt'], now)}d/{days(i['updatedAt'], now)}d "
              f"fr{fric.get(n, 0)} {' '.join(links) or '-'} | {i['title']}")
    if unlinked:
        print(f"\n# open friction with no issue: {len(unlinked)}")
        print("\n".join(unlinked))
    ib = inbox()
    if ib:
        print(f"\n# inbox: {len(ib)}")
        print("\n".join(ib))
    prs = gh("pr", "list", "-R", f"{OWNER}/{REPO}", "--state", "open", "--json", "number,title,body",
             "-q", '.[]|"PR #\\(.number) \\(.title) [\\((.body // "") | [scan("(?i)(?:fixes|closes) #[0-9]+")] | join(","))]"').strip()
    if prs:
        print(f"\n# open PRs\n{prs}")


def show(numbers):
    for n in numbers:
        print(gh("issue", "view", str(n), "-R", f"{OWNER}/{REPO}", "--json",
                 "number,title,labels,milestone,body,comments", "-q",
                 '"#\\(.number) \\(.title) [\\(.labels|map(.name)|join(","))] \\(.milestone.title // "-")\\n\\(.body)'
                 '\\(.comments|map("\\n--- \\(.author.login): \\(.body)")|join(""))\\n"'))


def move(n, after, top):
    pid, items = board()
    ids = {i["number"]: i["item"] for i in items}
    if n not in ids or (after and after not in ids):
        sys.exit("issue not on the board")
    after_arg = [] if top else ["-F", f"after={ids[after]}"]
    gh("api", "graphql", "-f",
       "query=mutation($p:ID!,$i:ID!,$after:ID){updateProjectV2ItemPosition(input:{projectId:$p,itemId:$i,afterId:$after}){clientMutationId}}",
       "-F", f"p={pid}", "-F", f"i={ids[n]}", *after_arg)
    print(f"moved #{n} " + ("to top" if top else f"after #{after}"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--issue", nargs="+", type=int)
    ap.add_argument("--move", type=int)
    ap.add_argument("--after", type=int)
    ap.add_argument("--top", action="store_true")
    a = ap.parse_args()
    if a.issue:
        show(a.issue)
    elif a.move:
        if not (a.after or a.top):
            sys.exit("--move needs --after M or --top")
        move(a.move, a.after, a.top)
    else:
        digest()


if __name__ == "__main__":
    main()
