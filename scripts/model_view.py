#!/usr/bin/env python3
"""Render an OBJ from rt_model_dump to a PNG (vertex colours, three views) for checking by eye.

    python3 scripts/model_view.py build/model_dump/Q00.BIN.obj out.png [--object SUBSTRING]

Needs numpy and matplotlib. Keep the PNGs under build/: they show the game's models.
"""
import argparse

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def load(path, only):
    verts, cols, faces = [], [], []
    keep = only is None
    for line in open(path):
        if line.startswith("o "):
            keep = only is None or only in line
        elif line.startswith("v "):
            p = line.split()
            verts.append([float(x) for x in p[1:4]])
            cols.append([float(x) for x in p[4:7]] if len(p) >= 7 else [0.7, 0.7, 0.7])
        elif line.startswith("f ") and keep:
            faces.append([int(t.split("/")[0]) - 1 for t in line.split()[1:4]])
    return np.array(verts), np.array(cols), np.array(faces, dtype=int)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("obj")
    ap.add_argument("png")
    ap.add_argument("--object", help="only objects (chains) whose name contains this")
    args = ap.parse_args()
    v, c, f = load(args.obj, args.object)
    if not len(f):
        raise SystemExit("no faces")
    tris = v[f]
    col = c[f].mean(axis=1)
    col = np.clip(col * (0.6 / max(np.median(col), 1e-3)), 0, 1)  # colour scale varies by format
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9
    shade = np.abs(n @ np.array([0.4, 0.8, 0.45]))  # a little shading so shapes read
    col = np.clip(np.nan_to_num(col * (0.45 + 0.55 * shade[:, None])), 0, 1)
    lo, hi = v[np.unique(f)].min(0), v[np.unique(f)].max(0)
    fig = plt.figure(figsize=(15, 5))
    for i, (elev, azim, title) in enumerate(((20, -60, "perspective"), (90, -90, "top"), (0, -90, "front"))):
        ax = fig.add_subplot(1, 3, i + 1, projection="3d")
        # y is up in the game and the OBJ; show x, z on the ground.
        pc = Poly3DCollection(tris[:, :, [0, 2, 1]], facecolors=col, edgecolors="none")
        ax.add_collection3d(pc)
        ax.set_xlim(lo[0], hi[0]); ax.set_ylim(lo[2], hi[2]); ax.set_zlim(lo[1], hi[1])
        ax.set_box_aspect((hi[0] - lo[0] + 1e-3, hi[2] - lo[2] + 1e-3, hi[1] - lo[1] + 1e-3))
        ax.view_init(elev=elev, azim=azim)
        ax.set_title(title)
        ax.set_axis_off()
    fig.suptitle(f"{args.obj.split('/')[-1]}: {len(f)} triangles")
    fig.tight_layout()
    fig.savefig(args.png, dpi=110)


if __name__ == "__main__":
    main()
