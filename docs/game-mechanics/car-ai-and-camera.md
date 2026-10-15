# Car AI / traffic and the chase camera

Two subsystems documented here: (1) the AI that drives non-player cars — waypoint
tracking, steering/throttle generation, race rivals and town traffic; and (2) the
chase camera task cluster that follows a car, eases FOV/pitch, and avoids the
terrain.

Grounded in `docs/native-reference/functions.tsv`, `config/game_symbols.toml`, and
read-only Ghidra decompiles (`SLUS_203.98`, `r5900:LE:32:default`) of `0x21A990`,
`0x21CE28`, `0x24FCD8`, `0x24FED8`, `0x250068`, `0x2508E0`, `0x253230`,
`0x253680`, `0x21DC18`, `0x21E070`, `0x21E690`, `0x21EC58`, `0x21F698`, plus a
byte-level check of `0x21D0B8`. Companion doc: [car-physics.md](car-physics.md)
(car state object, control-word bits). Claims not directly observed are marked
`[inferred]`.

---

## 1. Car AI and traffic

### How it works

Every car — player, rival, or town traffic — reaches the same input dispatch:
`AcquireVehicleInput` (`0x21A990`), called from the master vehicle tick
`UpdateVehiclePhysics` (`0x21BA70`). The dispatcher routes by the car's status
word at `+0x198`: cars with kind bits `& 3` set read the pads through
`gs_controls_player_control` (`0x21A820`); cars with bit `0x80` (AI-owned) call
the AI controller function pointer `DAT_003356dc` with `(scene, car, navIndex,
flags)`. Before dispatching, if the car has a waypoint table (`+0x250 != 0`) the
dispatcher runs `UpdateVehicleWaypointProgress` (`0x250068`), whose return value
is that `navIndex`. The dispatcher also handles the record/playback paths: sys
flag `0x40` replays a stored input ring (12-byte records walked from
`sys+0x44`, end sentinel `0x1FFFF9`, exhausted → status bit `0x200`), sys flag
`0x20` records one, and a scripted branch (scene byte `+0x20 == 0x18`) drives the
car by AI while topping the nitro accumulator `+0x23C` by `0x200`/frame (cap
`0x40000`) and forcing status bit `0x400`. The tail of the function force-feeds
gas (control `1`) when the car is slow under status bit `0x100`, and control `2`
when route bit `0x800` is set (the "arrived" bit set by `CheckCarArrivedAtZone`
`0x259498`).

The controller pointer is installed by `SetAICarController` (`0x21CE28`), a
20-byte setter that defaults to the stub at `0x21D0B8` when passed NULL. The stub
is exactly `jr ra; move v0, zero` — a null controller that returns "no input"
(verified by disassembly; Ghidra has no function defined there). Two installers
exist: town mode (`UpdateTownBlockTasks` `0x20E708`) passes `0x2508E0`
directly — this is the town-traffic brain; race mode (`StartRaceModeTask`
`0x20F950`) passes a function pointer loaded from the scene descriptor
(`*(scene+0x10) + 0xC`), so each race course supplies its own AI controller
`[inferred: per-course rival behaviour]`.

AI state is created per car by `CarAiInit` (`0x24FCD8`) at spawn
(`SpawnCarEntityTask` `0x218458`): it clears a 0x100-byte per-car AI buffer at
`0x1759D70 + k*0x100` (pointer in car `+0x25C`), then binds two course tables —
a waypoint node table (`+0x250`, 0x18-byte records = two 2D segment endpoints
plus a second segment) and a navigation graph (`+0x254`, 8-byte records: node
index, target index, branch candidates, and an attribute byte copied to car
`+0x24B`) — via the course lookup `0x25FBC0` (debug print `carAIinit T%02d, %d`).
Cars without a course path (town traffic) instead get a behaviour table at
`+0x258` chosen from `DAT_002A9B80 + idx*0x100` (16 default 0x100-byte route
tables, copied by `CopyAiRouteTables` `0x250DD0`), the fallback `DAT_002ABF80`,
or a per-player variant derived from the save-slot block `0x1780A60`.

