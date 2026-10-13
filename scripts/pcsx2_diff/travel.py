#!/usr/bin/env python3
"""Distance each dump's cars travelled (sum of per-frame steps, x/z) between two clocks, averaged
over the cars, and car 0's position: a systematic speed bias shows up here before the cars' paths
diverge. B's clock = A's + shift.

usage: travel.py shift clock0 clock1 dump [dump ...]   (the first dump's clocks are the reference;
shift applies to dumps whose name starts with pcsx2)
"""
import math
import os
import sys

import compare as C

shift, c0, c1 = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
for path in sys.argv[4:]:
    d = C.load(path, -shift if os.path.basename(path).startswith("pcsx2") else 0)
    tot = 0.0
    n = 0
    for k in range(24):
        s = 0.0
        prev = None
        for t in range(c0, c1 + 1):
            if t not in d:
                prev = None
                continue
            p = C.pos(d[t], k)
            if prev is not None:
                s += math.hypot(p[0] - prev[0], p[2] - prev[2])
            prev = p
        tot += s
        n += 1
    p0 = C.pos(d[c1], 0) if c1 in d else None
    print(f"{os.path.basename(path):22s} mean distance/car {tot / n:10.3f}   car0 at {c1}: {p0}")
