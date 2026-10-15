# FLD tile format — container structure DECODED (was: topology blocked)

Updated 2026-10-15. The earlier negative result ("padding/macro-driven")
predated locating the runtime loader. Ghidra xrefs to `fieldCellTables`
(0x177AB20) lead to `loadFieldTile` @ 0x20BD4C / `FUN_0020c188` @ 0x20C188,
which hand the container semantics directly. Verified numerically against
`FLD/000.BIN` (2.6 MB) on the user's disc extract.

## Header (5 x u32 @ 0x00)

`[0x20, offA, offB, offC, fileSize]` — 000.BIN: `0x20, 0x5800, 0x1b2490,
0x263b80, 0x263e70`.

## Regions

1. **`[0x20, …)` tile DMA packets.** Four DMA REF tags (hi byte `0x10` =
   REF/chain, QWC = low 16 bits, addr = bits 0–27; addresses are RUNTIME RAM
   offsets against load base `0x100000`, some targeting scratchpad
   `0x1FF0xxxx`). Payloads are inline (tag + QWC quadwords). This is the
   texture/VIF staging the old analysis mistook for a vertex index region.
2. **`offA`: 65-entry monotonic pointer table**, offsets **relative to offA**
   (the loader adds the copy base at runtime — `*p += base`, 65 iterations).
   All 65 non-zero and in-range for 000.BIN; each target begins with a DMA
   tag word (`0x10000259`-style: QWC + `EN|TADR` bits) followed by its
   payload — one sub-block per model chunk.
3. **Immediately after: 256-entry attribute table** (248 non-zero,
   non-monotonic — material/attribute words, semantics still open).
4. **`offB`: PMP model block** — magic word **`0x608`** (or `0x600`),
   literally printed by the loader as `"Old Style PMP format"` for 0x600.
   Followed by a monotonic section-pointer table (`0x1ab8, 0x1ff8, 0x2488,
   …`, relative); 0x608 = new style with an extension block whose file offset
   is the word at `offB + 0x180` (`0x2d838` for 000.BIN, relative).
   Q≈1.0 float6 vertex density starts immediately inside this block —
   matching the long-standing density-peak observation for `[offB, offC)`.
5. **`[offC, fileSize)`**: small tail (`0x2f0` B here) — trailing records.

## Loader state machine (FUN_0020c188)

`case 0/1`: DMA first `0x800`-gram windows of the file into scratch
`0x100000`. `case 2`: walk the REF-tag chain (`byte3 == 0x10`, terminate
`byte3 == 'p'` = `0x70` = REF-End), sync DMA chan 1, `sendTexturePackets`,
size-check `< 0x750000`, copy `DAT_00100004..end` into
`fieldTileData + slot * 0x3A8000`. `case 3`: relativize the 65-entry and
256-entry tables by the tile base, read the PMP magic at
`(header[2]-header[1])`-style offset, store `fieldCellTables[slot]`.

## Corpus behaviour (64 tiles)

`tests/native_assets/test_field_converter.cpp` catalogs all `FLD/*.BIN`:
**50 canonical** (this layout), **13 layout variants** where `offB < offA`
(alternate region ordering — e.g. `011.BIN`: `offA=0x1e800`,
`offB=0x1e3dc0`), and **1 header variant** (`023.BIN`, first word `0x160`).
Variant parsing is deferred; the split is asserted in the suite so drift is
caught.

## What remains for real terrain

- Parse PMP sections from the `offB` section table (counts live in the
  section blocks; vertex stride f6 `(x,z,y,S,T,Q)` already observed).
- Interpret the 65 sub-block DMA payload streams (VIF-adjacent, feeds VU0
  macros) — only needed for per-cell placement/materials, not for the tile
  mesh itself, which the PMP block carries.
- 256-entry attribute table semantics.

`assets/field_converter.{h,cpp}` should be extended to implement: header →
directory walk → PMP magic/section table → vertex extraction. The
"needs a VU0 macro disassembler" blocker applies only to sub-block payload
detail and COURSE route streams, not to tile geometry.
