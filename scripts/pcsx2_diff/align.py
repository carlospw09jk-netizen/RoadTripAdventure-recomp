#!/usr/bin/env python3
"""Find the clock offset between a recomp dump and a PCSX2 dump: the shift (PCSX2 clock minus
recomp clock) at which car 0's position over the first frames matches best.

usage: align.py recomp.bin pcsx2.bin [lo hi]
"""
import math
import sys

import compare as C

A = C.load(sys.argv[1])
B = C.load(sys.argv[2])
lo, hi = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (0, 900)
best = []
for sh in range(lo, hi):
    ts = [t for t in range(min(A), min(A) + 400) if t in A and (t + sh) in B]
    if len(ts) < 30:
        continue
    e = sum(math.dist(C.pos(A[t], 0), C.pos(B[t + sh], 0)) for t in ts) / len(ts)
    best.append((e, sh, len(ts)))
best.sort()
print(best[:5])