`UpdateVehicleWaypointProgress` (`0x250068`) localizes the car on the course:
side-of-segment cross-product tests walk the nav graph from the car's current
node (`+0x24A`) forward or backward through branch records until the car sits on
the correct side of its segment, then records the node attribute (`+0x24B`),
computes the target heading from `atan2` of the target node's segment (scaled by
`DAT_00332FA8` into 0x10000 angle units) against the car heading `+0x1D4`, counts
heading error beyond ±0x5000 in the off-course counter `+0x248`, and stores the
lateral offset to the target segment midpoint in `+0x24C` (cos/sin helpers
`0x274938`/`0x274A10`). It returns the next nav-graph index. The controller
`CalculateAiSteeringAndThrottle` (`0x2508E0`) then converts that into a control
word: `ComputeWaypointTargetAngle` (`0x24FED8`) produces one or two packed
heading angles (aim mode = car `+0x194 & 3`: segment start, midpoint, or both),
the current speed is compared as an index (`speed * 0xD8 / 0x28000`) against the
per-node target-speed byte in the route table (low 7 bits), yielding throttle
level 0 (coast) / 1 (gas) / 2 (brake); a speed-cap byte at `+0x246` overrides it.
Steering error beyond ±0x4000 sets one of the steer bits `0x2000`/`0x8000`
(gated by car `+0x19A`), huge errors (±0x4000 with sys flags `& 0xC == 4`)
instead nudge the heading accumulator `+0x1D4` directly by ±0x100. Per-node
bytes in the AI buffer carry a turn-phase marker (`0x80`/`0x40`) plus a small
countdown that trims the node's speed target up or down — plausibly a per-car
racing-line/difficulty adjuster `[inferred]`. The result is OR'd with the same
control bits the player path produces (value `9` = gas + jet appears when
on-target `[inferred]`).

Race rivals are spawned by `SpawnRaceRivalTasks` (`0x20ED30`, participant slot
table `DAT_01787008`); a second, simpler steering chain exists for scripted
drives: `UpdateAiCourseSteering` (`0x253680`) + `SteerAiToDefaultWaypoint`
(`0x253448`) + `SetAiSteeringTarget` (`0x253230`), a three-state machine (state
at route entry `+0x70`, control word at `+0x74`, sub-state at `+0x78`, stride
0xC) that drives toward fixed per-cell targets (`DAT_002AC4A0 + cell*0x40`, with
headings at `DAT_002AC480 + cell*0x40`, arrival radius 20 units) and settles the
heading with hysteresis thresholds (±0x6000 / ±0x4000 / ±0x2001 / ±0x80),
emitting control words `0x2001`/`0x8001`/`0x2005`/`0x8005`/`1`/`2` and masking
to the steer bits `0xA000` over the speed limit. It is reached only through
`0x254CC8` (no listed callers) — presumably installed as a task callback by the
tuning-garage (`0x254430`) or street (`0x2595F0`) scene tasks `[inferred]`.
Finally, `UpdateTrafficHazard` (`0x240CD8`) is a town-side script: it tests a
trigger quad (`IsPointInsideQuad` `0x240BE0` over the car position `+0x90`),
advances a timed hazard state and mutates vehicle properties (can call
`ResetVehicleState` `0x21C998`); `SaveAIDataFile` (`0x20E5B8`) dumps the AI
buffer to `AI Save %s` for debugging.

