#!/usr/bin/env python3
"""Run the recomp headless in virtual time through the attract demo and dump the race state once
per demo frame (clock: frames run of the demo task 0x20A150, pool slot 2 +8; HUD TOTAL TIME stays 0 in the demo): the 24 car objects.

usage: rt_attract_dump.py --app <RoadTrip binary> --data <data dir (disc + game)> --out <file>
       [--frames N] [--env KEY=VAL ...]
Record format (little endian): u32 vblank, u32 demo clock, u32 task function, CARS_LEN bytes.
No game data is written into the repo; keep --out outside it.
"""
import argparse
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests/regression"))
from rtharness import Game  # noqa: E402

MODE, TOTAL, COUNT = 0x33590A, 0x177A138, 0x177A13C  # demo task (0x20A150) frames run, its function
CARS, STRIDE, N = 0x177ACE0, 0x270, 24
CARS_LEN = STRIDE * N


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", required=True)
    ap.add_argument("--data", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--frames", type=int, default=3000)
    ap.add_argument("--env", action="append", default=[])
    ap.add_argument("--work", default=None)
    a = ap.parse_args()
    env = dict(e.split("=", 1) for e in a.env)
    work = Path(a.work or tempfile.mkdtemp(prefix="rtdump_"))
    out = open(a.out, "wb")
    with Game(Path(a.data), work, app=Path(a.app), env=env, speed="max") as g:
        reads = [(MODE, 1), (TOTAL, 4), (COUNT, 4), (CARS, CARS_LEN)]
        last, n, started = None, 0, False
        for _ in range(60 * 150):  # up to 150 s of game
            d = g.step(1, reads=reads)
            mode, total, count = d[0][0], struct.unpack("<I", d[1])[0], struct.unpack("<I", d[2])[0]
            if mode != 8 or count != 0x20A150 or total == last or d[3][:12] == bytes(12):
                continue
            if not started:
                print(f"demo race seen at vblank {g.vblank}, total_time {total}", flush=True)
                started = True
            last = total
            out.write(struct.pack("<III", g.vblank, total, count) + d[3])
            n += 1
            if n >= a.frames:
                break
        print(f"wrote {n} frames, last total_time {last}, vblank {g.vblank}")
    out.close()


if __name__ == "__main__":
    main()
