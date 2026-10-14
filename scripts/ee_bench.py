#!/usr/bin/env python3
"""EE codegen benchmark: a fixed Quick Race, lockstep and uncapped, with the guest's state hashes.

  ee_bench.py --app <RoadTrip.app/Contents/MacOS/RoadTrip> --base <data dir: disc/ + game/> [--vblanks 1800]

What it measures (the game's drawing is off: RT_RENDER=0, so what is timed is the EE, the IOP, VU1
and the GIF, not the GS): wall seconds for the timed vblanks (the game thread is the long pole) and the process's CPU seconds. What it checks: the state hashes (every chunk) at every
100th timed vblank, written to --out as JSON, so two builds can be compared for identical guest
behaviour (`ee_bench.py --diff a.json b.json`).

Navigation is the regression harness's (rtharness.quick_race); the race itself is the gas held with
the stick steering in a fixed slow sine, as test_save_states does.
"""
from __future__ import annotations

import argparse
import json
import math
import shutil
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tests/regression"))
from rtharness import BUTTONS, Game, seconds  # noqa: E402
from rtharness.quick_race import open_course, start_race  # noqa: E402


def cpu_seconds(pid: int) -> float:
    import psutil

    t = psutil.Process(pid).cpu_times()  # per-thread times need root on macOS
    return t.user + t.system


def run(args) -> dict:
    work = Path(args.work)
    shutil.rmtree(work, ignore_errors=True)
    game = Game(Path(args.base), work, app=Path(args.app), speed="max", state_hash=False, render=False,
                env={"RT_GS_BACKEND": "pgs", "RT_THREAD_CPU": "1"}).start()
    try:
        if args.scenario == "race":
            open_course(game, 0)
            start_race(game)
            game.pad("cross")
            game.run(seconds(8))
        # "boot": nothing pressed, the title and its attract demo from power on
        pid = game.proc.pid
        before = cpu_seconds(pid)
        sampler = None
        if args.sample:  # macOS `sample`: the call tree per thread, for where the game thread's time goes
            import subprocess

            sampler = subprocess.Popen(["sample", str(pid), str(args.sample), "-file", args.sample_file],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        t0 = time.time()
        hashes = {}
        done = 0
        while done < args.vblanks:
            n = min(30, args.vblanks - done)
            if args.scenario == "race":
                mask = 0xFFFF & ~BUTTONS["cross"]
                lx = int(128 + 100 * math.sin(done / 37.0))
                game._call("pad", buttons=mask, lx=lx, ly=128, rx=128, ry=128)
            game.run(n)
            done += n
            if done % 300 == 0:
                hashes[game.vblank] = game._call("state_hash")["hashes"]
        if sampler:
            sampler.wait()
        wall = time.time() - t0
        cpu = cpu_seconds(pid) - before
        return {
            "vblanks": args.vblanks,
            "wall_s": round(wall, 3),
            "process_cpu_s": round(cpu, 3),
            "hashes": hashes,
        }
    finally:
        game.close()
        log = (work / "app.log").read_text(errors="replace") if (work / "app.log").exists() else ""
        for line in log.splitlines():
            if line.startswith("[thread-cpu]"):
                print(line)


def diff(a: str, b: str) -> int:
    ja, jb = json.loads(Path(a).read_text()), json.loads(Path(b).read_text())
    bad = 0
    for vb, ha in ja["hashes"].items():
        hb = jb["hashes"].get(vb)
        if hb != ha:
            bad += 1
            keys = sorted(k for k in ha if ha.get(k) != (hb or {}).get(k))
            print(f"vblank {vb}: differs in {keys}")
    print("identical" if not bad and ja["hashes"].keys() == jb["hashes"].keys() else f"{bad} differing samples")
    return 1 if bad else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--app")
    ap.add_argument("--base")
    ap.add_argument("--work", default=str(REPO / "build/scratch/ee_bench/run"))
    ap.add_argument("--vblanks", type=int, default=1800)
    ap.add_argument("--scenario", choices=["race", "boot"], default="race")
    ap.add_argument("--out")
    ap.add_argument("--sample", type=int, default=0, help="seconds of `sample` during the timed part")
    ap.add_argument("--sample-file", default="sample.txt")
    ap.add_argument("--diff", nargs=2)
    args = ap.parse_args()
    if args.diff:
        return diff(*args.diff)
    result = run(args)
    summary = {k: v for k, v in result.items() if k != "hashes"}
    print(json.dumps(summary))
    if args.out:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_text(json.dumps(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
