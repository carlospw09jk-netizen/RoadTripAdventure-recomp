#!/usr/bin/env python3
"""Real-time GS comparison: the hardware GS against paraLLEl-GS (issue #19), windowed, as players run it.

  gs_perf.py town-state                          make <out>/town.rtstate (lockstep: test_save_states' town recipe)
  gs_perf.py run <race|town> <hw|pgs> <60|120> <upscaler> <tag>   one run; appends a JSON line to <out>/results.jsonl
  gs_perf.py matrix [--rounds 3] [--upscalers fsr1,metalfx_temporal] [--scenes race,town]
  gs_perf.py table                               medians per configuration (markdown)

race: tests/regression/test_perf.py's scripted Quick Race (RT_INPUT_SCRIPT, RT_EXIT_AT_VBLANK).
town: the town state loaded over the live test socket (rtharness), gas held and steering gently
      (doors parked off the map, as rtharness.town does), classic controls.
Each run: supersampling 4, frame generation re-rendered at 120. Measured: the [perf] windows
(RT_PERF_LOG) of the last 25 s, [pacing] (RT_PRESENT_DEBUG) device-lock waits, [gputime]
(RT_GPU_TIMES; paraLLEl-GS's own compute passes aren't in it) and the GPU's utilisation (macOS:
ioreg; NVIDIA: nvidia-smi; elsewhere none), less the idle level sampled before the run.

Data: --base (default build/regression/base: disc/ and game/, as the regression suite installs
them) is linked into a scratch data directory per run under --out (default
build/scratch/perf/gs_perf). Never the player's data directory.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tests/regression"))
from rtharness import DEFAULT_APP, Game, link_dir, seconds  # noqa: E402

QUICK_RACE = "10:start,13:down,15:cross,19:cross,23:cross,27:cross,31:cross,35:cross,39:cross,43:cross,47:cross:50"
MEASURE_S = 25
TOWN = 1  # Peach Town


def settings(hz: int, upscaler: str) -> str:
    return (f'[display]\nrefresh_rate = {hz}\nframe_mode = "rerender"\n'
            f'[quality]\nsupersampling = 4\nupscaler = "{upscaler}"\n[general]\nmenu_hint_shown = true\n')


def gpu_utilisation() -> float | None:
    try:
        if sys.platform == "darwin":
            out = subprocess.run(["ioreg", "-r", "-c", "IOAccelerator", "-d", "1", "-w0"],
                                 capture_output=True, text=True, timeout=5).stdout
            m = re.search(r'"Device Utilization %"=(\d+)', out)
            return float(m.group(1)) if m else None
        if shutil.which("nvidia-smi"):
            out = subprocess.run(["nvidia-smi", "--query-gpu=utilization.gpu", "--format=csv,noheader,nounits"],
                                 capture_output=True, text=True, timeout=5).stdout
            return float(out.split()[0])
    except (OSError, ValueError, IndexError, subprocess.TimeoutExpired):
        pass
    return None


def process_gpu_ns(pid: int) -> int | None:
    """macOS: the GPU time the process's Metal clients have used so far (ns; AGXDeviceUserClient)."""
    if sys.platform != "darwin":
        return None
    out = subprocess.run(["ioreg", "-r", "-c", "AGXDeviceUserClient", "-w0", "-d1"], capture_output=True, text=True).stdout
    total = 0
    for client in out.split("+-o "):
        if f'"pid {pid},' in client:
            total += sum(int(v) for v in re.findall(r'"accumulatedGPUTime"=(\d+)', client))
    return total


class GpuSampler(threading.Thread):
    """The GPU's utilisation (whole GPU) and, on macOS, the app's own GPU time, every 0.5 s."""
    def __init__(self):
        super().__init__(daemon=True)
        self.samples: list[tuple[float, float]] = []
        self.gpu_time: list[tuple[float, int]] = []
        self.pid = 0  # the app, once started
        self.stop = threading.Event()

    def run(self):
        while not self.stop.is_set():
            u = gpu_utilisation()
            if u is not None:
                self.samples.append((time.time(), u))
            if self.pid and (ns := process_gpu_ns(self.pid)) is not None:
                self.gpu_time.append((time.time(), ns))
            self.stop.wait(0.5)


def idle_gpu() -> float:
    """The GPU's utilisation with no game running (the desktop, other apps), subtracted."""
    vals = []
    for _ in range(6):
        if (v := gpu_utilisation()) is not None:
            vals.append(v)
        time.sleep(0.5)
    return statistics.median(vals) if vals else 0.0


