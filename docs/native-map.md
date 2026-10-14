# The game's maps: what its code does, and how we draw them (re/native-map)

Notes from Ghidra (`scripts/ghidra/fn.py`, names in `config/game_symbols.toml` under "Maps") and runtime
probes. Addresses are the USA release (SLUS-20398). Nothing here is game code or data.

## 1. What the game does

Every map in Road Trip is **a static model drawn through the 3D path** (VU1 program entry 5, untextured
strips, no lighting) under a view block uploaded with `vifUploadAt4`, clipped by a GS scissor: the game has
no separate 2D map renderer. Each map draw starts with the 3-qword packet at 0x2A43B0 (`vifDirect`: A+D
TEST_2 = 0x3380B, ALPHA_2 = 0x8000000044), which is why the GS side recognises a map by TEST_2 alone
(`[map] gs_test`). The colour sets for the strips come from `vifUploadColours(set, 0x2A4360)`.

| Routine | Who | What |
|---|---|---|
| `gs_map_draw_functions_town` 0x242438 (param: subject block 0x177ACE0, zoom mode 0/1, task context) | `townMapTask` 0x242FE8 | the town and field-area HUD map |
| `gs_map_draw_functions_race` 0x2427D0 (param: race task) | race tasks 0x22DDD0 (Adventure/Quick Race), 0x254430 | the course map |
| `gs_map_draw_functions_scissored` 0x242A88 (task, split index) | mini-game race tasks 0x251B60, 0x25A120 | split-screen / mini-game course maps |
| `townMapBezel` 0x249208 | `townMapTask` | the frame: three UiBatch sprites (TBP 7828, PSMT4, CBP 7892, CSA 10; 30..174 x 152..212) |
| `raceMapMarkers` 0x242D20, `raceMapPickups` 0x243398, `townMapCoins` 0x241E90, `mapDot` 0x243338 | tasks | the cars, pickups and coins |

### 1.1 The view block (`mapViewMatrix` 0x21F920)

Builds the 8-qword block `vifUploadAt4` sends: an identity with the translation `(x, y)` at +0x20/+0x24,
scale `s` in the x axis and `s * DAT_00332CAC` (0.47: a field is half a frame high) in the y axis, then the
constant offset vector 0x2A43E0 `(-800, -200, -800, 1)` is multiplied in and the model's own tile-origin
matrix (`tileSlotOrigin` 0x221288, per quadrant from `tileQuadrantOrigins` 0x29D080) goes in `vifObjectMatrix`.
`DAT_00333288` is the game's own wide flag (the x scale is multiplied by `DAT_00332CA8`); it is 0 in every
configuration we run (the widescreen option narrows the HUD on the GS side instead).

**Town / field area** (north-up, never rotates): view (x, y) =
`(zoom.x0 + 1728 - px*s/200, zoom.y0 + 1936 + pz*s/200 * 0.47)` with `(px, pz)` the subject block's x and z
(the car), `zoom` = `townMapZoomTable` 0x2A42C0: mode 0 `{302, 89, s 50}` (near), mode 1 `{202, 136, s 25}`
(far); mode byte 0x177A215 toggled by Select (`townMapTask` XORs bit 0 of its state byte when the pad
mask 0x1780A5A has the button). That is the `[map]` formula in `config/game_state.toml`:
`fx = 102 + (x - px) * s / 200`, `fy = 183 - (z - pz) * 0.47 * s / 200` in field pixels, the car at
(102, 183). The window is `mapScissorTown` 0x2A43F0: SCISSOR_2 x 45..158, y 156..207.

The map is up to four models, one per loaded tile quadrant (`fieldMapModels` 0x177AB70 + 4 * slot,
task context +0x60/+0x64/+0x68/+0x6C = the quadrants' tile ids, `tileSlotLoaded` 0x221258 says whether the
slot holds that id): each `drawModelParts(model, 2)` (part B), through its own `vifObjectMatrix`.
Colour set: 0x2A4330 (white) or 0x2A4340 (grey) by `task+0x14 - 0xE484 < 0x1944D` (a clock range: the
day / night look), then the other sets: markers red 0x2A4370, town cars green half-alpha 0x2A4390, coins
yellow 0x2A43A0. The player's mark is the arrow chain 0x290870 (`chainMapArrow`) at the car, heading
`DAT_0177AE24 * 2^-15 * pi`; the town's drivers are dots (`chainMapDot` 0x290940) from the table at
0x177B058 (0x270-byte entries, +0xAF a location index into 0x2BE43C). A coin is one dot per coin in
`gs_map_coin_table` 0x2A3970 (100 x {x, y, z, tile code}) whose tile is loaded and not collected
(`FUN_0023DBF0`). Radius of a marker: `50 / s` scales the 50-unit model.

**Race** (`raceMapPlacements` 0x2A4050, 24 entries of `{x, y, scale, rotation}` by course byte `+0x22`):
the model (slot 2, 0x177AB78) is drawn once, rotated by the entry's angle (`FUN_00272C38`) and scaled by
its scale, view x = `entry.x + 1728`. Colour 0x2A4350 `(0.2, 0.2, 0.6, 0.7)`: the translucent track. The
pad bits in the task (+0x2E, +0x32: 0x2000/0x8000 x, 0x1000/0x4000 y, 2/8 scale, 4/1 angle) edit the entry
live (a leftover layout editor: `FUN_0026DE80` prints it). No scissor: the track's box is the map's. The
scissored variant (0x2A42E0: `{442|127|-188, 1910, 105, 0}` by split index) sets SCISSOR_2 from 0x2A4430
(x 48..208, y 68..200) for the two-player maps; the others 0x2A4410 (36..126, 156..207).

Markers (race): player 1 red arrow, player 2 teal arrow (`task+10 & 2`), rivals green dots (any entry whose
status word `+0x198` is non-zero and has no 0x30 bits), finished or special entries (0x30 set) red or teal
dots by bit 0x40; pickups yellow dots from 0x177EB40. All scaled `50 / entry.scale`.

## 2. What we draw from

The captured-batch path: the hardware GS sees map batches by TEST_2 = 0x3380B and draws them into a layer of
its own (see CLAUDE.md, "The game's map"). With native draws on (the default on the hardware GS) the strips
reach the GS as `NativeDraw` records, not packets: `SubmitNative` builds their screen triangles on the CPU
for map draws, `noteMapBox` reads them and `record()` sends the draw to the layer (`Batch::map`).

The static data above is all a renderer needs to draw a map without the layer: the model (strips under
entry 5), the view block (`mapViewMatrix`), the scissor and the colour sets. `MapView.cpp`
and the second screen already compute the projection (`rt::game::mapPoint`) for their marks.

## 3. Defects found and fixed

(See the log at the end of this file.)
