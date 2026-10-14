# Native 2D draws (Phase 5, step 3)

The game's HUD, menus, text, maps and fades no longer reach the GS as GIF packets to parse. Survey:
`docs/native-2d.md` on `re/native-2d-survey` (every 2D draw goes through `vifDirect` 0x226F70, PATH2,
and `sendTexturePackets` 0x207B50, PATH3).

## What it is

- `gs_native_2d.{h,cpp}` (fork): `Native2DDecoder` interprets the registers 2D writes (PRIM, RGBAQ, ST, UV,
  XYZ/XYZF 2 and 3, TEX0_1, TEX1_1, TEX2_1, CLAMP_1, ALPHA_1, TEST_1; PACKED, REGLIST and A+D) and returns a
  `Native2DPacket`: ordered `Draw` (vertices of one run in one complete `Native2DState`: prim flags, TEX0,
  TEX1, CLAMP, ALPHA, TEST), `Clut` (the palette load a TEX0/TEX2 write asks for) and `Raw` (verbatim GIF
  bytes for what 2D doesn't own: context-2 state and the 3D's shadow sprite, TEXA, SCISSOR, XYOFFSET,
  uploads).
- Hook: `PS2Memory::setNative2DHook`, called from the VIF1 worker for each DIRECT payload and for PATH3 chains
  of at most 16 KB. Taken payloads leave a marker packet (A+D write of the id to register 0x7C) on the same
  path, so the GS thread handles them in order with everything else. Payloads without draws, and payloads the
  decoder can't take whole, go on as the packets they were; the latter are counted by reason.
- GS thread (`GS::processNative2DMarker` / `runNative2D`): per run of draws, `WideLayout::beginRecords /
  addRecord / endRecords` (the same element grouping and anchors as for packets, on the records' raw X),
  then primitive assembly, batches built from the record's state over the frame environment, `Submit`. HUD
  placement, the UI classes (`vertexClass`), the 2D verdicts per frame buffer, the preferred display source
  and frame skip behave as for packets; the map layer, the UI mask, glyph repaint and texture packs hang off
  the batch and its texture, which are unchanged.
- Textures: still found by the backend from the GS-memory image through `tex0` (the record has no pixel
  source of its own). A direct source replaces that lookup behind `Native2DState::tex0` without touching the
  decoder.

## Switches and counters

`RT_NATIVE_2D=0` off. `RT_NATIVE_2D_VERIFY=1`: the packet path draws (and is captured), the records are run
dry and compared batch by batch (vertices, prim, tex0, tex1, clamp, alpha, test, UI class); mismatches are
printed as `[native2d] MISMATCH` and counted. `RT_NATIVE_DRAW_LOG=1` adds a `[native2d]` line every 5 s:
payloads, taken as records (draws, primitives, cluts, verbatim sub-packets), fallbacks by reason (prim, format,
carry, no_state, prmode, truncated, ring, disabled). Tests: `native_2d_test` (decoder and WideLayout records),
`tests/regression/test_native_2d.py` (scenes: old packets vs records; verify).

## What the native 2D path still needs from the GS side

Needed, and why:
- The frame environment registers of context 1 (FRAME_1, ZBUF_1, XYOFFSET_1, SCISSOR_1) and TEXA/TEXCLUT:
  libgraph (`sceGsPutDrawEnv` 0x26FE80, `sceGsSwapDBuffDc` 0x26F738) writes them per frame buffer through
  DMA, and they are not in the draw record. Native replacement: intercept those libgraph calls and keep the
  environment as plain per-frame-buffer data.
- A GIF parser for the verbatim `Raw` items (3D context-2 state packets, uploads) and for the PATH1 stream
  around the native 3D draws. Native replacement: decode those state packets at `vifDirect` as well (the
  survey lists them), and the loaders' uploads at `sendTexturePackets`.
- GS local memory and the CLUT: the textures. Needed until textures come from the game's files directly
  (the texture worker's job).
- Frame-buffer reads (frame blend, the loading blur): a backend feature (sampling a render target), not GS
  emulation.
Not needed: GSRegisters for draw state, the vertex queue, `vertexKick`, PRIM/PRMODE handling, the packet
mirror, paraLLEl-GS.