def live_env(backend: str) -> dict:
    return {"RT_GS_BACKEND": backend, "RT_PERF_LOG": "1", "RT_GPU_TIMES": "1", "RT_PRESENT_DEBUG": "1",
            "RT_TIME": "real", "RT_SPEED": "1", "RT_HEADLESS": "0", "RT_RENDER": "1", "RT_TEST_LIVE": "1",
            "RT_PROGRESSIVE_FIELDS": "1", "RT_GS_SSAA": "4", "RT_GAME_OPTIONS": "0", "RT_CONTROL_SCHEME": "classic"}


def run_race(args, work: Path, backend: str, hz: int, upscaler: str, sampler: GpuSampler) -> str:
    data = work / "data"
    data.mkdir(parents=True)
    for n in ("disc", "game"):
        link_dir(Path(args.base) / n, data / n)
    (data / "saves" / "mc0").mkdir(parents=True)
    (data / "saves" / "mc1").mkdir()
    (data / "settings.toml").write_text(settings(hz, upscaler))
    env = dict(os.environ, RT_DATA_DIR=str(data), RT_INPUT_SCRIPT=QUICK_RACE, RT_EXIT_AT_VBLANK=str(95 * 60),
               RT_GS_BACKEND=backend, RT_PERF_LOG="1", RT_GPU_TIMES="1", RT_PRESENT_DEBUG="1")
    with open(work / "app.log", "w") as log:
        p = subprocess.Popen([str(args.app)], env=env, cwd=work, stdout=log, stderr=subprocess.STDOUT)
        sampler.pid = p.pid
        try:
            p.wait(timeout=600)
        finally:
            if p.poll() is None:
                p.kill()
                p.wait()
    return (work / "app.log").read_text(errors="replace")


def run_town(args, work: Path, backend: str, hz: int, upscaler: str, sampler: GpuSampler) -> str:
    import numpy as np
    from rtharness.town import TownDriver, TownMap, door_quads

    state = Path(args.out) / "town.rtstate"
    if not state.exists():
        sys.exit(f"{state} missing: run 'gs_perf.py town-state' first")
    g = Game(Path(args.base), work, env=live_env(backend), settings=settings(hz, upscaler), app=Path(args.app)).start()
    sampler.pid = g.proc.pid
    try:
        g._call("load_state", path=str(state), max_vblanks=600)
        TownDriver(g, TownMap(np.zeros((800, 800), np.float32)), door_quads(g, TOWN), town=TOWN).park_doors()
        g.pad("cross", lx=60)  # gas, a gentle left: circles among the buildings and the traffic
        time.sleep(15 + MEASURE_S)
    finally:
        g.close()
    return (work / "app.log").read_text(errors="replace")


