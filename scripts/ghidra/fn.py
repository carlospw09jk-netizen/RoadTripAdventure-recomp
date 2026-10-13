#!/usr/bin/env python3
"""Print a function's pseudocode, callers and callees from scripts/ghidra/decompile.py's export.

    python3 scripts/ghidra/fn.py 0x2263B8            # address (inside a function is fine)
    python3 scripts/ghidra/fn.py vifOpenCamera       # a name (ours or Ghidra's FUN_...)
    python3 scripts/ghidra/fn.py 0x227048 --graph    # callers/callees only
    python3 scripts/ghidra/fn.py --find 'sceDmaSend' # functions whose pseudocode matches a regex
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load(out: Path) -> dict:
    p = out / "callgraph.json"
    if not p.exists():
        sys.exit(f"{p} missing: run scripts/ghidra/decompile.py first")
    return {int(a, 16): v for a, v in json.loads(p.read_text()).items()}


def resolve(graph: dict, key: str) -> int:
    try:
        a = int(key, 16)
    except ValueError:
        hits = [x for x, v in graph.items() if v["name"] == key] or \
               [x for x, v in graph.items() if key.lower() in v["name"].lower()]
        if not hits:
            sys.exit(f"no function named {key}")
        if len(hits) > 1:
            print("several match: " + ", ".join(f"0x{x:X} {graph[x]['name']}" for x in hits[:20]))
        return hits[0]
    if a in graph:
        return a
    for x, v in graph.items():
        if x <= a <= int(v["end"], 16):
            return x
    sys.exit(f"no function at 0x{a:X}")


def label(graph: dict, a: str) -> str:
    v = graph.get(int(a, 16))
    return f"{a} {v['name']}" if v else a


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("funcs", nargs="*")
    ap.add_argument("--out", default=str(ROOT / "build/decomp"))
    ap.add_argument("--graph", action="store_true", help="callers and callees only")
    ap.add_argument("--find", help="list functions whose pseudocode matches this regex")
    args = ap.parse_args()
    out = Path(args.out)
    graph = load(out)

    if args.find:
        rx = re.compile(args.find)
        for a in sorted(graph):
            p = out / "fn" / f"{a:08x}.c"
            if p.exists() and rx.search(p.read_text(errors="replace")):
                print(f"0x{a:08X} {graph[a]['name']}")
        return

    for key in args.funcs:
        a = resolve(graph, key)
        v = graph[a]
        if not args.graph:
            p = out / "fn" / f"{a:08x}.c"
            print(p.read_text(errors="replace") if p.exists() else f"(no pseudocode for 0x{a:08X})")
        print(f"// callers of 0x{a:08X} {v['name']}:")
        for c in v["callers"]:
            print("//   " + label(graph, c))
        print("// callees:")
        for c in v["callees"]:
            print("//   " + label(graph, c))
        print()


if __name__ == "__main__":
    main()
