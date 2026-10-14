# The game's rendering layer (EE side) and model formats

Notes for the native renderer (branch `re/native-render`). Our own descriptions of how Road Trip
builds its 3D draw lists and how its models are stored; addresses are the USA release (SLUS-20398).
Names are in `config/game_symbols.toml` and appear in the Ghidra export (`scripts/ghidra/fn.py`).
Verified = read in the decompiled code or the disc data, or measured; *likely* and *guess* mark
the rest. The VU1 microprograms themselves are documented separately (Phase 1 items 3-4).

Tools:
- `python3 scripts/ghidra/decompile.py`: Ghidra headless project and pseudocode export (`build/decomp/`).
- `python3 scripts/ghidra/fn.py <addr|name>`: one function's pseudocode, callers and callees.
- `build/.../rt_model_dump <disc file> <out dir>` (`tools/model_dump.cpp`): a file's model chains as
  OBJ + JSON (chains, batches, materials); `--va 0x...` decodes chains inside the boot ELF.
- `scripts/model_view.py <obj> <png> [--object NAME]`: renders a dump with matplotlib.

Checked by eye (dumps rendered to PNG): a car body (CAR0/Q00, Choro Q sedan with dark windows),
Peach Town's tile (FLD/223: terrain, roads, houses with gabled roofs), a course (COURSE/C00:
an oval raceway with stands on rolling terrain). Game space is y up. Every .BIN on the disc decodes
with these layouts: each batch's vertex block is NLOOP x the entry's vectors per vertex (plus the
centre for billboards).

## Microprogram entries

The VU1 microcode starts with a jump table; MSCAL immediates map to the recompiler's `entryN` by
`imm * 8 / 0x10` (byte address = imm * 8).

| MSCAL imm | Entry | Called from | Role (EE side) |
|---|---|---|---|
| 0x0 | entry0 | `vifCameraSetup` 0x226540 | after a camera upload |
| 0x2 | entry1 | `vifObjectMatrix` 0x226B18 | after an object matrix upload (VU 0..3) |
| 0x4 | entry2 | model chains | lit models: cars, props, shop interiors, coins |
| 0x6 | entry3 | model chains | billboards (trees, signs) in FLD/COURSE/ACTION |
| 0x8 | entry4 | model chains | terrain cells (FLD/COURSE/ACTION), the arc ribbon |
| 0xA | entry5 | model chains | unlit models: minimap, car shadows, some car parts, far props |
| 0xC | entry6 | ELF chains | sky dome and sky layers |
| 0xE | entry7 | `vifObjectMatrixSky` 0x227270 | the sky's object matrix |

Batch counts on the disc (every .BIN scanned): entry4 ~640k, entry2 ~150k, entry5 ~13k,
entry3 ~9k. entry3 is common (billboards) though it was never in a capture.

## VU1 memory as the EE fills it

| VU1 qword | Written by | Content |
|---|---|---|
| 0x00-0x03 | `vifObjectMatrix` / `vifObjectMatrixSky` | object (local to world) matrix, then MSCAL entry1 / entry7 |
| 0x04-0x12 | `vifUploadCamera` (15 qw from the camera block) | camera matrices (below) |
| 0x04-0x0B / 0x10-0x17 | `vifUploadAt4` / `vifUploadAt16` (8 qw) | alternative view blocks (map, menus) |
| 0x10-0x13 | `vifUploadLightAt16` | light block (0x177F5E0: light position/direction) |
| 0x13-0x15 | `vifUploadSceneParams` (0x177F720) | time-of-day weights x, y, z; fog parameters |
| 0x14-0x17 | `vifUploadLightMatrix` | light/colour matrix (0x177F620 + n*0x40, time-of-day blended) |
| 0x18-0x19 | `vifUploadColours` | 8 floats: car paint intensities (from `paintIntensity` 0x29D150 by colour nibble), map colours |
| 0x1A | `vifSetBatchOffset` | tile origin added to model positions (terrain quadrants) |
| 0x21F.. | `vifFixedRegs` | a 2-qword GS register packet at a fixed address |
| BASE 0x8D, OFFSET 0x1AE | `vifCameraSetup` | double-buffered input: TOPS = 0x8D or 0x23B |
| TOPS+0 | model chain | the batch's output GIF tag |
| TOPS+1.. | model chain | the batch's vertices |
| TOPS+0x192.. | model chain or `vifBatchRegs` / `vifUploadPacket` | a GS register packet (material) |

## Packet helpers (0x2263B8-0x227270)

All build into one VIF1 DMA chain at `vifPacketPtr` (0x3356EC): `vifBeginPacket(0|1)` picks
0x1750BF0 or a second buffer 0x1730BF0 (the main pointer is kept in 0x3356F0 while the second is
built), `vifEndPacket` writes an END tag (MSKPATH3 off) and returns the chain start, `vifKick`
starts VIF1 DMA in chain mode (CHCR 0x145). Each helper checks the buffer limit (0x17523D0 /
0x1750BD0) and calls `fatal` (0x2006F0) on overflow.

| Helper | DMA tag | VIF codes |
|---|---|---|
| `vifUploadCamera(p)` | REF 15 → p | MSKPATH3 on; UNPACK V4-32 15 → 0x04 |
| `vifUploadSceneParams(p)` | REF 3 → p | FLUSHE; UNPACK V4-32 3 → 0x13 |
| `vifCameraSetup()` | CNT 1 | FLUSHE; MSCALF 0; then BASE 0x8D, OFFSET 0x1AE |
| `vifSetBatchOffset(q)` | CNT 1 | FLUSHE; UNPACK V4-32 1 → 0x1A; the qword |
| `vifUploadAt4(p)` / `vifUploadAt16(p)` | REF 8 → p | (FLUSHE); UNPACK V4-32 8 → 0x04 / 0x10 |
| `vifUploadLightMatrix(m)` / `vifUploadLightAt16(m)` | CNT 4 | FLUSHE; UNPACK V4-32 4 → 0x14 / 0x10 |
| `vifUploadColours(a, b)` | CNT 2 | FLUSHE; UNPACK V4-32 2 → 0x18 |
| `vifObjectMatrix(m)` | CNT 5 | FLUSHE; UNPACK V4-32 4 → 0x00; matrix; FLUSHE; MSCALF 2 (entry1); BASE 0x8D; OFFSET 0x1AE |
| `vifObjectMatrixSky(m)` | CNT 5 | the same with MSCALF 0xE (entry7) |
| `vifBatchRegs(d0, r0, d1, r1)` | CNT 2 | STCYCL 1,1; UNPACK V4-32 2 → TOPS+0x192: a REGLIST GIF tag (template `gifRegListTemplate` 0x29ED10: NLOOP 1, EOP, NREG 2, REGS = r0, r1) and {d0, d1} |
| `vifFixedRegs(...)` | CNT 2 | as above → 0x21F (no TOPS) |
| `vifUploadPacket(p, n)` | REF n → p | STCYCL 1,1; UNPACK V4-32 n → TOPS+0x192 |
| `vifDirect(p, n)` | REF n → p | FLUSH; DIRECT n (PATH2 GS packet: fog colour, scissor, alpha/test registers) |
| `vifCallModel(chain)` | CALL → chain | none: the model's own chain does the rest |

