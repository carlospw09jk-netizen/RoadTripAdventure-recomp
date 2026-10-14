#!/usr/bin/env python3
"""Samples the app with macOS `sample` while a scene runs live (native-draw profiling).

    native_sample.py <town|race> <outfile> [--app APP] [--base DIR] [--checkpoint DIR] [--state town.rtstate]
                     [--seconds 10] [--env K=V ...]

town: loads the state over the live test socket (as scripts/gs_perf.py does) and drives in circles;
race: the scripted Quick Race (waits 60 s of play first). Hardware GS, 60 Hz, 4x. One GPU job: take
rt-gpu.lock first. Output is `sample`'s call-graph text (per thread).
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tests/regression"))
sys.path.insert(0, str(REPO / "scripts"))
from rtharness import DEFAULT_APP, Game  # noqa: E402
import gs_perf  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scene", choices=["town", "race"])
    ap.add_argument("out")
    ap.add_argument("--app", default=str(DEFAULT_APP))
    ap.add_argument("--base", default=str(REPO / "build/regression/base"))
    ap.add_argument("--state", default="")
    ap.add_argument("--seconds", type=int, default=10)
    ap.add_argument("--env", action="append", default=[])
    args = ap.parse_args()
    work = Path(args.out).with_suffix("")
    work.mkdir(parents=True, exist_ok=True)
    extra = dict(e.split("=", 1) for e in args.env)
    if args.scene == "town":
        import numpy as np
        from rtharness.town import TownDriver, TownMap, door_quads
        env = dict(gs_perf.live_env("hw"), **extra)
        g = Game(Path(args.base), work / "run", env=env, settings=gs_perf.settings(60, "none"), app=Path(args.app)).start()
        try:
            g._call("load_state", path=args.state, max_vblanks=600)
            TownDriver(g, TownMap(np.zeros((800, 800), np.float32)), door_quads(g, gs_perf.TOWN), town=gs_perf.TOWN).park_doors()
            g.pad("cross", lx=60)
            time.sleep(15)
            subprocess.run(["sample", str(g.proc.pid), str(args.seconds), "-file", args.out], check=False)
        finally:
            g.close()
    else:
        data = work / "data"
        data.mkdir(parents=True, exist_ok=True)
        for n in ("disc", "game"):
            gs_perf.link_dir(Path(args.base) / n, data / n)
        (data / "saves" / "mc0").mkdir(parents=True, exist_ok=True)
        (data / "saves" / "mc1").mkdir(exist_ok=True)
        (data / "settings.toml").write_text(gs_perf.settings(60, "none"))
        env = dict(os.environ, RT_DATA_DIR=str(data), RT_INPUT_SCRIPT=gs_perf.QUICK_RACE, RT_EXIT_AT_VBLANK=str(95 * 60),
                   RT_GS_BACKEND="hw", **extra)
        with open(work / "app.log", "w") as log:
            p = subprocess.Popen([args.app], env=env, cwd=work, stdout=log, stderr=subprocess.STDOUT)
            try:
                time.sleep(70)
                subprocess.run(["sample", str(p.pid), str(args.seconds), "-file", args.out], check=False)
            finally:
                p.kill()
                p.wait()


if __name__ == "__main__":
    main()
