#!/usr/bin/env python3
"""Per-frame word-level comparison of dumps: for the given clocks (of A; B's clock = A's + shift),
how many 32-bit words of the 24 car objects differ, and the ulp size of the differences.

usage: words.py A.bin B.bin shift clock [clock ...]
"""
import struct
import sys

import compare as C

A = C.load(sys.argv[1])
B = C.load(sys.argv[2], -int(sys.argv[3]))
for t in map(int, sys.argv[4:]):
    a, b = A.get(t), B.get(t)
    if a is None or b is None:
        print(t, "missing")
        continue
    n = len(a) // 4
    diff = [i for i in range(n) if a[4 * i:4 * i + 4] != b[4 * i:4 * i + 4]]
    ulps = []
    for i in diff:
        x = struct.unpack_from("<I", a, 4 * i)[0]
        y = struct.unpack_from("<I", b, 4 * i)[0]
        if (x ^ y) >> 31:
            ulps.append(None)  # sign differs
        else:
            ulps.append(abs(x - y))
    small = sum(1 for u in ulps if u is not None and u <= 2)
    big = sum(1 for u in ulps if u is None or u > 2)
    perc = {}
    for i in diff:
        perc[i * 4 % C.STRIDE] = perc.get(i * 4 % C.STRIDE, 0) + 1
    top = sorted(perc.items(), key=lambda kv: -kv[1])[:8]
    print(f"clock {t}: {len(diff)} of {n} words differ ({small} within 2 ulp as integers, {big} further); "
          f"most-differing offsets: " + ", ".join(f"+0x{o:03X}x{c}" for o, c in top))
