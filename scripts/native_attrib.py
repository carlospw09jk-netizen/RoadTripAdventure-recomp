#!/usr/bin/env python3
"""Names the renderers behind RT_NATIVE_DRAW_LOG=1's per-chain lines.

    python3 scripts/native_attrib.py <app.log> [--top 20] [--decomp build/decomp]

The app logs, every 5 s, the top 20 model chains by draws: where the chain's vertex block came from
(a region of guest memory), the vifCallModel (0x227048) call whose chain starts at or before it,
and that call's return address (the renderer). This adds the renderer's name: the function in
config/game_symbols.toml (or the Ghidra export's call graph, when build/decomp exists) that
contains the return address. Totals over all windows in the log. Local debug output: nothing here
is game data, but keep the log out of git.
"""
from __future__ import annotations

import argparse
import json
import re
import tomllib
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LINE = re.compile(r"\[native\]\s+chain 0x([0-9a-f]+) \(([^)]*)\) call 0x([0-9a-f]+) ra 0x([0-9a-f]+): gpu (\d+) cpu-ref (\d+) fallback (\d+)"
                  r"(.*?), (\d+) verts, programs 0x([0-9a-f]+)(?:, (\d+) block)?")


def function_names(decomp: Path) -> list[tuple[int, int, str]]:
    funcs: list[tuple[int, int, str]] = []
    cg = decomp / "callgraph.json"
    if cg.exists():
        for addr, e in json.loads(cg.read_text()).items():
            funcs.append((int(addr, 16), int(e.get("end", addr), 16), e["name"]))
    names = tomllib.loads((ROOT / "config/game_symbols.toml").read_text()).get("functions", {})
    for addr, e in names.items():
        a = int(addr, 16) if isinstance(addr, str) else addr
        funcs = [f for f in funcs if f[0] != a] + [(a, a, e["name"])]
    return sorted(funcs)


def name_of(funcs, ra: int) -> str:
    best = None
    for a, end, name in funcs:
        if a <= ra and (best is None or a > best[0]):
            best = (a, end, name)
    return f"{best[2]}+0x{ra - best[0]:x}" if best else "?"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log")
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--decomp", default=str(ROOT / "build/decomp"))
    args = ap.parse_args()
    funcs = function_names(Path(args.decomp))
    rows = defaultdict(lambda: [0, 0, 0, 0, set(), "", 0])  # gpu, cpuref, fallback, verts, programs, region, blocks
    reasons = defaultdict(lambda: defaultdict(int))
    for line in Path(args.log).read_text(errors="replace").splitlines():
        m = LINE.search(line)
        if not m:
            continue
        chain, region, call, ra, gpu, cpu, fb, rest, verts, progs, blocks = m.groups()
        key = (int(chain, 16), int(ra, 16))
        r = rows[key]
        r[0] += int(gpu); r[1] += int(cpu); r[2] += int(fb); r[3] += int(verts)
        r[4].add(int(progs, 16)); r[5] = region; r[6] = max(r[6], int(blocks or 0))
        for name, n in re.findall(r"(\w[\w-]*)=(\d+)", rest):
            reasons[key][name] += int(n)
    by_renderer = defaultdict(lambda: [0, 0, 0])
    for (chain, ra), r in rows.items():
        by_renderer[name_of(funcs, ra).split("+")[0]][0] += r[0]
        by_renderer[name_of(funcs, ra).split("+")[0]][1] += r[1]
        by_renderer[name_of(funcs, ra).split("+")[0]][2] += r[2]
    print("renderer                         gpu  cpu-ref  fallback")
    for name, (g, c, f) in sorted(by_renderer.items(), key=lambda kv: -sum(kv[1]))[: args.top]:
        print(f"{name:30s} {g:7d} {c:8d} {f:9d}")
    print("\nchain       region               renderer                       gpu  cpu-ref  fallback  blocks  programs  fallback reasons")
    for (chain, ra), r in sorted(rows.items(), key=lambda kv: -(kv[1][0] + kv[1][1] + kv[1][2]))[: args.top]:
        why = ", ".join(f"{k}={v}" for k, v in reasons[(chain, ra)].items())
        mask = 0
        for p in r[4]:
            mask |= p
        progs = "+".join(str(i) for i in range(8) if mask >> i & 1)
        print(f"0x{chain:07x}  {r[5]:20s} {name_of(funcs, ra):28s} {r[0]:6d} {r[1]:8d} {r[2]:9d} {r[6]:7d}  e{progs:7s}  {why}")


if __name__ == "__main__":
    main()
