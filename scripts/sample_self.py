#!/usr/bin/env python3
"""Self time per function of one thread from a macOS `sample` file (call-graph section).

  sample_self.py sample.txt [thread-name-substring] [top N]

`sample` prints the top-of-stack ranking over all threads; this rebuilds the tree of one thread
(default GameThread) and attributes each node's samples less its children's to the node, so a
function's own time and what it spends in the scheduler's accounting are apart.
Groups: functions of the game library (libroadtrip_game) are summed under "game code".
"""
from __future__ import annotations

import re
import sys
from collections import Counter

LINE = re.compile(r"^([ +!:|]*?)(\d+) (.*?)(?:  \(in ([^)]*)\))?(?: \+ [\d,.]+)?(?:  \[0x[^\]]*\])?$")


def main() -> None:
    path = sys.argv[1]
    want = sys.argv[2] if len(sys.argv) > 2 else "GameThread"
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    lines = open(path, errors="replace").read().splitlines()
    start = next(i for i, l in enumerate(lines) if re.search(r"Thread_\w+.*" + re.escape(want), l) and l.strip()[0].isdigit())
    stack: list[tuple[int, int, str]] = []  # (indent, count, name)
    self_time: Counter = Counter()
    children_sum: dict[int, int] = {}
    nodes: list[tuple[int, int, str, str]] = []
    for l in lines[start + 1:]:
        if l.strip() == "" or l.startswith("Total number") or l.startswith("Sort by") or re.match(r"^    \d+ Thread_", l):
            break
        m = LINE.match(l)
        if not m:
            continue
        indent = len(m.group(1))
        nodes.append((indent, int(m.group(2)), m.group(3).strip(), m.group(4) or ""))
    # the first line is the thread itself (indent 4); its children start deeper
    parent_stack: list[int] = []
    child_total = [0] * len(nodes)
    for i, (indent, count, name, lib) in enumerate(nodes):
        while parent_stack and nodes[parent_stack[-1]][0] >= indent:
            parent_stack.pop()
        if parent_stack:
            child_total[parent_stack[-1]] += count
        parent_stack.append(i)
    total = 0
    by_name: Counter = Counter()
    by_lib: Counter = Counter()
    for i, (indent, count, name, lib) in enumerate(nodes):
        own = count - child_total[i]
        if own <= 0:
            continue
        key = re.sub(r"\(.*", "", name) if "sub_" not in name else re.sub(r"_0x[0-9a-f]+.*", "", name)
        by_name[key] += own
        by_lib["game code" if "roadtrip_game" in lib else (lib or "?")] += own
        total += own
    print(f"{want}: {total} samples")
    for lib, n in by_lib.most_common(8):
        print(f"  {n:6d} {100 * n / total:5.1f}%  [{lib}]")
    print()
    for name, n in by_name.most_common(top):
        print(f"  {n:6d} {100 * n / total:5.1f}%  {name[:110]}")


if __name__ == "__main__":
    main()