`vifBatchRegs` is how cars and props get their texture: register 7 = TEX0_2 (register 0xF =
NOP for the second slot, or 7 again with CLD 0 and CSA = part index for parts sharing one CLUT).
A car's TEX0: TBP0 = 0x243E + slot*0x44, TBW 2, PSMT8, 128x128, TCC 1, MODULATE, CBP = TBP0 + 0x40,
CLD 1. Terrain, billboards and course objects carry their material inside their own chain.

## Camera block (0x177E6D0 + idx * 0x190)

`renderInit` (0x221300) gives the viewport task its block from its viewport field n (task + 0x1C):
0x177E6D0 for n = 0 (one player) and n = 1, 0x177E860 for n = 2 (the second player's half).
`buildCamera` (0x21F698) fills it:

| Offset | Content |
|---|---|
| +0x000 | projection (x scale = focal * aspect factor) x view: world → screen-scaled clip |
| +0x040 | projection (focal / 512) x view: world → normalised clip (likely the clipper's) |
| +0x080 | viewport: x 512 * aspect factor, the GS offsets and depth scale (clip → GS coordinates) |
| +0x0C0 | projection x view rotation only (no translation): for the sky |
| +0x100 | view matrix (world → camera) |
| +0x170 | camera position used to centre the sky |
| +0x180 | camera position (light direction is computed from it) |
| +0x18C | focal distance (cars' LOD divides view depth by it) |

`vifUploadCamera` sends +0x000..+0x0EF (15 qwords) to VU1 0x04..0x12. The widescreen hook narrows
the projections built here (CLAUDE.md, Widescreen).

## Scene drawing

`renderInit` sets the viewport task's draw function from the location byte (+0x22 of the scene):
0xFF → `drawSceneField` (towns and field areas), 0x22 → `drawSceneCloudHill`, 0x12/0x1C/0x20 →
`drawSceneAction` (maze, tunnel race, curling), else `drawSceneRace`. Per frame, per viewport,
`drawSceneField` (0x220938) builds one chain in this order:

1. `updateLightDirection`; picks which of the 4 tile quadrants are near the player.
2. `vifBeginPacket(1)`; `drawSkyDome` (camera block, `vifObjectMatrixSky` with the camera
   position, ELF chain 0x290A40) and `drawSkyLayers` (ELF chains 0x2962E0, 0x298A50, colours by
   the time-of-day weights).
3. Camera, scene parameters, entry0; the fog packet (`vifDirect` of a FOGCOL packet built at
   0x172FF50, and 0x29CFC0).
4. `drawFieldTiles` (0x21FB38): for each loaded quadrant, `vifSetBatchOffset` (origin from
   0x29D080), then 64 cells of 200 x 200 units (an 8 x 8 grid of one 1600-unit tile), each culled by
   `cellVisible` (0x21FA38) and drawn with `vifCallModel(cellTable[i])`.
5. `drawCarsField` (0x2228A8): 24 cars (0x177ACE0, stride 0x270): shadow quads first (per car an
   object matrix and the ELF chain 0x290570, entry5), then `drawCar` (0x221788) for cars whose
   projected size passes a threshold, then `drawCarExtraParts`.
6. `drawFieldProps` (32 placed objects, misc models 0x177F580), `drawCoins`, `drawAnimatedProps`
   (dispatch table 0x2C43B0 to 0x25BE98..0x25D5F8).
7. Camera again; `drawArcRibbon` (0x2224F8) and the 65th chain of each tile (`drawFieldTilesExtra`,
   after everything opaque: water and other translucent layers, *likely*); `drawCarEffects`.
8. `vifEndPacket`, `vifKick`.

No depth sorting anywhere: draw order is fixed (opaque world, cars, props, then translucent layers)
and the GS Z buffer does the rest. Culling is per terrain cell (frustum test of the cell centre)
and per car (size on screen).

### LOD

- Cars (`drawCar`): body model 0 (near) or 1 (far) by view depth / focal against 0x332CC0, and at
  most 6 near bodies per frame (counter 0x3356E8, reset by `drawCarsField`). Wheels (*likely*:
  they get the car's wheel positions, car + 0x80): 3 levels (`drawCarWheelsNear` 0x221570 when the
  distance factor is 0, `drawCarWheelsMid` 0x2213E8 below 0x332CCC, one far model otherwise).
- Terrain, props: no LOD (cells are culled whole).

### Car drawing (`drawCar`, 0x221788)

Per car slot s: the slot's six models at `carModels` (0x177F310 + s*0x18), the car record's colour
nibbles → `paintIntensity` → `vifUploadColours` (VU 0x18-0x19); light matrix by time of day
(0x177F620 + n*0x40) through 0x226058; object matrix; `vifBatchRegs` with the slot's TEX0; body
(near/far), body variant (flag 0x1000 picks model 2 or 3), optional extras (models 4, 5); parts
(0x177EAC8.., wheels, wings, jets) each with its own matrix and CSA.

## Model storage

### Containers

| Files | Load address | Layout |
|---|---|---|
| FLD/xyz.BIN (64 tiles, name = tile code in base 4), COURSE/Cnn, ACTION/Ann | 0x707B80 + slot * 0x3A8000 (`loadFieldTile` 0x20BAC8) | header: u32 header size (0x20), then section offsets s1..s5. Bytes header size..s1 are the textures' GS upload chain (CNT tags to END). Section 1: 65 relative pointers (64 cells, row-major 8 x 8, then one extra) followed by the cell chains. s2: collision ("PMP", version 0x600/0x608). s3: the minimap model. s4: misc model table (props). |
| CAR0..CAR4/Qnn.BIN | 0x3FBB80 + slot * 0x34000 (`loadCarModelAt`) | 6 relative model pointers (body near, body far, variants, extras), u32 model data size, then the textures' upload chain (TBP rebased to 0x243E + slot * 0x44) |
| CARS/PARTS, TIRE, WHEEL, Q150; SHOP/*; SYS/COIN | various | a table of model pointers, then models (same model format) |
| boot ELF (.vutext) | in place | sky (0x290A40, 0x2962E0, 0x298A50), car shadow (0x290570), arc ribbon (0x299430) |

Pointers are relative to the section (or file) start; the loaders add the load address once.

### Model header (16 bytes)

`{u32 partA_offset, u32 partB_offset, u32 partA_batches, u32 partB_batches}`, then the main chain at
+0x10. `vifCallModel(model + 0x10)` draws the main chain; `drawModelParts(model, mask)` (0x2234C8)
draws part A (mask bit 0) and/or part B (bit 1) at `model + offset` when its count is non-zero.
The minimap is part B of a tile's map model (FLD/223: 65 batches, entry5, untextured strips).

### Chain layout (every model chain on the disc)

One CNT tag holding the whole mesh, then RET (ELF chains are a single RET tag with data). VIF codes:

```
STCYCL 1,1
[UNPACK V3-32 n → TOPS+0x192]        material: A+D GIF tag + (n-1) registers   (optional, sticky)
repeat per batch:
  UNPACK V4-32 1 → TOPS+0            output GIF tag (PACKED, PRE, NREG 3, REGS ST RGBAQ XYZF2)
  UNPACK V3-32 m → TOPS+1            vertex block, m = NLOOP x vectors per vertex
  MSCALF entry
```

Never seen in model chains: STROW, STCOL, STMASK, masked or 8/16-bit UNPACKs, ITOP, MSCNT.
The vertex count comes from the output tag's NLOOP (no ITOP). V3 unpacks leave W alone.

Material blocks: A+D TEX1_2 (0x15), TEX0_2 (7), CLAMP_2 (9), optionally MIPTBP1_2 (0x35). They
stay in force for the following batches until the next block (GS state; how the microprogram kicks
them across the double buffer is for the microcode notes). Cars and props have none in their
chains: `vifBatchRegs` supplies TEX0_2 before the call.

Output GIF tag (TOPS+0):

| Bits | Meaning |
|---|---|
| NLOOP | vertices in the batch (one strip) |
| PRIM | 0x264 / 0x26C / 0x274 / 0x27C seen: triangle strip, CTXT 2, ABE, FGE; IIP and TME vary |
| bit 33 (spare) | 0 or 1 per batch: a flag for the microprogram (meaning open) |
| word 3 (REGS high, unused by GIF with NREG 3) | entry3/4: the same tag's upper word with PRIM = triangle fan (0x313Ec000 for 0x27C): the clipper's output tag (*likely*); entry2: 0, 1 or 2 (open) |

### Vertex layouts (vectors of 3 floats, 12 bytes each)

| Entry | Per vertex | Vectors |
|---|---|---|
| entry2 (lit) | 4 | position; unit normal; colour (0..255 scale, e.g. 200); (s, t, aux) - aux 0..357 on cars, 0..178.5 in the shop (open: not q) |
| entry5 (unlit) | 4 | position; (0, 0, 1) or zeros (unused normal); colour (0..255); (s, t, q = 1) |
| entry4 (terrain) | 5 | position (tile-local, 0..1600); three colour vectors A, B, C; (s, t, q = 1) |
| entry3 (billboard) | 1 + 5 per corner | centre position; then per corner: offset (dx, dy, 0) in screen-aligned units, colours A, B, C, (s, t, q) |
| entry6 (sky) | 3 | the vertex block starts with the GIF tag as one V3 vector; then position, colour (128 = 1.0), (s, t, q) |

Terrain colours: the time-of-day weights at VU 0x13 (x day, y dusk, z night by the clock in
`updateTimeOfDayLight` 0x225988) blend them. Colour A looks like day and C like night; B's first
component is always 1.0 in town tiles and 0..1 on courses, so B is not a plain RGB: the exact
mapping (which components each weight scales) is open until the entry4 reference exists.

### Static or rebuilt?

- All model chains on the disc are loaded once and never written afterwards: a Quick Race probe
  (Peach Raceway, 20 s of racing) hashed 0x3FBB80..0x707B80 (car models) and 0x707B80..0x1500000
  (course, slot 2 at 0xE57B80) and found no changed byte. Only `vifCallModel` references them.
- Per frame, the EE writes only: matrices, camera, colours, materials (`vifBatchRegs`), and the
  packet chain itself (0x1750BF0 / 0x1730BF0).
- One exception: `drawArcRibbon` (0x2224F8, from 0x2207D0) recomputes 21 x 2 vertex positions with
  VU0 macro code into the ELF chain 0x299430 every frame, then calls it (entry4).
- Skinning/animation: none in the vertex data. Moving parts (wheels, wings, props, animated town
  objects) are separate models with their own object matrices.
- The minimap (`[map]` draw functions 0x242438 / 0x2427D0 / 0x242A88) is a static model too (map
  model part B, entry5) under a map view uploaded with `vifUploadAt4`; car markers (0x242D20 ..)
  are small models with per-car matrices.
- Sky: static ELF chains through entry6; the dome's matrix is centred on the camera (entry7).
- 2D (menus, HUD) does not go through these helpers' model calls: it is PATH2/PATH3 packets
  (`vifDirect`, GS upload chains). What reaches entry2 outside the 3D scene is menu scenes with 3D
  models (garage, car select).

## Open questions

- entry4/entry3 colour vectors: the exact blend (see above); entry2's third ST component.
- The spare bit 33 of the output tag and entry2's word 3 (0/1/2: possibly which paint colour of
  VU 0x18-0x19 applies; *guess*).
- How a material at TOPS+0x192 is kicked across the double buffer (once, or every batch) - decides
  whether a native path can treat materials as sticky state (assumed here).
- `drawCarEffects` (0x225120) sub-renderers 0x224068 / 0x2243E0 / 0x2246C0 / 0x224EC0 and the prop
  renderers 0x25BE98..0x25D5F8 are only named by their caller; `drawCarouselScene` (0x2455C0)
  and the menu scene 0x246250 are not yet confirmed.
- VU1 0x10-0x17 is written by several helpers with different content (light block vs view block);
  which microprograms read which is for the microcode notes.

## Native draw path (phase 2; on by default for the hardware GS on desktop, `RT_NATIVE_DRAW=0` off; Android off unless `=1`)

What exists (fork: `gs_native_draw.{h,cpp}`, `hw/native.vert`, `gs_hw_backend.cpp`, `ps2_runtime.cpp`
MSCAL callback, `gs_frontend.cpp` `processNativeMarker`):

- **Interception** on the VIF1 worker at an MSCAL for program 2 (lit strip) or 5 (offset strip), when
  the microcode image is Road Trip's and the batch's output tag is the strips' (PRE, triangle strip,
  PACKED, ST/RGBAQ/XYZF2). `buildNativeDraw` copies what the program reads: the setup run's VF
  registers (matrices, lights, fog, tints), the memory constants (entry 5) and the vertex block at TOP.
  The batch header packet (TOP+402: TEX0 etc.) still goes out as a normal GIF packet first, its NLOOP
  cleared as the program clears it. The run is skipped; entry 5's register side effects are applied.
- **Order:** a 32-byte A+D marker (register 0x7D, the draw's id) goes into the PATH1 stream where the
  XGKICK would have been. On the GS thread the frontend gives the draw to the backend
  (`SubmitNative`) as its own closed `Batch` (state of the strip's PRIM, texture resolved as for any
  batch), so merging, DATE, scissor, texture packs and frame buffers work as before.
- **Fallback on the GS thread:** where the backend can't take it (map batches, the UI mask, motion
  vectors, frame generation, a frame the classifier must see) the frontend builds the packet the
  program would have made (`nativeReference`, bit-exact VU float arithmetic) and processes it as usual.
- **Fallback on the worker:** an odd tag, a count that doesn't fit, a D-bit run (the clipper's debug
  break) and a microcode other than Road Trip's run as before; counted by reason in
  `RT_NATIVE_DRAW_LOG=1` lines (`[native] strips: ...`, every 5 s). Programs 2-6 are all native now.
- **Clipped programs (3 object, 4 world, 6 sky; branch `re/native-clip`):**
  - The draw carries the VF registers and VU words the program read (`vf`, `mem`, `top1`), and the
    GPU gets per-draw constants (matrices; entries 3/4: the chosen fog set from VF10/VF11 by the tag's
    spare bit 33, fog set B, VU26 offset, the colour-set weights VF9; entry 3: the object's origin in
    clip space; entry 6: VU6 axis, tint, cycling weights).
  - **Which triangles are not drawn** (ADC) is decided on the VIF1 worker, as a bit mask the shader
    reads: `classifyStrip` works the six CLIP flags of every vertex out in double with a margin far
    above the VU's rounding (1e-5 of the sum of the terms) and only trusts certain flags. A strip
    with every vertex inside (the great majority: 98% of terrain strips, 99.4% of object strips in
    the captures) costs one pass of that; a strip whose triangles are all either inside or rejected
    whole (all three vertices beyond one of -y, +y, -x, +x, -z, or any beyond +z) gets its mask and
    is otherwise drawn as it is. Anything else (a triangle to clip, a flag too close to call) and
    **every sky strip** (back-face culling by strip parity is a sign test of a cross product) runs
    `vu1hle::run` on a scratch VU (`nativeClippedReference`): the sibling's reference of the
    microprogram, bit-exact against the interpreter on every capture. Its kicked packets give the
    mask (ADC bits), the **polygons the clipper cut out of crossing triangles** (`NativeDraw::fans`,
    Sutherland-Hodgman in the microprogram's own arithmetic) and the registers it leaves.
  - The polygons are exact, so they are drawn as the packets the microprogram kicks, and **before the
    strip** (the microprogram kicks each as it finds it, the strip at its end): `processNativeMarker`
    sends `fans` through the ordinary GIF path, then the strip (native or as a packet).
  - **Carried state:** the sky's back-face test uses the previous vertex's screen position and edge
    from VF17/VF18, but the first two vertices of a run can't close a triangle, so nothing carries
    into a drawn triangle; the parity is the vertex's index in the run. Side effects of a skipped run
    on registers a later program reads (VF8 entry 3, VF12, VF17/VF18, and, when the reference ran,
    VF27.w/VF28 too) are applied by `applyNativeSideEffects`; a plainly inside strip leaves VF27.w
    and VF28 alone (they end up only in STQ.w, which the GS ignores).
  - **Colour sets** (entries 3/4): VF9.xyz are the weights of sets A, B, C and are the first row of
    the time-of-day light blend `updateTimeOfDayLight` (0x225988) makes at 0x177F620 + n*0x40
    (`todLightDay/Dusk/Night` 0x29D500/540/580 and the second set 0x29D5C0/600/640, each scaled by
    the clock's day/dusk/night weights written to `sceneParams` x/y/z and added), uploaded to VU 0x14
    by `vifUploadLightMatrix` and loaded into VF9..VF12 by SetupClipped; so the shader needs no
    time-of-day input of its own: it reads VF9 as the setup left it. (B.x, the alpha channel's source
    and 1.0 in towns, 0..1 on courses, is vertex data; the table above is not what varies it.)
    The exact table contents and which light set n each renderer picks are not decoded (open).
- **native.vert** does per vertex what the programs do: matrices, perspective divide, XY snapped to
  12.4 by truncation, Z as the 24-bit integer, STQ, fog, the lighting (entry 2) or offsets (entry 5),
  colour and fog as the integers the packet carries, and drops a triangle when any of its three
  vertices is outside the clip volume (the programs' ADC). Flat shading takes the closing vertex.
  Differences from the VU: round-to-nearest GPU floats (rare one-step differences in truncated XY/Z).
- **Verification:** `RT_NATIVE_DRAW_VERIFY=1` runs the microprogram as before and compares the
  reference with its output in VU memory (STQ.w of entry 2 excluded: it carries the previous
  vertex's fog). `tests/regression/test_native_draw.py` (hardware GS): the reference matches on every
  strip of a Quick Race and a town drive, and the native frames match the old path's. For the clipped
  programs the check also covers the CPU's verdict (ADC mask, polygons) against the reference made
  again from the inputs, and the last clipped polygon against VU memory 30.. (the polygon buffer).
  Scenes: Quick Races on courses 0, 2, 3 and 5 (carousel included), Peach Town, and a drive out of
  town into a field area; 0 mismatching runs in ~149 M verified runs (24.3, 12.9, 15.7, 18.2, 22.4, 27.9 and 28.0 M per scene) (13 Oct 2026).
- `vu1_replay <capture> --native` (the captures of `docs/vu1-microcode.md`, 13 Oct 2026): builds a
  `NativeDraw` for each record of entries 2-6 and compares the strip packet, the polygons, the ADC
  mask and the registers left with what the interpreter did: 0 mismatches in 48,467 runs
  (entry 4 3,463 + 8,000 [87 needed the reference, 82 with polygons], entry 3 14,625 [184 with
  polygons], entry 6 2,266 [all through the reference, 209 with polygons], entries 2 and 5).
- **Cost of the CPU part** (Quick Race, per 5 s window): of ~0.6 M entry-4 runs a few % went through
  the reference; the rest cost one `classifyStrip` pass. Fallback by reason: `unknown_entry`, which
  was ~40% of strip runs in a race (entry 4 ~616k, entry 3 ~15k, entry 6 ~4k of ~1.6 M per window),
  is gone; what remains is `ring_full` (the GS thread behind the worker) and the GS thread's own
  declines (map batches), unchanged.

### Native draws with motion vectors, the UI mask, frame generation, widescreen and the map (re/native-gen)

The backend used to decline native draws (the frontend then built the packet) for the map, the UI
mask, motion vectors, frame generation and any frame the widescreen classifier had to see. None of
them is declined now: the log's `declined_<reason>` counters (`classifier`, `backend`, `skipped`,
`other`, `map`, `ui_mask`, `motion`, `frame_gen`) read 0 in a Quick Race and Peach Town with each
asked for. `RT_NATIVE_DECLINE=<csv of classifier,ui_mask,map,motion,frame_gen>` puts a decline back
(A/B: the draw is built as a packet and counted under that name). Headless test hooks:
`RT_HWGS_MOTION=1`, `RT_HWGS_UI=1`, `RT_HWGS_SHADOWS=<n>`.

- **Who knows what.** The classifier (WideLayout) and the motion tracker live on the GS thread's side
  of the marker. `GS::processNativeMarker` takes the verdict from the draw's own data: where the
  frame's kind is known and its HUD hasn't begun (`plainPath1`) the class is the default (scene in a
  driving frame, UI on a 2D screen) and nothing is built; where the classifier has something to
  learn (the frame's first draw, the HUD phase) the packet the strip would make is built once on the
  CPU (`nativeReference`) and passed through the same `classifyWideUnlocked` as a GIF packet (moved out
  of `processGIFPacket`). A native strip is one unit for it, so its HUD placement is one affine map of
  screen X (`WideLayout::lastPacketPlacement`: scale k, bias 0 / 320(1-k) / 640(1-k) for the left,
  centre and right anchor), carried on the draw (`NativeDraw::vertexClass`, `xScale`, `xBias`) and
  applied in `native.vert` with the same 1/16 rounding. Only HUD draws (the minimap in towns, the
  course map and the markers in races) take this CPU path; the 3D never does.
- **UI mask and map layer.** `SubmitNative` builds the draw's triangles on the CPU
  (`nativeScreenTriangles`, ADC-skipped triangles left out, placement applied) for UI-class and map
  draws only, stages them for the UI mask (`stageUi`) and leaves them in the vertex list beside the
  draw (`Batch::hostCount`), so `noteMapBox` finds the map's box as before. The draw itself goes to
  the map layer in `record()` like any map batch (`Batch::map` from `isMap`).
- **Motion vectors.** MotionTracker's `onMscal` runs on the VIF1 worker before the native hook, so the
  marker before the native marker names the object's matrices (`m_motionId`). The frontend puts this
  frame's and last frame's world->GS-window matrices on scene-class draws (`curC`, `prevC`, 16 floats
  each: MotionTracker's C and C_prev) and `native.vert` outputs `vMotion = cur - prev` in GS pixels from
  the vertex's object-space position: what `MotionTracker::vertexMotion` gives, without the 12.4
  rounding, and with its guard (nothing far outside the screen or where a projection degenerates).
  UI-class draws carry none (their vertices stay in shadow frames, as the classifier's UI class
  always did). Entry 5's weighted axis is not part of the object matrices: it is added to the
  position as drawn, not to the motion.
- **Frame generation.** A shadow frame redraws the same native draws (`keepForShadows` keeps the
  batches) with `NativeExtra.flags.y` = t = (i+1)/(shadows+1): each corner's homogeneous position is
  `h + t (C x p - C_prev x p)`, i.e. the object's matrix moved on by t of its last frame's change
  (what `MotionTracker::packetReproject` does for the paraLLEl-GS shadows; the hardware GS's
  `moveVertices` extrapolated in screen space). Z and texture coordinates stay (coplanar decals keep
  equal depths). Two guards kept from the older path: a triangle with a vertex moving over 200 GS
  pixels (a mismatched object, a camera cut) stays, and so does one whose w changes sign. No 200 px
  rule is needed for correctness otherwise: the matrices are the objects' own.
- **Verified (Mac, hardware GS, `tests/regression/test_native_draw.py`).** `test_native_takes_part`
  (Quick Race and town x motion, UI mask, frame generation), `test_native_widescreen_hud` (16:9, HUD at
  the edges), `test_native_map_layer` (Peach Town, Enhanced map: same box, same map): no declines, and
  the pictures match the declining path's. Present dumps of a Quick Race state at 120 Hz (virtual time,
  `RT_SPEED=0.25`, same state, `RT_PRESENT_DUMP`): generated frames native vs the old shadow path on
  the same native real frames: mean absolute difference 0.146 (real frames 0.062), and the pixels where
  a generated frame differs from both real neighbours although those agree (the "flash" measure of #23)
  are 0.72% of the picture on average, 2.4% at worst (the packet shadows: 0.78%, 3.2%): the same
  places in both, where the real frames themselves jump. In a first real-time run, with the native
  car shadow (entry 5, a weighted axis added to the position) first moved without that axis, the
  shadow vanished in generated frames: the displacement is now added to the position as drawn. Motion view (`RT_SHOW_MOTION=1`) native vs the packet path: mean 0.047, 0.1% of
  pixels over 24 levels (object edges). UI mask view (`RT_SHOW_UI=1`) with 16:9 edges: identical to the
  packet path's (mean 0.0005 once the two windows are aligned).
- **What remains declined or differs.** Programs 3, 4 and 6 (billboards, terrain, sky) are native
  too (merged from re/native-clip) and take part the same way: the shader reads the vertex's
  object-space position at the program's own record stride, and a shadow frame moves the position
  the program computes (through clip space, with the batch offset) by the matrices' change. The
  polygons the clipper cuts out of triangles crossing the volume's edge (`NativeDraw::fans`) are
  packets, processed in order before the strip through the ordinary packet path (classifier, UI mask,
  per-vertex motion, `keepForShadows` + `moveVertices` for shadow frames): they are small, at the
  screen's edge, and not worth a second shader path. A flat 2D-looking native draw (the minimap) is not
  snapped to whole GS pixels above 1x as the packet path's flat triangles are. `hw_spirv.h` is
  regenerated by `hw/compile.py`: after merging with other shader changes run it again rather than
  merging the generated header.


### Merge of the three branches (re/native-merge, 13 Oct 2026)

- **One reference.** `nativeReference` is vu1hle's reference for every program (the draw's registers are
  put back from `NativeDraw::c` for 2 and 5). The first pass's own copy of 2 and 5 was bit-identical on
  every captured record but ~1.5x faster per call (0.6-1.3 us against 1.0-1.9 us); it runs only for the
  classifier, the UI mask and the map box (a few dozen draws a frame), so it was deleted.
- **Fans (clipped polygons)** are packets, processed in order before the strip through the packet path:
  classifier, UI mask, map layer, per-vertex motion and `moveVertices` in shadow frames use the same
  matrices (MotionTracker's C and C_prev) as the strip's shader; only the extrapolation differs
  (screen-space linear against the strip's homogeneous move), on small polygons at the screen's edge.
- **Bug found by the merged suite:** `sameDraw` ignores PRIM.IIP, so a native draw took the Gouraud flag of
  whichever draw opened its batch: the 2P cars' glass was flat-shaded per triangle (`Batch::nativeGouraud` now).
- **ring_full:** the queue of draws waiting for the GS thread is 32768 (was 8192; a busy frame is ~9k draws).
  0 fallbacks of any kind in the Quick Race, town and field verify scenes.
- **A/B hooks:** `RT_NATIVE_DRAW_ENTRIES=<csv of programs>`, `RT_NATIVE_DRAW_RANGE=<lo>,<hi>` (only those draws
  go to the shader; bisecting a picture difference), `RT_NATIVE_DRAW_GPU=0`, `RT_NATIVE_DECLINE`.

### Thor measurements (13 Oct 2026, dev app, 120 Hz, SGSR 2, 2x) and to-do

| scene | native | game fps | shown | GPU busy | GsThread / GifVif1 GHz-eq |
|---|---|---|---|---|---|
| Fuji City | off | 59.8 | 120 | 83% (383 MHz-eq) | 1.12 / 1.07 |
| Fuji City | on | 10.6 | 10 | 97% (650 MHz-eq), overload pause | 1.34 / 0.44 |
| Peach Town | off | 59.8 | 120 | 85% (341) | 1.34 / 1.32 |
| Peach Town | on | 19.9 | 19 | 97% (660), overload pause | 1.39 / 0.46 |

The second-screen map held (town, field, race) with native on. Native is therefore **off by default on
Android**: ~12k draw calls a frame (one per strip, each with its own 9 KB constant block) are GPU-bound on
Adreno (a smaller constant block changed nothing). To do: batch strips into few draws (one storage buffer of
draw records per frame, draw index per vertex), re-measure on the Thor (fps, per-thread GHz-eq, GPU busy,
overload pause), then present dumps of generated frames for flashing (#23 method), then default on.

## Reference renderers and the shipped app (removal to-do)

The shipped app (DMG, APK, Windows zip) will not contain the non-native renderers once the native
path covers 2D and textures. The first step is a build switch, not a deletion:

- `-DRT_REFERENCE_RENDERERS=ON` (parent `CMakeLists.txt`, forwarded as `PS2X_REFERENCE_RENDERERS`
  to the fork) builds paraLLEl-GS's GS (`gs_pgs_backend.cpp` and the `parallel-gs` library). Default
  OFF: the presets, CI and the release builds ship without it. With OFF, `createPgsBackend` returns
  an error and only Granite (`granite-vulkan`, `granite-math`: the Vulkan device) stays, with the
  presenter and the hardware GS.
- Dev builds that run the regression goldens (paraLLEl-GS pictures; hw is compared to them with a
  tolerance) configure with `cmake --preset macos-release -DRT_REFERENCE_RENDERERS=ON`.
- Sizes on the Mac (arm64, release, LTO), `RoadTrip` binary: ON 15.9 MB (14.5 MB stripped), OFF
  14.8 MB (13.4 MB stripped); bundle ON 26.7 MB, OFF 25.7 MB. The saving is small until the rest below goes.

Still tied to the old paths (to remove, in this order):

1. CPU GS (`gs_cpu_backend.cpp`, 2,000 lines): `GSFrontend` constructs it as its default backend, and
   the hardware GS's `m_cpu` uses it for local-memory transfers, CLUT loads and readbacks. Needs
   those moved into the native/GS-memory code (`ps2_gs_memory`).
2. Renderer choice: Options -> Graphics -> Renderer ("Accurate (paraLLEl-GS)"), `[display] renderer`,
   `wantHardwareGs()` and `RT_GS_BACKEND=pgs|cpu` in `src/main.cpp`, the test harness's pin of
   `RT_GS_BACKEND=pgs`, `--update-goldens`.
3. paraLLEl-GS-only code in shared files: `PgsControl` (texture packs, supersampling, `setDecodeHook`,
   the settings apply code), `PgsShared` and `gs_pgs_shared.h` (also used by the presenter and the
   hardware GS), `gs_pgs_presenter.cpp` (its Vulkan presenter is shared; the name is historical),
   shadow frames for frame generation on paraLLEl-GS, `GSInterface::uploads` texture tracking.
4. VU1 -> GIF packet path: `ps2_gif_arbiter.cpp`, `GS::fastPath1Run`, the packet mirror,
   `MotionTracker::packetMotion/packetReproject`, the HUD classifier's packet parse (`WideLayout`).
5. Save states: the `GSFE` chunk (frontend registers, local memory, CLUT) and `HWG1` (hardware GS
   targets) are read by every GS; a state from the other one loads with empty targets.
6. Test socket commands and tests that assume the packet path (`frame`, `gs_*`, the 3x3 comparison
   with paraLLEl-GS goldens in `rtharness/goldens.py`).
7. Windows and Android CMake branches that link `parallel-gs` for Granite: they link
   `granite-vulkan` directly with OFF; check each once.

  strip of a Quick Race and a town drive, and the native frames match the old path's.

### Thor profile of native draws, the compute passes, the model cache (branch `re/native-cache2`)

**Where the time went** (Thor dev app, Peach Town state `m_town`, 60 Hz, `RT_GPU_TIMES=2`, `RT_HWGS_STATS=1`, simpleperf on `GifVif1Worker` / `GsThread`
with `--call-graph fp`; `scripts/native_sample.py` is the Mac equivalent, `RT_NATIVE_DRAW_LOG=1` names the chains):

- **GPU:** one draw call per strip (~10,000 a frame, most of them a few triangles) cost Adreno ~4 us of GPU each: 38% busy (old path, a few big draws)
  -> 94% (native, one draw per strip), with the vertex shader's ALU irrelevant (skipping the clip tests, the lighting and the flat-shading extra
  vertex left it at 87%; with one draw call per batch and the first draw's data it went back to 45%). Not descriptor or UBO binding either
  (binding the first draw's data only: 85%). **Fix:** `native_xform.comp`, two compute passes per recorded frame (a thread per strip vertex: position,
  fog, colour, motion into a scratch record; a thread per triangle corner: drop the ADC triangles, write the HwVertex), so a batch of strips is one
  ordinary draw again. 38% -> 52-59% busy; the rest is the passes themselves (~10k strips of ~25 vertices, ~700k corners a frame). Granite here has
  no SPIRV-Cross: a compute program needs its `ResourceLayout` declared by hand (without it the bindings are silently empty).
- **CPU:** the VIF1 worker is the busy thread (EE blocked on it ~20 ms a frame at 36 fps). Per draw it did: a 2.5 KB `NativeDraw` zero-filled and
  malloc'd (freed on the GS thread: scudo cross-thread, ~30% of the worker), `classifyStrip` (double maths per vertex, 7-9%), `nativeClippedReference`
  for the ~2% of strips the volume's edge cuts (11-13%: vu1hle allocates a vector per kicked packet), two GIF packets (header, marker) each drained
  at once, a mutex on the draw queue contended with the GS thread (cas on the Thor's little cores: 15-25%), counters (atomics) on every draw.
  **Done:** draws come from a pool (an entry is free when only the pool holds it; vectors keep their capacity; no cross-thread free), the queue is
  lock-free (the marker packet's passage through the GIF queue orders the publication), the packets are handed over every 16th draw, counters count
  only while logging, the vertex copy is an `assign` (no zero fill first). Peach Town at 60 Hz: 36 fps -> 60 fps.
- **What is left:** the worker still spends ~1.6-1.9 GHz-equivalent on native draws against ~1.4 for the whole old path (the lean-compiled VU1 programs run a
  strip in ~1 us), and the GS thread ~1.6-1.8 against ~1.2 (`processNativeMarker`, `SubmitNative`, `openFor`, `record`). Per program on the Thor
  (`RT_NATIVE_ONLY=<programs>` takes only those on the worker): 2 and 5 together +0.1 worker, +0.3 GS; 4 alone +0.4 worker, +0.6 GS, +70 MHz GPU;
  3 and 6 together neutral. So at 60 Hz in Peach Town and 120 Hz in Fuji City native on is still no better than the VU1 + GS path, and where it is
  worse the frame skip (Auto) starts skipping 3D frames and pausing generated frames: that is the screen flashing seen with `RT_NATIVE_DRAW=1`
  (skip levels 1-2 in the `[perf]` lines, `shown` falling from 120 to 40-56 and back). It cannot beat the old path while each strip costs a few
  microseconds of marshalling; the floor for a native draw has to be ~0.2 us.
- **The design that gets there** (not built): the model chains never change, so parse each once (address -> batch list: program, tag, vertex block,
  material registers) and keep the vertices on the GPU; the VIF1 worker then needs no UNPACK and no copy of a model's vertices, only each object's
  matrices from the setup run, written as one record (~0.4 KB) into a per-frame array; classification becomes a per-model bounding box against the
  object's clip matrix (only a box that crosses the volume takes the CPU's slow path); the frontend gets one marker per run of draws, not per draw.
  The model cache below is the first piece (it keys on content hash; keying on the chain's guest address, which the chain walk records with
  `RT_NATIVE_DRAW_LOG`, avoids hashing and the copy).

**Model cache** (`NativeModel`, `RT_NATIVE_MODEL_CACHE=1`; off by default: hashing each block cost the worker a fifth of its time): the block a model chain
unpacks, W lanes dropped (V3 UNPACKs leave W as the last data had it), looked up by a 64-bit CRC32C key of X, Y, Z of each record, 99.7-100% hits
(5,000-20,000 models, 1-5 MB); the backend puts a model in a 32 MB arena (host-visible storage buffer) when it has been drawn twice. The ribbon at
0x299430 is the only chain the game rewrites (a new block each frame, never drawn twice, streamed). Pictures are bit-identical with and without the
cache and arena (`test_model_cache_changes_nothing`). Strips of programs 2 and 5 drop their own copy of the vertices when they have a model;
`nativeVerts()` rebuilds it (W = 0) for what runs on the GS thread (the reference, CPU triangles).

**Batching** (`Batch::moreNative`): native draws that follow each other with nothing between and the same state share one `Batch`; with the compute
pass the batch's vertices are one run, drawn with one call.

**Attribution** (`RT_NATIVE_DRAW_LOG=1`): the VIF1 worker keeps, per DMA chain, the guest source address of each payload
(`PS2Memory::vif1UnpackSource`), so each draw knows where its vertex block came from. The 5 s log lists the top 20 chains: region, the `vifCallModel`
call (0x227048, hooked in `overrides.cpp` while logging) whose chain starts at or before it and its return address (the renderer), gpu /
cpu-reference / fallback counts by reason, and how many distinct blocks the chain produced (more than one = not static). `python3
scripts/native_attrib.py <app.log>` names the renderers from `config/game_symbols.toml`. Callers of 0x227048 and the chains they pass: field tiles and
their extra layer and the scene functions pass cell chains from the tables at 0x177AB20..0x177AB80; cars and wheels `carModels` / 0x177EAC8.. + 0x10;
ELF chains by constant: car shadow 0x290570 (`drawCarsField`, `drawCarsAction`, 0x223588), 0x290670 (0x2238A0), 0x290770 (0x223B30), minimap markers
0x290870 / 0x290940 (0x242D20..0x243398), sky 0x290A40 / 0x2962E0 / 0x298A50, arc ribbon 0x299430 (`drawArcRibbon`, the only static writer into a
chain: nothing else stores into 0x290000..0x29A000 chain data); props 0x177F5xx model pointers (0x25BE98..0x25D5F8), coins 0x335718. In a Peach
Town drive the draws come from `drawCar` (cars, 0x221f2c) and `drawCoins` (0x241e50) among the entry-2 chains; terrain cells are entry 4.

Switches: `RT_NATIVE_PATH=vertex` draws each strip with native.vert (one call each; the old design), `RT_NATIVE_MODEL_GPU=0` keeps the cache's blocks out of
the arena, `RT_NATIVE_ONLY=<programs>` (worker) and `RT_NATIVE_DRAW_ENTRIES` (GS thread, the rest built as packets) for A/B.

### What the native path still takes from the packet GS (to cut when the old renderers leave the app)

- **GS state** arrives as GIF packets through `GS::processGIFPacket` / `writeRegisterUnlocked`: every batch's header (TEX0, CLAMP, ALPHA, TEST, ... from TOP + 402)
  is submitted as a PATH1 packet, the 2D/PATH2/PATH3 traffic too, and `processNativeMarker` writes PRIM from the draw's tag. The draw's state
  (`buildDrawBatch`, `m_drawStateSerial`), the texture (`resolveTexture`: GS memory reads, CLUT, paletted decode, texture packs) and the frame/Z targets all
  come from the frontend's register model. A native renderer wants a draw record that carries its material (TEX0/CLAMP/ALPHA/TEST/FRAME/ZBUF) itself.
- **Ordering** with the rest of the GS traffic is the marker packet in the PATH1 stream (one 32-byte packet per draw plus the header packet), and the
  VU1 microprograms still run for the setup entries (0, 1, 7: camera and object matrices into VU memory, which `buildNativeDraw` reads back as
  `state.vf` / VU data) and for everything the native path declines (`RT_NATIVE_ONLY`, a bad tag, a deopt).
- **Classifier and layout**: `WideLayout` (HUD vs 3D verdicts, `vertexClass`, `xScale`/`xBias`), `MotionTracker` (`curC`/`prevC`) and the UI mask / map layer's
  CPU triangles (`nativeScreenTriangles`, built from `nativeReference`'s packet) are the frontend's; the declined cases fall back to
  `nativeReference` + `processGIFPacket`.
- **The CPU's share**: clipped programs (3, 4, 6) classify strips on the VIF1 worker (`classifyStrip`) and make the clipped polygons with vu1hle
  (`nativeClippedReference`), then draw them as GIF packets.
- **The backend**: `HwBackend::SubmitNative` still goes through `openFor` / `sameDraw` (the packet path's batch merging) and `Batch`es recorded by `record()`.

### Model chains taken whole, marker runs (RT_NATIVE_CHAINS=1, opt-in)

The VIF1 worker can recognise a model chain's payload in the stream (the DMA chain walk records each payload's guest address and length, `srcMap`;
`nativeChainFind` checks it byte for byte against its parsed copy) and draw its batches from the parse: nothing is unpacked into VU memory, the tag, the
vertex block (kept as `NativeModel`, also the GPU arena's block), the material (as the GIF packet) and the bounding box come from the parse, a strip whose
box is inside the clip volume or wholly beyond one plane is classified from the box (`classifyBox`), anything else goes through `classifyStrip` and the
reference as before, and a run that needs the microprogram gets its UNPACKs written into VU memory first (`nativeChainMaterialize`). Markers go out as one
packet per run of consecutive draws (`queueNativeMarker`, at most 48; the frontend loops `processNativeMarker` over the run).
Measured on the Mac (town, `RT_PERF_LOG`): model chains on or off changes the worker's busy time by under 1 point, because **every model chain the game
calls is exactly one batch (one strip)**: there is nothing to amortise across a chain, and the per-strip costs (the pooled draw, the constants, the
publish, the GS thread's `processNativeMarker` / `openFor` / `SubmitNative`) are the same either way. The hook that would remove them is above the
chain: the renderer functions (or `vifCallModel` plus the object-matrix UNPACK) emitting one record per object and the GS taking runs of draws as one
`SubmitNativeRun`; a GPU-driven per-object table with indirect draws is the design that can get a strip under ~0.2 us.
Mac town at 60 fps (`VU1` / `GS` busy %): old path 19.5 / 15.5, native 25.8 / 44.5; race 10.8 / 5.5 against 13.0 / 7.5. The GS thread's extra is mostly the
per-draw frontend (classifier, `nativeScreenTriangles` for HUD draws), the device-lock waits in `openFor` (texture creation while the presenter holds it)
and recording; the worker's: the clipped-polygon reference (~17%) and `classifyStrip` (~7%).