def parse(log: str, t_end: float, gpu: list, idle: float, hz: int, gpu_time: list) -> dict:
    perf = [l for l in log.splitlines() if l.startswith("[perf]")]
    win = perf[-(MEASURE_S * 2 + 2):-2]  # 0.5 s windows: the last MEASURE_S s, less the exit
    num = lambda pat, l: float(m.group(1)) if (m := re.search(pat, l)) else float("nan")  # noqa: E731
    rows = [dict(game=num(r"\] ([\d.]+) fps", l), shown=num(r"shown ([\d.]+)", l), worst=num(r"worst ([\d.]+)", l),
                 vu1=num(r"VU1 (\d+)%", l), gs=num(r"GS (\d+)%", l), cpu=num(r"CPU (\d+)%", l),
                 skipped=num(r"\((\d+) skipped\)", l)) for l in win]
    if not rows:
        return {"error": "no [perf] lines"}
    res = {k: statistics.median(r[k] for r in rows) for k in rows[0]}
    res["under57"] = sum(r["game"] < 57 for r in rows)
    res["shown_low"] = sum(r["shown"] < 0.95 * hz for r in rows)  # frame generation paused or frames lost
    res["skipped"] = sum(r["skipped"] for r in rows)
    pacing = [l for l in log.splitlines() if l.startswith("[pacing]")][-MEASURE_S // 2:]
    waits = [float(m.group(1)) for l in pacing if (m := re.search(r"waited for ([\d.]+)", l))]
    res["lock_wait_ms"] = statistics.median(waits) if waits else float("nan")
    blocks, cur = [], {}
    for l in log.splitlines():
        if m := re.match(r"\[gputime\] (.+): ([\d.]+) ms each, ([\d.]+) per frame context", l):
            if m.group(1) in cur:
                blocks.append(cur)
                cur = {}
            cur[m.group(1)] = float(m.group(2)) * float(m.group(3))
    if cur:
        blocks.append(cur)
    blocks = blocks[-MEASURE_S // 2:]
    res["gpu_ms"] = statistics.median(sum(b.values()) for b in blocks) if blocks else float("nan")
    util = [u for t, u in gpu if t_end - MEASURE_S - 1 <= t <= t_end - 1]
    res["gpu_util"] = statistics.median(util) - idle if util else float("nan")
    # The app's own GPU time over the window, in ms per second (= % of one GPU, x10).
    span = [(t, ns) for t, ns in gpu_time if t_end - MEASURE_S - 1 <= t <= t_end - 1]
    res["gpu_ms_per_s"] = ((span[-1][1] - span[0][1]) / 1e6 / (span[-1][0] - span[0][0])
                           if len(span) > 1 and span[-1][0] > span[0][0] else float("nan"))
    return res


def cmd_town_state(args):
    from rtharness.adventure import FIELDS, continue_to_factory, scene

    edits = {FIELDS["location"]["offset"]: bytes([TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    g = Game(Path(args.base), Path(args.out) / "runs" / "town-state", checkpoint=Path(args.checkpoint), app=Path(args.app),
             render=True, progress_edits=edits).start()
    try:
        continue_to_factory(g)
        for _ in range(4):  # Change parts / Race / Save data / Quit game / Drive around town
            g.press("down")
        g.press("cross")
        g.run(seconds(2))
        g.press("cross")  # "Come again!"
        g.run(seconds(10))
        assert scene(g) == 2, "driving in town"
        g.pad("cross")
        g.run(seconds(5))
        g.release()
        g._call("save_state", path=str(Path(args.out) / "town.rtstate"), max_vblanks=600)
    finally:
        g.close()


def cmd_run(args, scene, backend, hz, upscaler, tag):
    work = Path(args.out) / "runs" / f"{scene}_{backend}_{hz}_{upscaler}_{tag}"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    idle = idle_gpu()
    sampler = GpuSampler()
    sampler.start()
    log = (run_race if scene == "race" else run_town)(args, work, backend, int(hz), upscaler, sampler)
    t_end = time.time()
    sampler.stop.set()
    sampler.join()
    res = dict(scene=scene, backend=backend, hz=int(hz), upscaler=upscaler, tag=tag,
               **parse(log, t_end, sampler.samples, idle, int(hz), sampler.gpu_time))
    with open(Path(args.out) / "results.jsonl", "a") as f:
        f.write(json.dumps(res) + "\n")
    print(json.dumps(res), flush=True)


KEYS = ["shown", "game", "worst", "under57", "shown_low", "skipped", "gs", "vu1", "cpu", "gpu_util", "gpu_ms_per_s", "gpu_ms",
        "lock_wait_ms"]


def cmd_table(args):
    rows = [json.loads(l) for l in (Path(args.out) / "results.jsonl").read_text().splitlines() if l.strip()]
    groups: dict = {}
    for r in rows:
        if "error" not in r:
            groups.setdefault((r["scene"], r["hz"], r["upscaler"], r["backend"]), []).append(r)
    print("| scene | Hz | upscaler | GS | runs | " + " | ".join(KEYS) + " |")
    print("|" + "---|" * (5 + len(KEYS)))
    for (s, hz, up, b), rs in sorted(groups.items()):
        vals = [statistics.median(r.get(k, float("nan")) for r in rs) for k in KEYS]
        print(f"| {s} | {hz} | {up} | {b} | {len(rs)} | " + " | ".join(f"{v:.1f}" for v in vals) + " |")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["town-state", "run", "matrix", "table"])
    ap.add_argument("params", nargs="*")
    ap.add_argument("--app", default=str(DEFAULT_APP))
    ap.add_argument("--base", default=str(REPO / "build/regression/base"))
    ap.add_argument("--checkpoint", default=str(REPO / "build/regression/checkpoints/adventure_first_save"))
    ap.add_argument("--out", default=str(REPO / "build/scratch/perf/gs_perf"))
    ap.add_argument("--rounds", type=int, default=3)
    ap.add_argument("--scenes", default="race,town")
    ap.add_argument("--rates", default="60,120")
    ap.add_argument("--upscalers", default="fsr1,metalfx_temporal" if sys.platform == "darwin" else "fsr1,fsr3")
    args = ap.parse_args()
    Path(args.out).mkdir(parents=True, exist_ok=True)
    if args.cmd == "town-state":
        cmd_town_state(args)
    elif args.cmd == "run":
        cmd_run(args, *args.params)
    elif args.cmd == "table":
        cmd_table(args)
    else:  # one run of each configuration per round, the two GSs alternating
        for r in range(1, args.rounds + 1):
            for scene in args.scenes.split(","):
                for hz in args.rates.split(","):
                    for up in args.upscalers.split(","):
                        for backend in ("pgs", "hw"):
                            cmd_run(args, scene, backend, hz, up, f"r{r}")
        cmd_table(args)


if __name__ == "__main__":
    main()
