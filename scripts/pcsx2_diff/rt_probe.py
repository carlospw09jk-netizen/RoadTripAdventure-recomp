#!/usr/bin/env python3
"""Print a few race fields every N vblanks of the recomp's attract demo (to find clocks)."""
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests/regression"))
from rtharness import Game  # noqa: E402

app, data, step, total = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
with Game(Path(data), Path(tempfile.mkdtemp(prefix="rtprobe_")), app=Path(app), speed="max") as g:
    for _ in range(total // step):
        d = g.step(step, reads=[(0x33590A, 1), (0x15AFC1C, 4), (0x15AFB8C, 4), (0x177ACE0, 12), (0x177ACE0 + 0x270, 12),
                                (0x3356C4, 4), (0x3356D8, 4), (0x335928, 4), (0x177AE20, 4), (0x177A0D0, 0x900)])
        f = lambda b: struct.unpack("<3f", b)
        print(g.vblank, "mode", d[0][0], "total", struct.unpack("<I", d[1])[0], "cd", struct.unpack("<I", d[2])[0],
              "c0", [round(x, 2) for x in f(d[3])], "c1", [round(x, 2) for x in f(d[4])],
              "scene", struct.unpack("<I", d[5])[0], "h", hex(struct.unpack("<I", d[6])[0]),
              "df", hex(struct.unpack("<I", d[7])[0]), "lapf", struct.unpack("<I", d[8])[0], flush=True)
        pool = d[9]
        act = [(i, struct.unpack_from("<IIII", pool, i * 0x30)) for i in range(0x30) if struct.unpack_from("<I", pool, i * 0x30)[0]]
        print("   tasks", [(i, hex(t[3]), t[2]) for i, t in act], flush=True)
