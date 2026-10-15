# Native 2D draws (Phase 5, step 3)

The game's HUD, menus, text, maps and fades no longer reach the GS as GIF packets to parse. Survey:
`docs/native-2d.md` on `re/native-2d-survey` (every 2D draw goes through `vifDirect` 0x226F70, PATH2,
and `sendTexturePackets` 0x207B50, PATH3).

## What it is

- `gs_native_2d.{h,cpp}` (fork): `Native2DDecoder` interprets the registers 2D and scene state packets write
  (PRIM, RGBAQ, ST, UV, XYZ/XYZF 2 and 3, TEX0_1/2, TEX1_1/2, TEX2_1/2, CLAMP_1/2, ALPHA_1/2, TEST_1/2,
  SCISSOR_1/2, FRAME_1/2, ZBUF_1/2, XYOFFSET_1/2, TEXA; PACKED, REGLIST and A+D) and returns a `Native2DPacket`:
  ordered `Draw` (vertices of one run in one complete `Native2DState`: prim flags, TEX0, TEX1, CLAMP, ALPHA, TEST,
  context index `ctxt`, optional `hasTexa`/`texa`, optional `hasScissor`/`scissor`), `Clut` (the palette load a
  TEX0/TEX2 write asks for) and `Raw` (verbatim GIF bytes for what 2D doesn't own: Context-2 draws and state,
  unowned registers, uploads). Context-2 state packets and shadow-darken passes (`PRIM = 0x246`) are tracked in
  per-context register sets (`ctx[2]`).
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
- State packets at `vifDirect`: Context-2 state registers (TEST_2, ALPHA_2, TEX0_2, TEX1_2, CLAMP_2, SCISSOR_2,
  FRAME_2, ZBUF_2, XYOFFSET_2) and the shadow-darken pass are now decoded natively by `Native2DDecoder` into
  `ctx[2]`, with Context-1 draws tagged with optional `texa` and `scissor`. Context-2 draws and state writes remain
  forwarded as verbatim `Raw` items to keep the GS frontend environment in sync for subsequent 3D draws. A GIF parser
  remains for verbatim `Raw` items (uploads, unowned registers) and for the PATH1 stream around the native 3D draws.
- GS local memory and the CLUT: the textures. Needed until textures come from the game's files directly
  (the texture worker's job).
- Frame-buffer reads (frame blend, the loading blur): a backend feature (sampling a render target), not GS
  emulation.
Not needed: GSRegisters for draw state, the vertex queue, `vertexKick`, PRIM/PRMODE handling, the packet
mirror, paraLLEl-GS.
