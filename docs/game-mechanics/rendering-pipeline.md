# 3D rendering pipeline: scene draw, cars, day/night lighting, and the VIF1/GIF submission path

How a frame of Road Trip's 3D world is drawn: the per-viewport scene draw functions
(`drawSceneField`/`drawSceneRace`/`drawSceneAction`/`drawSceneCloudHill`), the car
renderer (`drawCarsField`/`drawCarsAction` → `drawCar` → `drawCarExtraParts`, with
`drawModelParts` as the generic part-A/B dispatcher), the day/night lighting and fog
model (`updateTimeOfDayLight`), and the VIF1/GIF DMA submission path everything is
built on (`vifBeginPacket`…`vifKick`, `vifDirect`, `vifCallModel`,
`sendTexturePackets`), plus how the shipped app's native draw path replaces the VU1
microprograms.

Grounded in `docs/native-reference/functions.tsv`, `config/game_symbols.toml`,
`config/game_state.toml`, `docs/native-render.md` (model formats, VU1 memory map,
native draw) and read-only Ghidra decompiles (`SLUS_203.98`, `r5900:LE:32:default`)
of `0x221300`, `0x220938`, `0x220D40`, `0x2228A8`, `0x222B70`, `0x221788`,
`0x2234C8`, `0x225988`, `0x226F70`, `0x227048`, `0x227108`, `0x227178`, `0x2271F0`
and their neighbours. Claims not directly observed are marked `[inferred]`. The
model-file formats, VU1 microprogram entries and vertex layouts are documented in
`docs/native-render.md` and are not repeated here.

## How it works

Each frame, the main loop `gameMainLoop` (`0x2003B8`) opens the **main** VIF1 packet
(`vifBeginPacket(0)` → `0x1750BF0`) for the 2D/HUD layer, runs the task pool,
appends the UI batch flush and the frame blend, and kicks it. The 3D scene is drawn
by a separate per-viewport task: `renderInit` (`0x221300`) gives the task its camera
block (`cameraBlocks` `0x177E6D0` + n*0x190 by the viewport field at task `+0x1C`)
and installs the scene draw function from the location byte (scene `+0x22`):
`0xFF` → `drawSceneField` (towns and field areas), `0x22` → `drawSceneCloudHill`,
`0x12`/`0x1C`/`0x20` → `drawSceneAction` (maze, tunnel race, curling), anything
else → `drawSceneRace` (race courses). The scene functions build **one VIF1 DMA
chain per frame in a fixed order — no depth sorting** (the GS Z buffer does that);
culling is per terrain cell and per car. `drawSceneField`
(`0x220938`) in order: update the light direction, pick which of the 4 tile
quadrants the player is on, `vifBeginPacket(1)` (the **second** buffer
`0x1730BF0`), sky dome + sky layers, camera + scene params + MSCAL entry0, the fog
packets, the 4 loaded tile quadrants' cells, cars, props, coins, animated props,
then the camera again and the translucent pass (arc ribbon, each tile's 65th
chain, car effects), `vifEndPacket`, `vifKick`.

`drawSceneRace` (`0x220D40`) is the same skeleton over one course tile: it walks
the 64 cell chains of the course tile slot (the table entry at `0x177AB28`,
index 2 of `fieldCellTables` `0x177AB20`), culling each 200×200-unit cell with
`cellVisible` (`0x21FA38`), with the tile origin for `vifSetBatchOffset` taken from
`0x29D0B0` (an entry of `tileQuadrantOrigins` `0x29D080`), and draws the 65th
chain (`+0x100`) in the translucent pass. It also handles split-screen: viewport
0 begins and ends/kicks its own chain; in a two-player race, viewport 1 opens the
second buffer and draws without ending it, and viewport 2 appends into that same
open buffer, restores the full-screen GS packet and ends/kicks — both players' 3D
in one chain. Each non-zero viewport first sends a per-viewport GS packet from
`0x29D020 + n*0x20` `[inferred: the half-screen scissor]`. `drawSceneAction`
(`0x220FE8`) draws the mini-game maps and is the only caller of
`drawCarsAction` (`0x222B70`).