### Key functions

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x21A990` | AcquireVehicleInput | Routes player vs AI vs replay input per car; runs waypoint progress; tops nitro `+0x23C`; forced gas/brake tails | `0x21BA70` | `0x250068`, `0x258E50`, `0x21A820`, `DAT_003356dc` (fn ptr) |
| `0x21CE28` | SetAICarController | Installs `DAT_003356dc`; NULL → stub `0x21D0B8` | `0x20E708`, `0x20F950` | — |
| `0x21D0B8` | DefaultAiController | 8-byte stub (`jr ra; move v0, zero`): returns 0 = no input | fn ptr default | — |
| `0x24FCD8` | CarAiInit | Per-car AI setup: 0x100-byte buffer `0x1759D70+k*0x100`, waypoint tables `+0x250/+0x254`, route table `+0x258` | `0x218458` | `0x25FBC0`, memset, printf |
| `0x250068` | UpdateVehicleWaypointProgress | Nav-graph walk via side tests; target heading, off-course counter `+0x248`, lateral error `+0x24C`; returns next nav index | `0x21A990` | `0x274AF8`, `0x274938`, `0x274A10`, `0x203A50`, `0x27EEE0` |
| `0x2508E0` | CalculateAiSteeringAndThrottle | Town-traffic controller: throttle 0/1/2 from speed vs node target, steer bits `0x2000`/`0x8000`, heading nudge ±0x100, per-node trim | `0x20E708` (installed as `DAT_003356dc`) | `0x24FED8` |
| `0x24FED8` | ComputeWaypointTargetAngle | Packed target heading(s) from a waypoint segment and car pos; 3 aim modes | `0x2508E0` | `0x274AF8` |
| `0x253680` | UpdateAiCourseSteering | Scripted-drive steering state machine toward fixed cell targets | `0x254CC8` | `0x253230`, `0x253448`, `0x272958`, `0x272970`, `0x274AF8`, `0x274B10` |
| `0x253448` | SteerAiToDefaultWaypoint | State-0 steering toward the default waypoint target | `0x253680` | `0x253230`, `0x2727E8`, `0x272940`, `0x272958`, `0x272970`, `0x272988`, `0x274AF8`, `0x274B10` |
| `0x253230` | SetAiSteeringTarget | Heading-error hysteresis (states 0/1/2) → control word `0x2001`/`0x8001`/`0x2005`/`0x8005`/`1`/`2`; speed-limit mask `0xA000` | `0x253448`, `0x253680` | `0x21D358` |
| `0x20ED30` | SpawnRaceRivalTasks | Spawns rival car tasks; fills participant table `DAT_01787008` | `0x20F950` | `0x204ED0`, `0x23DB80`, `0x204C48` |
| `0x259498` | CheckCarArrivedAtZone | Arrival thresholds (`DAT_003330AC..0xB4`, speed 117/83) → route bit `0x800`, parked flags `0x177AD40` | `0x2595F0` | — |
| `0x240CD8` | UpdateTrafficHazard | Town traffic hazard: trigger quad, timed state, vehicle property changes, possible `0x21C998` | (task-driven) | `0x21C998`, `0x23D628`, `0x23D778`, `0x240BE0` |
| `0x250DD0` | CopyAiRouteTables | Copies 16 default 0x100-byte route tables from `DAT_002A9B80` | `0x2277B0`, `0x23DA58` | — |
| `0x20E5B8` | SaveAIDataFile | Debug dump of the AI buffer (`AI Save %s`) | `0x26D020` | sceIo write path |

### Data structures / globals (AI)

| Address / field | Meaning |
|---|---|
| `DAT_003356dc` | AI car-control fn ptr `(scene, car, navIndex, flags) -> control word`; default `0x21D0B8`; installed by `0x21CE28` |
| car `+0x198` | Status word: kind bits `& 3` (player), bit `0x80` = AI-owned, `0x100` auto-gas, `0x200` replay-exhausted/odometer gate, `0x400` scripted-AI, `0x800` arrived (route bit) |
| car `+0x194` | Low 2 bits: waypoint aim mode (segment start / midpoint / both) |
| car `+0x19A` | AI steering enable gate (steer bits suppressed when 0) |
| car `+0x246` | Speed-cap byte overriding the node target speed |
| car `+0x248` | Off-course counter (heading error beyond ±0x5000) |
| car `+0x24A` | Current nav-graph node index |
| car `+0x24B` | Current node attribute byte (from nav record `+6`) |
| car `+0x24C` | float lateral offset to the target segment midpoint |
| car `+0x250/+0x254` | Waypoint node table (0x18/record) and nav graph (8/record: node, target, branches, attribute) |
| car `+0x258` | Route/behaviour table ptr: `DAT_002A9B80 + idx*0x100`, `DAT_002ABF80`, or `0x1780A60`-derived per-player variant; per-node bytes = target speed (low 7 bits) |
| car `+0x25C` | ptr to the 0x100-byte AI scratch buffer (turn-phase/trim bytes per node) |
| `0x1759D70` | AI buffer array, 0x100 bytes per car |
| `DAT_002A9B80` | 16 default route tables (0x100 bytes each) |
| `DAT_002ABF80` / `DAT_002AC080` / `DAT_002AC098` | Fallback route table and route-selection index maps |
| `DAT_002AC4A0` + cell*0x40 | Scripted-drive target cells: X/Z target, heading (`DAT_002AC480 + cell*0x40`), thresholds at +0x10/+0x14/+0x18 |
| `DAT_01787008` / `DAT_01787016` | Race participant slot table / finish positions |
| `DAT_00332FA8`, `DAT_00332F9C/0x332FA0/0x332FA4`, `DAT_00332FAC` | Angle scale constants (heading to 0x10000 units; per-aim-mode scales) |

### Control flow (AI)

Per frame, inside the per-car tick `0x21BA70` (see [car-physics.md](car-physics.md)):

1. `0x21A990` AcquireVehicleInput.
2. If car `+0x250 != 0`: `0x250068` UpdateVehicleWaypointProgress → nav index,
   `+0x24A/+0x24B/+0x24C/+0x248` updated.
3. AI branch (`+0x198 & 0x80`, or the scripted `sys+0x20 == 0x18` path):
   `(*DAT_003356dc)(scene, car, navIndex, flags)` → control word. Town traffic
   runs `0x2508E0` → `0x24FED8`; race rivals run the course-installed pointer.
4. Player branch: `0x21A820` reads the pads; replay branches read/append the
   12-byte input ring.
5. The returned control word continues through the normal physics path
   (steering ramps, engine model, etc.).

Scripted drives instead run `0x254CC8` → `0x253680` → (`0x253448`) → `0x253230`
each tick, writing the control word into the route entry `+0x74` for the host
task to consume. Course setup order: `0x20ED30`/`0x20E708` spawn cars →
`0x218458` SpawnCarEntityTask → `0x24FCD8` CarAiInit → per-frame as above.

---

## 2. Chase camera

### How it works

The camera is a chained task: a small record whose `+0x8` is a frame counter and
whose `+0xC` is the next handler function; `InitializeTask` (`0x204D20`) swaps
handlers and each state tail-calls the chain. `InitCameraTaskState` (`0x21E4D8`)
boots the chain (spawning `SelectAndBuildCamera` `0x238B18` or loading preset
type `0x10`/`0x20`), and the steady-state handler is `RunCarCameraTask`
(`0x21E690`), driven per frame from `0x20EBE8`. The chain can divert to
`RunCameraZoomTransition` (`0x21EC58`) on car respawn/reset, to
`CameraWaitOrCutsceneState` (`0x21ED90`) when the scene requests a fixed camera,
and to the preset-switch pair `SwitchCameraPresetType10`/`Type20`
(`0x21EE98`/`0x21EF98`) or `CycleCameraPresetState` (`0x21E328`, view-button
cycle mod 3). Camera state lives per port at `DAT_0177E9F0 + port*0x20`; the
rendered matrices land in the per-viewport block `cameraBlocks` (`0x177E6D0`,
0x190 bytes per viewport).

One frame of `RunCarCameraTask` works like this. On the first frame it zeroes
the chase spring state and calls `SetViewDistanceGlobals` (`0x21F910`) with
2048-unit distances. Each frame it re-selects the view distance by camera mode
(mode 2 shortens the far plane to ≈1992, mode 3 to `DAT_00332C4C`) and skips
everything while the game is paused (sys `+0x28 & 0x2000`). On the first setup
frame and on the view button (action-map "view" mask at `DAT_01780A56`) it picks
a camera preset through `LoadCameraPreset` (`0x21FAE8`, 32-byte records at
`0x29CE00`): index = variant bit (task flags `& 2`) OR'd with 4 when the car's
mode block bit `0x400` is set (the wing/travel mode of car-physics), or
presets 8/9 in scene kind `0x1A`; a view-button switch snaps the chase spring
state to the current car-relative offset (rotated by car heading `+0x1D4`) so
the camera doesn't swing. Pitch aims at the preset's `+0x14` short (plus `0x500`
while the car is wading: car `+0x213` with mode bit `0x100`), slewed at most
±0x80 per frame into the pitch short at cam state `+0x14`; a yaw-follow short at
`+0x1A` tracks the negative of the car's yaw offset `+0x1D6` (counter-steer
vis), and the pitch/zoom short at `+0x16` decays toward zero ±0x80/frame. Debug
mode (sys `+0xB`) routes input to `ApplyFreeCameraInput` (`0x21F198`). Then the
frame commits: `ComputeChaseCameraBasis` → fixed→float car position
(`FixedPosToFloatVec4` `0x21C7F0` over car `+0xA0`) → `buildCamera` →
`ResolveCameraTerrainCollision`.

`ComputeChaseCameraBasis` (`0x21DC18`) turns the preset offset (a vec4 at cam
state `+0x10`) and the car's heading/pitch into two camera basis vectors. Two
modes: with cam mode `+0x1C` nonzero it smooths the chase offset through a
per-port spring block (`DAT_0172FF30 + port*0x10`: smoothed lateral offset and
height plus their rate-limits, constants `DAT_00332C30..00332C44`, a genuine
spring-damper feel); with `+0x1C == 0` the camera sits fixed behind the car at a
height taken from the spring block's third float. The offsets are rotated by the
car heading (`+0x1D4`, angle scale `DAT_00332C2C`) and by the cam pitch/yaw
shorts (`+0x16`/`+0x1A`, scale `DAT_00332C48`), then normalized
(`0x2727E8`) into two basis vectors fed to `BuildYawTransform` (`0x208008`) and
`ApplyMatrixRotation` (`0x208080`).

`buildCamera` (`0x21F698`) assembles the 0x190-byte camera block: either from
euler angles via `ComposeCameraMatrix` (`0x21F340`) or, when cam flags bit
`0x1000` is set, from supplied basis rows (fixed scene cameras). It runs
`BuildCameraViewMatrices` (`0x21F450`) twice — view/frustum matrices with the X
axis mirrored — builds the projection from the view-distance globals
(`DAT_003356E0/E4`) and the FOV/zoom float at cam state `+0x10` (scaled by
`DAT_00332C84`, forced 1.0 when `DAT_00333288` is set), and writes matrices at
block `+0`, `+0x40`, `+0x80`, `+0xC0`, `+0x100`, the camera position copy at
`+0x180` (consumed by `updateLightDirection` `0x2261A0`), and the zoom at
`+0x18C`. `ResolveCameraTerrainCollision` (`0x21E070`) then probes two camera
points (block `+0x100` and `+0x110`) through the terrain-height function
pointer `DAT_00335710`; if either clips (query returns −1 or the point is pushed
below its intended height) it steps the cam pitch short (`+0x16`) by ±0x80 and
rebuilds via `buildCamera` until both points clear the ground or the pitch
passes zero. The probe's region selector comes from `GetGridCellIndex`
(`0x21C880`) of the car's fixed position, or the constant 2 outside course
scene kind −1.

The zoom transition (`0x21EC58`), entered when sys `+0x28` bit `0x400` (the
spawn/reset bit) is set: the cam state's pitch/zoom short `+0x16` starts at
`0x8000` and ramps `+0x200`/frame while the counter `+0x8` is in `0x40..0x7F`;
once the counter reaches `0x80` it clears the `0x400` flag and hands the handler
back to `RunCarCameraTask`. `CameraWaitOrCutsceneState` (`0x21ED90`) holds until
a player flag clears and otherwise builds fixed scene cameras (preset types
`0x17`/`0x18`) from stored matrices; `SelectAndBuildCamera` (`0x238B18`) picks
the nearest camera block for the current car with follow/zoom flags and
delegates to `0x21ED90` for type −1. Menu scenes get their own camera:
`BuildMenuSceneCamera` (`0x21F098`) eases a spin angle (`DAT_0029CCD0`) and zoom
for scene `0x16`.

### Key functions (camera)

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x21E690` | RunCarCameraTask | Per-frame chase camera: preset/FOV/pitch easing, view-button cycling, chase basis → buildCamera → terrain pass | `0x20EBE8`, `0x21EC58`, `0x21ED90` | `0x21C7F0`, `0x21C880`, `0x21DC18`, `0x21E070`, `0x21E4D8`, `0x21EC58`, `0x21F198`, `0x21F910`, `0x21FAE8`, `0x21FB20`, `0x21F698`, `0x274938`, `0x274A10` |
| `0x21DC18` | ComputeChaseCameraBasis | Smoothed chase offset from car heading `+0x1D4` and cam pitch/yaw; per-port spring limits; 2 normalized basis vectors | `0x21E690`, `0x21EC58` | `0x208008`, `0x208080`, `0x2727E8`, `0x274938`, `0x274A10`, `0x2729D0` |
| `0x21E070` | ResolveCameraTerrainCollision | Probes block points `+0x100`/`+0x110` via `DAT_00335710`; steps pitch ±0x80 and rebuilds until clear | `0x21E690` | `0x21F698` |
| `0x21F698` | buildCamera | Fills the 0x190 camera block (matrices `+0/+0x40/+0x80/+0xC0/+0x100`, position `+0x180`, zoom `+0x18C`) | 14 callers (all camera states + menus/options) | `0x21F340`, `0x21F450`, `0x2729D0`, `0x272758`, `0x272A48` |
| `0x21F340` | ComposeCameraMatrix | 4x4 camera matrix from euler angles at src `+0x14`, optional premultiplies | `0x21F698` | `0x207EC8`, `0x272B90`, `0x272C38`, `0x272728`, `0x2729D0`, `0x272758`, `0x272A48` |
| `0x21F450` | BuildCameraViewMatrices | View basis + frustum/projection matrices (X mirrored) | `0x21F698` | `0x2727A0`, `0x2727E8`, `0x272868`, `0x272728`, `0x2729D0`, `0x272A48` |
| `0x21EC58` | RunCameraZoomTransition | Respawn zoom ramp: `+0x16` from `0x8000`, +0x200/frame until counter `+0x8` ≥ `0x80`; clears sys `0x400`, resumes `0x21E690` | `0x21E690` | `0x21C7F0`, `0x21DC18`, `0x21F698` |
| `0x21E4D8` | InitCameraTaskState | Boot handler: spawns `0x238B18` or loads preset type `0x10`/`0x20`, chains next handler | `0x21E690` | `0x204D20`, `0x204C48` |
| `0x21E328` | CycleCameraPresetState | View-button cycle: state `+0x25` mod 3 over a 24-entry preset table | `0x21E4D8`, `0x21EE98`, `0x21EF98` | `0x204D20` |
| `0x21ED90` | CameraWaitOrCutsceneState | Wait/cutscene state: fixed scene cams (types `0x17`/`0x18`) from stored matrices | `0x21E690`, `0x238B18` | `0x204D20`, `0x21E690`, `0x21F698`, `0x2729D0`, `0x272A48` |
| `0x21EE98` / `0x21EF98` | SwitchCameraPresetType10/20 | Timed preset swaps (timer `+0x8` hits `0xC0`) alternating preset types `0x10`/`0x20` | each other, `0x21E4D8` | `0x204D20` |
| `0x238B18` | SelectAndBuildCamera | Picks the nearest camera block for the current car; type −1 → `0x21ED90` | `0x21E328`, `0x21E4D8`, `0x21E690`, `0x21EE98`, `0x21EF98` | `0x21ED90`, `0x21F698`, `0x274B10`, `0x272A48` |
| `0x21FAE8` | LoadCameraPreset | Copies a 32-byte preset record (stride 32) from `0x29CE00` into the cam state | `0x21E690` | — |
| `0x21FB20` | GetCarSlotEntry | 0x20-stride slot accessor over `DAT_0029CE00` (preset/slot data; `+0x14` pitch) | `0x21E690` | — |
| `0x21F910` | SetViewDistanceGlobals | Writes view-distance floats `DAT_003356E0/E4` from camera mode + options | `0x21E690` + scene renderers | — |
| `0x21F198` | ApplyFreeCameraInput | Debug free-cam movement per button bits (0x1/0x4/0x1000–0x8000) | `0x21E690` | `0x26D880` |
| `0x21F098` | BuildMenuSceneCamera | Menu scene `0x16` spin/zoom camera | `0x20EBE8` | `0x21F698`, `0x2729D0`, `0x272A48` |
| `0x21C7F0` | FixedPosToFloatVec4 | Car fixed-point position `+0xA0` → float vec4 (scale `DAT_00332C20`) | `0x21E690`, `0x21EC58` | — |
| `0x21C880` | GetGridCellIndex | 2-bit region selector from the fixed-position sign bits (terrain probe region) | `0x21E690` | — |

