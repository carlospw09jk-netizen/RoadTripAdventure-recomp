# COURSE/*.BIN data status (Road Trip, SLUS-20398)

2026-10-15 local-analysis pass (no OpenAI quota left this window; run in-repo).

- 19 course files (C00–C20, gaps at C15/C17). Header `u32[5+]`:
  magic `0x20` ([magic, offA, offB, offC, size]) or `0x30` (extra region
  boundaries at words 5+, size at word 4 differs from last boundary).
- Region roles mirror FLD tiles: `[0x20,offA)` model descriptors,
  `[offA,offB)` main terrain/mesh stream (1.7–3.5 MB), `[offB,offC)`
  mid stream, `[offC,size)` trailing per-course table.
- Trailing table prologue (identical across courses): i16 `[48, 0, 80, 0, 0,
  0, N, 0, ...]` where `N` varies per course (12..22 — likely an object/prop
  count), then repeating 32-byte records whose words are
  `0x10000000, 0, 0x01000201, 0, 0x60000000, 0, 0, 0` constant-template
  entries — **VU0 macro / DMA control words**, same family as the FLD
  `0x1012xxxx` tags (see `field-topology-status.md`).
- No raw u32 copies of the race ladder values 58500/144000/162000 exist in
  any course file (they are computed in ELF code, not stored per-course).
- Conclusion: route/checkpoint semantics inside COURSE binaries are encoded in
  VU0-macro streams and are NOT extractable without the VU micro-disassembly
  work that blocks FLD topology. The native app's authored monotonic-zone
  routes (`gameplay/route.*`) remain the shipped stand-in; per-course scalar
  anchors (prop count `N`, region sizes) are available via
  `assets/course_data.*` when macro RE lands.