The car pass (`drawCarsField` `0x2228A8`, called by the field, race and cloud-hill
scenes) walks the 24 car records at `0x177AC50 + k*0x270` (Ghidra
`gs_controls_car_object`; the physics field map is in
[car-physics.md](car-physics.md)). Pass 1 — shadows: for every active car
(status `+0x198` non-zero) it converts the fixed-point world position `+0xA0` to a
float vec4 relative to the tile origin (`0x21C728`), uploads 8 colour floats
({1,1,1,0.5} plus the four floats at `+0x144..+0x174` `[inferred: the car's shadow
tint]`) to VU1 0x18–0x19, sends the car's object matrix (`+0x00`) through
`vifObjectMatrix`, sets the slot's TEX0/CLAMP (TBP0 `0x243E + slot*0x44`, CBP
`0x247E + slot*0x44`) via `vifBatchRegs`, and calls the shadow-quad ELF chain
`0x290570` (VU1 entry 5, unlit). Pass 2 — bodies: the camera-projection ×
car-matrix product's depth term ÷ the camera block's focal distance (`+0x18C`)
gives a per-car LOD factor; cars below `0.00313` (`0x332CF4`) are skipped (a
near-plane/back cull, `[inferred]`; described as a projected-size threshold in
`docs/native-render.md`), the rest go to `drawCar`. Pass 3 — extras:
`vifUploadColours` from `0x29D2D0/0x29D2E0`, then `drawCarExtraParts` for each car
that drew. `drawCarsAction` is the same three passes plus, per car, one extra
`drawCar` with a mirrored object matrix, a flipped light vector (the `0x177F5E0`
light block with its direction negated) and a fixed far LOD factor `0.03`
(`0x332CFC`) before the real draw `[inferred: a reflection on the mini-game
floors]`.

`drawCar` (`0x221788`) draws one car from its appearance record (pointer table at
`0x177EA50`, indexed by the model slot at car `+0x194`): the record's 24-bit paint
word supplies six nibbles that index `paintIntensity` (`0x29D150`, 16 floats
0.1–0.8) into the 8 colour floats uploaded to VU1 0x18–0x19; the light matrix is
one of four time-of-day blends (`lightMatrixBlend 0x177F620 + n*0x40`, n = bits
12–13 of the car's `+0x19C`), its rows scaled by a fade factor from the car's
transformed depth minus the value at `+0x11C` (`0x226058`
ScaleCarLightMatrix); the object matrix and body TEX0 follow, then the body model
— model 0 (near) if the LOD factor is below `0.03` (`0x332CC0`) **and** the
per-frame near-body budget (max 6, counter `0x3356E8`, reset each `drawCarsField`)
isn't spent, model 1 (far) otherwise — the body variant (models 2/3 by record bit
`0x1000`), the optional extras (models 4/5 by record bits `0x2000`/`0x4000`), up to
ten part models from the table at `0x177EAC8` (each with its own attachment offset
from `0x2ED728 + paintSlot*0x44`, object matrix, and a shared-CLUT CSA), and the
wheels: `drawCarWheelsNear` (`0x221570`, only at LOD factor exactly 0 — the
no-animation/shop path), `drawCarWheelsMid` (`0x2213E8`) below `0.05`
(`0x332CCC`), else one far wheel model with the tyre texture (TBP `0x1C44 +
tyreType*2`); record bit `0x400` swaps in a four-model wheel style with per-wheel
rotations from the car's `+0x80/+0x84/+0x88`. `drawCar` returns the body LOD used
(0 near, 1 far). `drawCarExtraParts` (`0x222360`) then draws part A of that
LOD-chosen body model and part B of the near body model plus the variant model,
using the model header's part offsets/counts gated by the car's flag bytes
`+0x1FC` (bit 0) and `+0x1FE` (bit 1). `drawModelParts` (`0x2234C8`) is the generic
version of that gate — mask bit 0 draws the chain at `model + partA_offset` when
its batch count is non-zero, bit 1 the part-B chain — and is how the minimaps draw
part B of a tile's map model.

