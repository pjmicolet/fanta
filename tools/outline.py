#!/usr/bin/env python3
"""Print a C++ file's types and functions with line ranges, to read only what's needed.

Usage: tools/outline.py FILE [FILE...]

Output lines are START-END NAME; pass the range to `sed -n START,ENDp`. This is a
brace-matching approximation, not a parser. Function declarations show as a single line.
"""
import re
import sys

TYPE_RE = re.compile(r"\b(struct|class|enum|union)\b")
SKIP_RE = re.compile(r"^(using|typedef|friend|return|public|private|protected|static_assert)\b")
MIN_TABLE_LINES = 5


def blank_noncode(src):
    """Blank comments, strings and preprocessor lines, keeping offsets and newlines."""
    out = list(src)
    i, n = 0, len(src)

    def wipe(a, b):
        for k in range(a, min(b, n)):
            if out[k] != "\n":
                out[k] = " "

    while i < n:
        c = src[i]
        if src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            wipe(i, j)
            i = j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            wipe(i, j)
            i = j
        elif c == "#" and src[src.rfind("\n", 0, i) + 1 : i].strip() == "":
            j = i
            while True:
                j = src.find("\n", j)
                if j < 0:
                    j = n
                    break
                if src[j - 1] != "\\":
                    break
                j += 1
            wipe(i, j)
            i = j
        elif c == '"' and re.search(r"R$", src[max(0, i - 1) : i]):
            m = re.match(r'"([^(\s]*)\(', src[i:])
            end = src.find(")" + m.group(1) + '"', i) if m else -1
            j = n if end < 0 else end + len(m.group(1)) + 2
            wipe(i + 1, j - 1)
            i = j
        elif c in "\"'":
            # A quote between digits is a C++14 digit separator (0x0200'0000).
            if c == "'" and i and src[i - 1].isalnum() and src[i + 1 : i + 2].isalnum():
                i += 1
                continue
            j = i + 1
            while j < n and src[j] != c and src[j] != "\n":
                j += 2 if src[j] == "\\" else 1
            wipe(i + 1, j)
            i = j + 1
        else:
            i += 1
    return "".join(out)


def match_brace(code, start):
    depth = 0
    for i in range(start, len(code)):
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return len(code) - 1


def outline(path):
    src = open(path, errors="replace").read()
    code = blank_noncode(src)
    line_of = lambda pos: code.count("\n", 0, pos) + 1
    rows = []

    def show(a, b, indent, text):
        text = " ".join(text.split())
        text = text[:97] + "..." if len(text) > 100 else text
        span = str(line_of(a)) if line_of(a) == line_of(b) else f"{line_of(a)}-{line_of(b)}"
        rows.append((span, indent, text))

    def scan(i, end, indent):
        head = i
        while i < end:
            c = code[i]
            if c == ";":
                text = code[head:i].strip()
                # Keep declarations; a multi-line macro call or field isn't worth a row.
                if "(" in text and "=" not in text and not SKIP_RE.match(text):
                    a = head + len(code[head:i]) - len(code[head:i].lstrip())
                    show(a, i, indent, text)
                head = i = i + 1
            elif c in ":" and code[head:i].strip() in ("public", "private", "protected"):
                head = i = i + 1
            elif c == "{":
                close = match_brace(code, i)
                text = code[head:i].strip()
                # Inside open parens this is a default argument, not a body.
                if text.count("(") > text.count(")"):
                    i = close + 1
                    continue
                a = head + len(code[head:i]) - len(code[head:i].lstrip())
                if re.match(r"^(inline\s+)?namespace\b", text) or text.startswith('extern "C"'):
                    show(a, close, indent, text)
                    scan(i + 1, close, indent + 1)
                elif TYPE_RE.search(text) and "(" not in text.split("{")[0].replace("template", ""):
                    show(a, close, indent, text)
                    scan(i + 1, close, indent + 1)
                elif TYPE_RE.search(text) and text.startswith("template") and "(" not in text:
                    show(a, close, indent, text)
                    scan(i + 1, close, indent + 1)
                elif "=" in text.replace("operator", "").split("(")[0] and "operator" not in text:
                    # An initialised table or lambda: only worth a row if it's long.
                    if line_of(close) - line_of(a) >= MIN_TABLE_LINES:
                        show(a, close, indent, text.split("=")[0])
                elif "(" in text:
                    # A constructor's brace initialisers (: a{1}, b{2} {) are part of its body.
                    while re.match(r"^\s*,", code[close + 1 :]) or (
                        code[close + 1 :].lstrip().startswith("{")
                    ):
                        nxt = code.find("{", close + 1)
                        close = match_brace(code, nxt)
                    show(a, close, indent, text)
                elif text and line_of(close) - line_of(a) >= MIN_TABLE_LINES:
                    show(a, close, indent, text)
                head = i = close + 1
                # Skip a trailing `;` or declarator such as `} name;`.
                m = re.match(r"\s*\w*\s*;", code[i:])
                if m:
                    head = i = i + m.end()
            else:
                i += 1

    scan(0, len(code), 0)
    width = max((len(r[0]) for r in rows), default=0)
    print(f"{path} ({src.count(chr(10))} lines)")
    for span, indent, text in rows:
        print(f"{span:>{width}}  {'  ' * indent}{text}")


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        return 0 if len(sys.argv) > 1 else 2
    for path in sys.argv[1:]:
        outline(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