### Data structures / globals (camera)

| Address / field | Meaning |
|---|---|
| `DAT_0177E9F0` + port*0x20 | Camera state: `+0x10` float zoom/FOV, `+0x14` pitch target short (slewed ±0x80), `+0x16` pitch/zoom short (ramped in `0x21EC58`, decays ±0x80), `+0x1A` yaw-follow short (= −car `+0x1D6`), `+0x1C` mode int (view-button toggle; `0x1000` = supplied-basis/fixed cam) |
| `DAT_0172FF30` + port*0x10 | Chase spring state: smoothed lateral offset, its rate, smoothed height, its rate; zeroed on task frame 0 |
| `0x177E6D0` (`cameraBlocks`) | 0x190-byte camera block per viewport; matrices `+0/+0x40/+0x80/+0xC0/+0x100`, position `+0x180`, zoom `+0x18C` |
| `0x29CE00` | Camera preset table (32-byte records; `+0x10` zoom float, `+0x14` pitch short); 24 entries for the mod-3 cycle |
| `DAT_003356E0/E4` | View-distance near/far floats written by `0x21F910`, read by `buildCamera` |
| `DAT_00335710` | Terrain height-query fn ptr used by `0x21E070` (returns −1 = no hit, else clamps the point's Y) |
| `DAT_00332C20/C2C/C48` | Scales: fixed→float position, heading→radians, cam pitch→radians |
| `DAT_00332C30..00332C44` | Spring rate limits for the chase offset (lateral/height, per mode) |
| `DAT_00332C84`, `DAT_00333288`, `DAT_00332C88..00332CA4` | FOV/projection constants used by `buildCamera` |
| `DAT_0029CCD0` | Menu camera spin angle (scene `0x16`) |
| car `+0xA0` | Fixed-point world position fed to the camera each frame |
| car `+0x1D4` / `+0x1D6` | Heading accumulator / yaw offset driving chase rotation and yaw-follow |
| car `+0x184` | Mode block: word `+8` bit `0x400` selects camera preset variant 4, bit `0x100` (with car `+0x213` wading) adds pitch `+0x500` |
| `DAT_01780A56` | Action-map "view" button mask (per-port stride `0x3448`) that cycles the camera preset |

### Control flow (camera)

Per frame (task chain via `0x20EBE8`, from the town/race setup tasks):

1. Handler dispatch on the camera task record (`+0x8` counter, `+0xC` handler):
   `0x21E4D8` boot → `0x21E690` steady state, with side chains `0x21EC58`
   (respawn zoom), `0x21ED90` (cutscene), `0x21EE98`/`0x21EF98`/`0x21E328`
   (preset swaps).
2. `0x21E690`: frame-0 spring reset + view distance; pause gate; per-mode view
   distance; view-button preset switch (snap springs); pitch target from the
   preset (`0x21FAE8`/`0x21FB20`) with wading bump; pitch/yaw slew.
3. `0x21DC18` ComputeChaseCameraBasis (spring smoothing per port).
4. `0x21C7F0` car position → `0x21F698` buildCamera (→ `0x21F340`/`0x21F450`)
   into `cameraBlocks`.
5. `0x21E070` terrain probe (`DAT_00335710`), pitch step + rebuild loop.
6. `renderInit` (`0x221300`) picks the viewport's camera block for drawing; the
   renderer uploads it via `vifUploadCamera` (`0x2263B8`) and light comes from
   `0x2261A0` (block `+0x180`).

---

## Open questions

- The race-mode AI controller installed from `*(scene+0x10)+0xC` is a data-table
  function pointer; its actual code (whether it is `0x2508E0`, the `0x253xxx`
  chain, or per-course code) is unknown.
- Control-word bit semantics: `1` gas and `0x2000`/`0x8000` steer are solid;
  `2` (brake vs reverse), `5`, `8`/`9` (jet?) follow the
  [car-physics.md](car-physics.md) encoding and are partly `[inferred]`.
- The `0x253xxx` scripted-drive chain reaches the game only via `0x254CC8`,
  which has no listed callers; being installed as a task callback by the
  tuning-garage (`0x254430`) / street (`0x2595F0`) scenes is `[inferred]`.
- The AI scratch buffer's per-node bytes (`0x80`/`0x40` phase + countdown
  trim of the node speed target) look like a racing-line/difficulty adjuster
  `[inferred]`; exact semantics unknown.
- Waypoint/nav record layouts (`0x18`-byte nodes, 8-byte graph records) are
  reconstructed from the decompile; field names are `[inferred]`.
- In `0x21A990`, the scripted branch (`scene+0x20 == 0x18`, car `+0x24A/0x24B`
  comparisons against `'f'`, `+0x19A/+0x19B` role bytes) is a demo/escort mode
  `[inferred]`; the `& ~8` mask on the AI control word is unexplained.
- Cam state `+0x1C` bit `0x1000` ("use supplied basis") and the exact role of
  the `0x21FB20` slot records (`+0x14` pitch observed) are `[inferred]`.
- `DAT_00335710` (terrain height query) installer and signature details are
  unknown; its −1 return means "no ground" `[inferred]`.
- The pitch/zoom short `+0x16` is `0x8000` at transition start and ramps toward
  zero — read as a zoom-out sweep `[inferred]`; the `+0x500` wading pitch bump's
  in-game look is unknown.
- `0x21D0B8` is not a defined Ghidra function (8 raw bytes, label
  `LAB_0021d0b8`); its role as the null controller comes from `0x21CE28`'s
  default plus the disassembly.