Day/night and fog come from `updateTimeOfDayLight` (`0x225988`, called each frame
by the race task `0x20F3B8`, the town task `0x2105B8` and `ApplyTimeOfDayLighting`
`0x26CA48`). The clock argument (vblanks since midnight) selects a phase — night
below 49,500 (`0xC15C`), dawn ramp 49,500→58,500, day 58,500→153,000, sunset
153,000→162,000, nightfall 162,000→~166,500 — and writes the three blend weights
(x day, y dusk, z night) into `sceneParams` (`0x177F720`, uploaded to VU1 0x13),
blends each of the four rows of the two light sets (`todLightDay/Dusk/Night`
`0x29D500/0x29D540/0x29D580` and the second set `0x29D5C0/0x29D600/0x29D640`) into
`lightMatrixBlend` (`0x177F620`) and `lightMatrixBlend2` (`0x177F660`) — row 0 is
the colour-set weight vector (VU1 0x14/VF9), rows 1/2 the fog sets A/B — and
writes the fog pair `fogFarParam` (`0x177F738`/`+4`), the fog colour
`fogColourRGBA` (`0x177F750`) and the sky-layer tints at `0x177F754..0x177F757`.
A height argument (clamped to 300) picks between the normal fog set (distance
ramps ~300–800) and a heavy-fog set at or below −2.5 `[inferred: underground/
water]`. Per scene, `updateLightDirection` (`0x2261A0`) points the light from the
light block `0x177F5E0` at the camera, and the scene functions push the rebuilt
FOGCOL packet (`gifAdPair` `0x21DB48` writing the A+D register 0x3D at `0x172FF50`)
plus fixed PATH2 packets (`0x29CFC0`, `0x29CFF0`) through `vifDirect`. The sky
(`drawSkyDome` `0x225608`, `drawSkyLayers` `0x2256A8`) is static ELF chains
(`0x290A40`, `0x2962E0`, `0x298A50`, VU1 entry 6) whose colour floats are driven by
the time-of-day weights, drawn first with the camera position as the matrix origin
(entry 7).

Everything above converges on one submission path. The packet helpers
(`0x2263B8`–`0x227270`) append DMA tag entries at `vifPacketPtr` (`0x3356EC`),
bounds-checked against the open buffer's limit (`0x17523D0` main / `0x1750BD0`
second) with `fatal` on overflow; `vifCallModel` (`0x227048`) emits the tag that
jumps the VIF1 into a model's **prebuilt, static** VIF chain (loaded once from
disc, never rewritten — the arc ribbon `0x299430` is the only chain rewritten per
frame); `vifDirect` (`0x226F70`) sends a PATH2 GS packet by DIRECT; `vifKick`
(`0x2271F0`) flushes the cache, waits for VIF1 idle and starts chain-mode DMA
(CHCR `0x145`). Texture upload is a separate, GIF-channel path: the loaders call
`sendTexturePackets` (`0x207B50`), which programs the GIF DMA (FlushCache, wait
channel 2, QWC 0/TADR/STAT 3/CHCR `0x105`) to walk a file's prebuilt GS upload
chain (CNT tags up to END), with `gifChainEndTag` (`0x207D80`) appending the END
tag when a packet is built rather than read from a file. The shipped app can also
**bypass the microprograms**: its native draw path intercepts the MSCAL of VU1
programs 2–6 on the VIF1 worker, copies the setup run's registers and the vertex
block into a GPU draw, and emits a 32-byte marker into the PATH1 stream where the
XGKICK would have been (details, fallbacks and verification in
`docs/native-render.md`).

