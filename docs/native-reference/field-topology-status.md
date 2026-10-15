# FLD tile topology status (Road Trip, SLUS-20398)

Distilled from `build/scratch/decomp-audit/field-index-grouping.md` (2026-10-15,
full test log lives there, gitignored).

- Container facts live in `disc-content-format.md` (scratch) — header
  `[0x20, offA, offB, offC, size]`, `0x1012xxxx` mesh/material descriptor
  tags, float6 `(x,z,y,S,T,Q)` vertex shape.
- The `u16` run at `[0x50A6, offA)` is **NOT topology** — 939/941 entries are
  zero; only the first two (`64, 63`) are nonzero. The exact layout occurs in
  only 26/64 tiles: treat it as scratch/padding, not indices.
- The 65-entry directory at `offA` is 16-byte-aligned in all 64 tiles but is
  not a fixed-stride vertex-pointer table (no stride/phase yields coherent
  float6 vertices; direct dereference reads mostly zeros).
- Strongest Q≈1.0 float6 density (~29%) is in `[offB, offC)` — consistent with
  direct **sequential vertex streams** consumed index-free by the VU1
  microprogram.
- Open: full vertex indirection/topology needs VU0/VU1 macro decompilation —
  the `0x1012xxxx` words drive microprogram-controlled uploads and are not VIF
  codes (`decodeVifChain` -> BadTag). Until then `convertFieldTile` emits
  vertex-only records and the renderer keeps its analytic road/ground ribbons.
