#!/usr/bin/env python3
"""Compare two attract-demo dumps (rt_attract_dump.py / pine_dump.py) frame by frame by demo clock.

usage: compare.py A.bin B.bin [--cars 0,1,2] [--detail N]
Prints, per car, the first clock at which the car's position (x,y,z at +0) differs, and the
position difference over time; with --detail the first differing words (offset, A, B) at the
first divergence of any car.
"""
import argparse
import math
import struct

STRIDE, N = 0x270, 24
REC = 12 + STRIDE * N


def load(path, shift=0):
    b = open(path, "rb").read()
    d = {}
    for i in range(len(b) // REC):
        v, t, c = struct.unpack_from("<III", b, i * REC)
        d[t + shift] = b[i * REC + 12:(i + 1) * REC]
    return d


def pos(blk, k):
    return struct.unpack_from("<3f", blk, k * STRIDE)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--detail", type=int, default=12)
    ap.add_argument("--cars", default="")
    ap.add_argument("--from", dest="start", type=int, default=0)
    ap.add_argument("--shiftb", type=int, default=0, help="B's clock minus A's clock for the same game frame")
    a = ap.parse_args()
    A, B = load(a.a), load(a.b, -a.shiftb)
    common = sorted(set(A) & set(B))
    common = [t for t in common if t >= a.start]
    print(f"A {len(A)} frames [{min(A)}..{max(A)}], B {len(B)} frames [{min(B)}..{max(B)}], common {len(common)}")
    if not common:
        return
    cars = [int(x) for x in a.cars.split(",") if x] or list(range(N))
    first = None
    ident = 0
    for t in common:
        if A[t] == B[t]:
            ident += 1
        elif first is None:
            first = t
    print(f"identical frames: {ident}/{len(common)}; first differing clock: {first}")
    if first is not None:
        ba, bb = A[first], B[first]
        words = [(o, struct.unpack_from('<I', ba, o)[0], struct.unpack_from('<I', bb, o)[0])
                 for o in range(0, len(ba), 4) if ba[o:o + 4] != bb[o:o + 4]]
        print(f"clock {first}: {len(words)} words differ; first {a.detail}:")
        for o, x, y in words[:a.detail]:
            k, off = divmod(o, STRIDE)
            fx, fy = struct.unpack('<f', struct.pack('<I', x))[0], struct.unpack('<f', struct.pack('<I', y))[0]
            print(f"  car {k} +0x{off:03X}: {x:08X} {y:08X}  ({fx:.9g} vs {fy:.9g})")
    # position error over time (all cars, max)
    print("clock  maxposdiff  car")
    marks = [t for t in common if (t - common[0]) % 300 == 0 or t == common[-1]]
    for t in marks:
        worst = (0.0, -1)
        for k in cars:
            pa, pb = pos(A[t], k), pos(B[t], k)
            if all(math.isfinite(x) for x in pa + pb):
                dd = math.dist(pa, pb)
                if dd > worst[0]:
                    worst = (dd, k)
        print(f"{t:6d}  {worst[0]:.6g}  {worst[1]}")
    # per-car first position divergence > thresholds
    for thr in (1e-6, 1e-3, 0.1, 1.0, 10.0):
        row = []
        for k in cars:
            for t in common:
                pa, pb = pos(A[t], k), pos(B[t], k)
                if all(math.isfinite(x) for x in pa + pb) and math.dist(pa, pb) > thr:
                    row.append((t, k))
                    break
        row.sort()
        print(f"first clock with a car position diff > {thr}: {row[:3] if row else 'none'}")


if __name__ == "__main__":
    main()