## Key functions

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x2003B8` | gameMainLoop | Frame loop: opens the main VIF1 packet, runs tasks, appends the UI batch flush + frame blend, kicks, swaps buffers | boot (`0x200008`) | `0x227108`, `0x227178`, `0x2271F0`, `0x203810` (uiBatchFlush), `0x208120` (frameBlend), task walker |
| `0x221300` | renderInit | Per-viewport setup: camera block by viewport field (task `+0x1C` → `+0x28`), picks the scene draw function by the location byte, resets car-effect slots | `0x20EBE8` | `0x225588`, `0x2213E0` |
| `0x220938` | drawSceneField | Towns/field: light direction, player quadrant pick, then one chain — sky, camera/params/fog, 4 tile quadrants, cars, props, coins, animated props, translucent pass, end, kick | renderInit | `0x2261A0`, `0x21C880`, `0x2201C0`, `0x225608`, `0x2256A8`, `0x21FB38`, `0x2228A8`, `0x2232E8`, `0x241D38`, `0x25D028`, `0x2207D0`, `0x220030`, `0x225120`, VIF helpers |
| `0x220D40` | drawSceneRace | Race courses: 64 culled cells of the course tile (slot 2), split-screen viewport/chain sharing, race props (locations 0x19/0x1A), translucent 65th chain | renderInit | `0x21FA38`, `0x225608`, `0x2256A8`, `0x222F08`, `0x2228A8`, `0x2232E8`, `0x225120`, `0x227048`, `0x226610`, VIF helpers |
| `0x220B20` | drawSceneCloudHill | Location 0x22 variant of the field scene (adds the faded arc ribbon) | renderInit | as `drawSceneField` + `0x2207D0` |
| `0x220FE8` | drawSceneAction | Mini-game maps (0x12 maze, 0x1C tunnel race, 0x20 curling); only caller of `drawCarsAction` | renderInit | `0x222B70`, `0x21FA38`, `0x225608`, `0x2232E8`, `0x225120`, VIF helpers |
| `0x21FB38` | drawFieldTiles | Per loaded quadrant: `vifSetBatchOffset` (origin from `0x29D080`) + 64 cells of 200×200 units, each culled by `cellVisible` and drawn via `vifCallModel` | drawSceneField | `0x21FA38`, `0x227048`, `0x226610` |
| `0x21FA38` | cellVisible | Frustum test of a 200-unit cell centre | `0x21FB38`, `0x220D40`, `0x220B20`, `0x220FE8` | `0x21D0C0` |
| `0x2228A8` | drawCarsField | 24 cars: shadow quads (chain `0x290570`, per-car tint + matrix + TEX0), then `drawCar` with the LOD cull and near-body budget, then `drawCarExtraParts` | `0x220938`, `0x220D40`, `0x220B20` | `0x21C728`, `0x221788`, `0x222360`, `0x226B18`, `0x226C28`, `0x227048`, `0x226F70`, `0x2266E8`, `0x2267A8`, `0x226940`, `0x226A18`, `0x2729E0`, `0x272758` |
| `0x222B70` | drawCarsAction | As `drawCarsField` for the mini-game maps, plus a mirrored far-LOD `drawCar` with a flipped light vector per car `[inferred: reflection]` | drawSceneAction | as `drawCarsField` |
| `0x221788` | drawCar | One car: paint nibbles → `vifUploadColours`, light-set blend + fade, object matrix, body TEX0, near/far body (LOD `0x03` + budget 6), variant (bit 0x1000), extras (models 4/5), up to ten parts, wheels (3 levels); returns the body LOD | `0x2228A8`, `0x222B70`, `0x2236D8` | `0x226058`, `0x221570`, `0x2213E8`, `0x226B18`, `0x226C28`, `0x226D58`, `0x227048`, `0x226A18`, `0x272728`…`0x272C38` |
| `0x222360` | drawCarExtraParts | Part A of the LOD-chosen body (mask bit 0 ← car `+0x1FC`) and part B of the near body + variant models (bit 1 ← `+0x1FE`), via the model header offsets/counts | `0x2228A8`, `0x222B70`, `0x22AEB0` | `0x226B18`, `0x226C28`, `0x227048`, `0x272758` |
| `0x2234C8` | drawModelParts | Generic part A/B dispatcher: mask bit 0 → chain at `model + partA_offset` (count non-zero), bit 1 → part-B chain | map draw fns (`0x242438`/`0x2427D0`/`0x242A88`), `0x22CC38`, `0x25D328` | `0x227048` |
| `0x221570` | drawCarWheelsNear | Close wheels with tyre detail (LOD factor exactly 0 — the no-animation/shop path) | `0x221788` | `0x272AE8`…`0x272C38`, `0x227048`, `0x226B18`, `0x226A18` |
| `0x2213E8` | drawCarWheelsMid | Mid-distance wheels (LOD factor < 0.05) | `0x221788`, `0x221F70` | as near, + `0x226D58` |
| `0x225988` | updateTimeOfDayLight | Clock phase → blend weights (x day, y dusk, z night) into `sceneParams`, two 4-row blended light matrices, fog pair/colour, sky tints | `0x20F3B8`, `0x2105B8`, `0x26CA48` | `0x272988` (scale), `0x272940` (add) |
| `0x2261A0` | updateLightDirection | Light direction = light position (`0x177F5E0`) − camera position (block `+0x180`), normalized | 12 scene/menu functions | `0x2727E8` |
| `0x226058` | ScaleCarLightMatrix | Scales the chosen light matrix's rows by a fade factor from the car's transformed depth (no fade above −2; floors 0.5/0.25 below) and re-uploads it | `0x221788` | `0x272970`, `0x226868` |
| `0x225608` | drawSkyDome | Sky colours (`0x2262D8` by location), camera upload, view block, unit matrix + camera position via entry 7, dome chain `0x290A40` | the 4 scene functions | `0x2262D8`, `0x2263B8`, `0x226F70`, `0x2267A8`, `0x226A18`, `0x227270`, `0x227048` |
| `0x2256A8` | drawSkyLayers | Two sky layers (chains `0x2962E0`/`0x298A50`) tinted by the night/day weights; the second is skipped at Cloud Hill | `0x220938`, `0x220D40`, `0x220B20` | `0x226A18`, `0x226C28`, `0x227048` |
| `0x227108` | vifBeginPacket | Opens packet 0 (main `0x1750BF0`) or 1 (second `0x1730BF0`, saving the main pointer); `fatal` on misuse | `0x2003B8` + 8 scene/menu functions | `0x2006F0` |
| `0x227178` | vifEndPacket | Writes the END tag (MSKPATH3 off), closes the buffer, returns the chain start | the same 9 | `0x2006F0` |
| `0x2271F0` | vifKick | FlushCache, wait for VIF1 DMA idle, then QWC 0 / TADR / STAT 2 / CHCR `0x145` — chain-mode start | the same 9 + `0x226398` | FlushCache |
| `0x226F70` | vifDirect | Appends a 16-byte entry sending n quadwords of a PATH2 GS packet by DIRECT (fog colour, scissor, test/alpha state); bounds-checked | 40 callers | `0x2006F0` |
| `0x227048` | vifCallModel | Appends the tag that jumps the VIF1 into a model's prebuilt VIF chain (its own STCYCL/UNPACK/MSCALF batches); bounds-checked | 41 callers | `0x2006F0` |
| `0x226B18` | vifObjectMatrix | Object matrix (4 qw) → VU1 0, FLUSH, MSCALF entry 1, BASE `0x8D`/OFFSET `0x1AE` | 35 callers | `0x2729E0`, `0x2006F0` |
| `0x226C28` | vifBatchRegs | REGLIST GIF tag + 2 GS registers (e.g. TEX0_2 + CLAMP_2) to TOPS+0x192 — how cars and props get their texture | 11 callers | `0x2006F0` |
| `0x2263B8`/`0x226480`/`0x226540` | vifUploadCamera / vifUploadSceneParams / vifCameraSetup | Camera block → VU1 4..0x12 (15 qw); scene params (`0x177F720`) → VU1 0x13 (3 qw); MSCAL entry 0 + double-buffer BASE/OFFSET | the scene functions | `0x2006F0` |
| `0x226610` | vifSetBatchOffset | Tile origin (quadword from `tileQuadrantOrigins`) → VU1 0x1A, added to terrain model positions | terrain/ribbon draw functions | `0x2006F0` |
| `0x207B50` | sendTexturePackets | Programs the GIF DMA channel (FlushCache, `sceDmaSync` wait, QWC 0/TADR/STAT 3/CHCR `0x105`) to send a file's prebuilt GS texture upload chain | 43 loaders/effect submitters | `sceDmaGetChan`, `sceDmaSync`, FlushCache |
| `0x207D80` | gifChainEndTag | Appends one END DMA tag (with qwc) to a packet being built, for `sendTexturePackets` | the texture-packet submitters | — |

## Data structures / globals

### VIF1 packet builder state

| Address | Name | Meaning |
|---|---|---|
| `0x3356EC` | `vifPacketPtr` | Write pointer of the chain being built; 0 = none open (helpers call `fatal` `0x2EAFA0`) |
| `0x3356F0`/`0x3356F4` | `vifPacketSaved`/`vifPacketSecond` | Main pointer saved while the second buffer is built; 1 while building it |
| `0x1750BF0`/`0x1730BF0` | packet buffers | Main and second VIF1 chain buffers; limits `0x17523D0`/`0x1750BD0` — overflow → `fatal` (`0x2EAFC0`) |

### Camera block (`cameraBlocks` `0x177E6D0` + viewport*0x190)

`renderInit` picks the block by the viewport field (task `+0x1C`; 2 → `0x177E860`).
Render-relevant fields: `+0x180` camera position (used by
`updateLightDirection` and the sky matrix), `+0x18C` focal distance — the car LOD
factor divides the car's view depth by it. Full layout in `docs/native-render.md`.

### Time-of-day, lighting and fog

| Address | Name | Meaning |
|---|---|---|
| `0x177F720` | `sceneParams` | The 3 qw uploaded to VU1 0x13: x = day weight, y = dusk, z = night (plus the fog parameters consumed by the microprograms) |
| `0x29D500`/`0x29D540`/`0x29D580` | `todLightDay/Dusk/Night` | Light set 1: 4 qw per row entry, blended by the weights |
| `0x29D5C0`/`0x29D600`/`0x29D640` | `todLight2Day/Dusk/Night` | Light set 2 |
| `0x177F620` (+ n*0x40, n = 0..3) | `lightMatrixBlend` | The blended 4-row matrices (row 0 = colour-set weights → VU1 0x14/VF9; rows 1/2 = fog sets A/B); each set's last word forced to `0x43000000` (128.0). Cars pick the set by bits 12–13 of car `+0x19C` |
| `0x177F660` | `lightMatrixBlend2` | The second blend (`0x177F620 + 0x40`) |
| `0x177F738`/`+4` | `fogFarParam` | Fog far parameters, written from the clock phase and the height argument |
| `0x177F750` | `fogColourRGBA` | Fog colour; `0x177F754..0x177F757` the sky-layer tint floats (read by `SetDrawSkyColours` `0x2262D8` and `drawSkyLayers`) |
| `0x332CC0` etc. | LOD/fade floats | `0x332CC0` = 0.03 (body near→far), `0x332CCC` = 0.05 (mid wheels), `0x332CC4` = 0.42 / `0x332CC8` = −0.63 (wheel spacing/base), `0x332CF4`/`0x332CF8` = 0.00313 (car draw cull), `0x332CFC` = 0.03 (the mirrored-car LOD), `0x332DCC`/`0x332DD0` = 0.01 (light-matrix fade slopes) |
| `0x3356E8` | near-body counter | Near car bodies drawn this frame; `drawCar` switches to the far body past 5 (max 6 near), reset by `drawCarsField`/`drawCarsAction` |

### Car model tables and the appearance record

- `carModels` `0x177F310` + slot*0x18 — six model pointers per car slot: 0 near
  body, 1 far body, 2/3 body variants (record bit `0x1000`), 4/5 extras (bits
  `0x2000`/`0x4000`). Model data at `0x3FBB80 + slot*0x34000`
  (`loadCarModelAt`, textures rebased to TBP `0x243E + slot*0x44`).
- Model header `{partA_offset, partB_offset, partA_batches, partB_batches}` —
  `drawModelParts`/`drawCarExtraParts` draw the chain at `model + offset` (byte
  offset = `(offset + 0xF) >> 4` rounded, times 4/0x10) only when the matching
  batch count is non-zero; the main chain is at `model + 0x10`.
- `0x177EA50` — pointer per car slot to its appearance record `[inferred name]`:
  word 0 = 24-bit paint word (six nibbles → `paintIntensity` `0x29D150`); byte
  `+4` = paint-slot index (part attachment offsets at `0x2ED728 + n*0x44`);
  byte `+5` = wheel CLUT (CSA); byte `+6` = tyre type (TBP `0x1C44 + n*2`);
  byte `+7` = parts-texture index (TBP `0x1BE4 + n*10`); word `+8` (low 16 bits) =
  equips bitmask (bits 0–9 part models, `0x200` an extra part, `0x400` the wheel
  style, `0x1000` the body variant, `0x2000`/`0x4000` extras).
- Parts model table `0x177EAC8`.. — the equipped-part models (wheels, wings,
  jets). Part 6 is the wheel set: near body draws a base model (`0x177EAE0`)
  plus four instances (`0x177EAE4`) at z offsets −0.63 + i*0.42, spun by the angle
  at car `+0x88`; the far body draws a single model (`0x177EAF4`).
- Car object `0x177AC50 + k*0x270` (24 cars) — render-relevant fields:
  `+0x00` object matrix (shadow quads), `+0x40` body matrix (`drawCar`'s product),
  `+0x80/+0x84/+0x88` wheel rotation angles, `+0xA0/+0xA8` fixed-point world
  position (shadow relative vector, quadrant pick), `+0x11C` the light-fade
  reference, `+0x144..+0x174` four shadow-tint floats `[inferred]`, `+0x194`
  model slot, `+0x198` status word (non-zero = active), `+0x19C` bits 12–13 the
  light set, `+0x1FC`/`+0x1FE` the part A/B flags for `drawCarExtraParts`.

### Terrain, tiles and ELF chains

- `fieldCellTables` `0x177AB20` — per tile slot, a pointer to 65 cell chain
  pointers; the race scene uses the slot-2 table at `0x177AB28`, and each tile's
  65th chain (table `+0x100`) is the translucent overlay (water etc.).
- `tileQuadrantOrigins` `0x29D080` — per-quadrant origin qwords for
  `vifSetBatchOffset` (the race scene uses `0x29D0B0`).
- Quadrant bytes `0x3335B8..0x3335BB` — per tile-slot state, reset to 0xFF, the
  player's quadrant set to 3, then updated by `DrawFieldTerrainTiles`
  (`0x2201C0`) as neighbour tiles stream in.
- ELF chains in `.vutext`: `chainCarShadow` `0x290570` (entry 5),
  `chainSkyDome` `0x290A40`, `chainSkyLayerA/B` `0x2962E0`/`0x298A50` (entry 6),
  `chainCarEffectA/B` `0x290670`/`0x290770`, `chainArcRibbon` `0x299430` (the
  only chain the game rewrites per frame, via `drawArcRibbon` `0x2224F8`).
- Fixed PATH2 packets: `0x29CFC0`/`0x29CFF0` (fog and translucent-pass GS state
  `[inferred]`), `0x29D240`/`0x29D290`/`0x29D2F0` (car-pass GS state), `0x29D020 +
  viewport*0x20` (per-viewport, `[inferred: scissor]`), the FOGCOL A+D packet
  rebuilt per frame at `0x172FF50` (GIF register 0x3D), sky colour scratch
  `0x29D470..0x29D4A0`.

## Control flow

**Per frame** (`0x2003B8`): `vifBeginPacket(0)` → run the task pool → the 2D/UI
batch flush (`uiBatchFlush` `0x203810` also uses `vifDirect` into the open
packet) → frame blend → `vifEndPacket`/`vifKick` of the main chain → buffer swap.
The 3D viewport task draws into the second buffer and kicks it mid-frame
(`vifKick` waits for the previous VIF1 transfer to finish).

**The scene chain** (field order; the race/action variants differ as above):
`updateLightDirection` → quadrant pick → `vifBeginPacket(1)` → `drawSkyDome` /
`drawSkyLayers` → `vifUploadCamera` + `vifUploadSceneParams` + `vifCameraSetup`
(MSCAL entry 0) → `gifAdPair` FOGCOL + `vifDirect` fog packets → terrain cells
(`drawFieldTiles`, or the inline 64-cell walk on courses) → `drawCarsField` →
props/coins/animated props → camera again → `vifDirect` translucent state → arc
ribbon / the 65th chains / `drawCarEffects` → `vifEndPacket` → `vifKick`.

**The car pass** (`drawCarsField`): view/light blocks up → per car the shadow quad
(chain `0x290570`) → GS state restore + light block again → per car the LOD
factor; cull below 0.00313; `drawCar` → `drawCarExtraParts` for cars that drew.
Inside `drawCar`: colours from the paint word → light matrix (set by car
`+0x19C`, faded by `0x226058`) → object matrix → body TEX0 → body model by LOD →
variant → extras → parts (each with its own matrix and shared-CLUT CSA) →
wheels; the returned LOD feeds the part-A model choice in `drawCarExtraParts`.

**Texture upload** (load time, GIF channel): `loadFieldTile`/`loadCarModel`/
`loadCarModelAt`/`loadCourseTile` call `sendTexturePackets` on the file's
prebuilt GS upload chain (built by `gifChainEndTag` when assembled in RAM); the
per-frame animated texture copies (`texAnimCopyA/B`) reuse the same DMA
mechanism (`docs/native-textures.md`).

**Native draw** (the shipped app's renderer, not game code): the VIF1 worker
intercepts the MSCAL of microprograms 2–6, builds a GPU draw from the setup run's
VF registers, VU memory constants and the vertex block, and leaves a 32-byte
marker in the PATH1 stream; the GS thread expands it (records path
`RT_NATIVE_RECORDS=1`: per-program constants, deduped material blocks, strips
rejected by the clipper dropped). Clipped programs (3/4/6) are classified on the
worker (`classifyStrip`), with exact clipped polygons drawn as GIF packets before
the strip; anything the path declines falls back to the bit-exact `nativeReference`.
On by default for the hardware GS on desktop, off on Android (one draw per strip
is GPU-bound there). Full detail, switches and verification:
`docs/native-render.md`.

## Open questions

- The exact DMA tag encoding inside the 16-byte entries `vifDirect`/
  `vifCallModel` append (CNT/REF word layout observed, semantics taken from the
  symbol notes) — `[inferred]`.
- Part bit 9 of the appearance record's equips word appears unreachable in the
  `drawCar` decompile (the loop exits at part 8) — a decompiler artifact or a
  legacy slot `[inferred]`.
- What sets the car's light-set index (car `+0x19C` bits 12–13) and which of the
  four blends each value means — the contact word's ground material is a
  candidate `[inferred]`.
- `drawCar`'s second argument (car `+0x11C`) in the light-matrix fade: observed
  as the subtracted reference in the fade factor, semantics unknown.
- `drawCarsAction`'s mirrored pre-draw (flipped light vector, negated matrix
  diagonals, fixed LOD 0.03): a reflection for the mini-game floors
  `[inferred]`.
- The clock's units and wrap point ("vblanks since midnight"): phase boundaries
  observed (49,500 / 58,500 / 153,000 / 162,000 / ~166,500; ramps 9,000 / 9,000 /
  4,500), but the wall-clock mapping and why blend weights reach 1.1 (clamped in
  the VU?) are unresolved.
- The register contents of the fixed PATH2 packets (`0x29CFC0`, `0x29CFF0`,
  `0x29D240`, `0x29D290`, `0x29D2F0`, `0x29D020 + n*0x20`) — read as "fog /
  translucent / car-pass / viewport state" from their call sites only
  `[inferred]`.
- The exact time-of-day → colour blend of the entry 3/4/6 vertex vectors and the
  spare bit 33 / word 3 of the output GIF tag — open in `docs/native-render.md`.
- Which scenes reach `drawCarShowroom` (`0x221F70`) beyond the intro/shop rig
  (`0x2236F8`) — the garage/car-select guess is from the symbol notes only.
