#!/usr/bin/env python3
"""Survey the GS uploads in the user's extracted disc (research tool for docs/native-textures.md).

    python3 scripts/gs_upload_survey.py <disc dir> [--models <dir of rt_model_dump outputs>]

Scans every .BIN/.GSL/.E3D for GIF packets that set BITBLTBUF/TRXPOS/TRXREG/TRXDIR (A+D, host to
local) and reports, per file group: upload counts, formats, sizes, bytes, overlaps inside a file
(with the real block layout of each format, so a palette sharing a block with the next texture
shows up), and conflicts between FLD tiles of different slots. With --models (a directory of
`rt_model_dump <file> <dir>/<GROUP>_<NAME>.BIN` outputs holding <NAME>.BIN.json) it also checks each
material's TEX0 against the uploads of its file.

Reads the disc at run time and prints numbers only: nothing derived from the game is written.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import struct
import sys

PSM = dict([(0, "CT32"), (1, "CT24"), (2, "CT16"), (10, "CT16S"), (0x13, "T8"), (0x14, "T4")])
BPP = dict([("CT32", 32), ("CT24", 24), ("CT16", 16), ("CT16S", 16), ("T8", 8), ("T4", 4)])

# GS block layout inside a 32-block page: (page w, page h, block w, block h, block number table)
_T32 = [[0, 1, 4, 5, 16, 17, 20, 21], [2, 3, 6, 7, 18, 19, 22, 23], [8, 9, 12, 13, 24, 25, 28, 29], [10, 11, 14, 15, 26, 27, 30, 31]]
_T16 = [[0, 2, 8, 10], [1, 3, 9, 11], [4, 6, 12, 14], [5, 7, 13, 15], [16, 18, 24, 26], [17, 19, 25, 27], [20, 22, 28, 30], [21, 23, 29, 31]]
LAYOUT = dict([
    ("CT32", (64, 32, 8, 8, _T32)), ("CT24", (64, 32, 8, 8, _T32)),
    ("CT16", (64, 64, 16, 8, _T16)), ("CT16S", (64, 64, 16, 8, _T16)),
    ("T8", (128, 64, 16, 16, _T32)), ("T4", (128, 128, 32, 16, _T16)),
])


def field(v, lo, n):
    return (v >> lo) & ((1 << n) - 1)


def blocks(psm, tbp, tbw, x0, y0, w, h):
    """The GS blocks a w x h image written at (x0, y0) of buffer (tbp, tbw) touches."""
    pw, ph, bw, bh, tab = LAYOUT[psm]
    per_row = max(1, (tbw * 64) // pw)
    out = set()
    for y in range(y0 // bh * bh, y0 + h, bh):
        for x in range(x0 // bw * bw, x0 + w, bw):
            out.add(tbp + ((y // ph) * per_row + x // pw) * 32 + tab[(y % ph) // bh][(x % pw) // bw])
    return out


def scan_file(data):
    """Host-to-local uploads: (psm, w, h, dbp, dbw, dsax, dsay, file offset)."""
    out = []
    q = struct.unpack_from("<%dQ" % (len(data) // 8), data, 0)
    off = 0
    while off + 32 <= len(data):
        tag, regs = q[off // 8], q[off // 8 + 1]
        nloop = tag & 0x7FFF
        if field(tag, 58, 2) == 0 and field(tag, 60, 4) == 1 and (regs & 0xF) == 0xE and 0 < nloop <= 64 and off + 16 * (nloop + 1) <= len(data):
            vals = [(q[(off + 16 * (k + 1)) // 8], q[(off + 16 * (k + 1)) // 8 + 1] & 0xFF) for k in range(nloop)]
            if all(a <= 0x63 for _, a in vals):
                bb = tp = tr = None
                direction = None
                for v, a in vals:
                    if a == 0x50:
                        bb = v
                    elif a == 0x51:
                        tp = v
                    elif a == 0x52:
                        tr = v
                    elif a == 0x53:
                        direction = v & 3
                if direction == 0 and bb is not None and tr is not None:
                    psm = PSM.get(field(bb, 56, 6))
                    if psm:
                        out.append((psm, field(tr, 0, 12), field(tr, 32, 12), field(bb, 32, 14), field(bb, 48, 6),
                                    field(tp, 32, 11) if tp else 0, field(tp, 48, 11) if tp else 0, off))
                off += 16 * (nloop + 1)
                continue
        off += 16
    return out


def upload_blocks(u):
    return blocks(u[0], u[3], u[4], u[5], u[6], u[1], u[2])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("disc")
    ap.add_argument("--models")
    args = ap.parse_args()

    files = dict()
    for root, _, names in os.walk(args.disc):
        for name in sorted(names):
            if name.endswith((".BIN", ".GSL", ".E3D")):
                p = os.path.join(root, name)
                files[os.path.relpath(p, args.disc)] = scan_file(open(p, "rb").read())

    total = sum(len(u) for u in files.values())
    nbytes = sum(u[1] * u[2] * BPP[u[0]] // 8 for ups in files.values() for u in ups)
    partial = sum(1 for ups in files.values() for u in ups if u[5] or u[6])
    print("uploads %d, %.1f MB; partial (dsax/dsay != 0): %d" % (total, nbytes / 1e6, partial))
    group = collections.defaultdict(collections.Counter)
    for rel, ups in files.items():
        for u in ups:
            group[rel.split("/")[0]][(u[0], u[1], u[2])] += 1
    for g, c in sorted(group.items()):
        print("== %s: %d uploads; most common %s" % (g, sum(c.values()), c.most_common(4)))

    pairs = collections.Counter()
    for rel, ups in files.items():
        own = collections.defaultdict(list)
        for k, u in enumerate(ups):
            for b in upload_blocks(u):
                own[b].append(k)
        seen = set()
        for lst in own.values():
            for a in range(len(lst)):
                for c in range(a + 1, len(lst)):
                    seen.add((lst[a], lst[c]))
        for a, c in seen:
            pairs[(rel.split("/")[0], ups[a][0], ups[c][0])] += 1
    print("block-level overlaps inside files (relative chains and streamed frames included):", sum(pairs.values()))
    for k, v in pairs.most_common(8):
        print("  %6d %s" % (v, k))

    fld = dict((r, set(b for u in ups for b in upload_blocks(u))) for r, ups in files.items() if r.startswith("FLD/"))
    names = sorted(fld)
    conflicts = 0
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            if names[i][-5] != names[j][-5] and fld[names[i]] & fld[names[j]]:
                conflicts += 1
    print("FLD tile pairs of different slots sharing a GS block: %d" % conflicts)

    if args.models:
        stat = collections.Counter()
        for d in sorted(os.listdir(args.models)):
            if "_" not in d:
                continue
            g, n = d.split("_", 1)
            p = os.path.join(args.models, d, n + ".json")
            if not os.path.exists(p) or g + "/" + n not in files:
                continue
            ups = dict()
            cov = set()
            for u in files[g + "/" + n]:
                ups.setdefault(u[3], u)
                cov.update(upload_blocks(u))
            seen = set()
            for m in json.load(open(p))["materials"]:
                t0 = int(m.get("TEX0_2", "0"), 16)
                if t0 in seen:
                    continue
                seen.add(t0)
                stat["materials"] += 1
                tbp, cbp, psm = field(t0, 0, 14), field(t0, 37, 14), field(t0, 20, 6)
                stat["cld%d" % field(t0, 61, 3)] += 1
                if "MIPTBP1_2" in m:
                    stat["with MIPTBP"] += 1
                if tbp in ups:
                    u = ups[tbp]
                    stat["tbp = an upload start"] += 1
                    if (u[1], u[2]) != (1 << field(t0, 26, 4), 1 << field(t0, 30, 4)):
                        stat["TEX0 size != upload size"] += 1
                if psm in (0x13, 0x14) and not all(b in cov for b in range(cbp, cbp + (1 if psm == 0x14 else 4))):
                    stat["palette blocks not uploaded"] += 1
        print("materials:", dict(stat))
    return 0


if __name__ == "__main__":
    sys.exit(main())
