# Car physics, steering, terrain contact, and collision

How a Road Trip / Choro Q car drives: the per-frame physics tick, the steering and
engine/traction model, wheel and body terrain contact, and the wall, obstacle and
car-vs-car collision response.

Grounded in `docs/native-reference/functions.tsv`, `config/game_symbols.toml`,
`config/game_state.toml` `[controls]`, and read-only Ghidra decompiles
(`SLUS_203.98`, `r5900:LE:32:default`) of `0x21A310`, `0x21A088`, `0x21AF28`,
`0x21BA70`, `0x21A820` and `0x21CE08`. Claims not directly observed are marked
`[inferred]`.

## How it works

Every car in a scene is a task-pool entity created by `SpawnCarEntityTask`
(`0x218458`), which loads the equipped parts' performance parameters into the car's
physics fields (`LoadCarPerformanceSpecs` `0x2180C0`), computes a spawn placement at a
building door or start line (`SpawnCarAtDoor` `0x2182B0`), and installs
`UpdateVehiclePhysics` (`0x21BA70`) as the entity's per-frame update callback. The
callback receives the entity and the scene/sys state; the car's physics state object
(Ghidra `gs_controls_car_object`, the array at `0x177AC50` + k * 0x270) is reached
through the entity's `+0x28` slot `[inferred]`. One tick walks input, steering,
engine/traction, world-position commit, ground contact, surface/collision impulses,
and finally the body matrix and visual suspension — in that order.

Input is routed per car by `AcquireVehicleInput` (`0x21A990`): the player's car calls
`gs_controls_player_control` (`0x21A820`), which reads the pads through the per-port
action map (`gs_controls_action_map` `0x1780A48` + port * 0x3448) and packs a control
word (bit 1 gas, 2 brake, 5 reverse, 8 jet, 0x10 lights, 0x20/0x40 wing up/down,
0x2000/0x8000 the two steer directions from the analog stick); AI cars call a
function pointer (`DAT_003356dc`, default `0x21D0B8`, installed by
`SetAICarController` `0x21CE28`) and track course waypoints via
`0x250068`. Steering is a chain of ramps: `gs_controls_car_update` (`0x21A310`) walks
the steering angle at `+0x1CE` by ±1 per held frame up to ±0x20 and self-centers it
by ±4 per frame when released; the angle, scaled by the car's `+0x242` multiplier and
the speed, becomes the curvature at `+0x1CC`. `UpdateSteeringDamping` (`0x21A088`)
then slews the steering command at `+0x1D2` (clamp ±0x200, ±8 per frame), integrates
the yaw offset at `+0x1D6`, applies the command at half rate when it opposes the
current yaw (setting the skid bit 0x20 at `+0x1F8`) `[inferred]`, and pulls the yaw
offset back toward zero by `0x1EC000 / speed`, clamped ±0x400.

The engine and brakes live in `gs_controls_car_update` (`0x21A310`). It calls the
engine/gearbox model through the global function pointer at `0x3356D8` — the Ghidra
symbol is misleadingly named `gs_game_scene3d_handler` — with (gear `+0x1FF`, the
car-frame speed, the control word, nitro `+0x22C`, rpm `+0x1D0`); the model's return
byte is the new gear. The pointer is installed at scene setup by `0x21CE08` as one of
two models, `0x21CE48` or `0x21CF80`. Braking ramps `+0x1FE` up over 32 held frames
and the force is read from the car's brake curve at `+0x200` as
`curve[ramp - 1] * 10000 >> 5`; traction is `0x59 * grip` where the grip halfword is
`+0x21C[contact & 7]`, indexed by the ground's grip class.
`AdvanceVehicleMotion` (`0x218EE0`) integrates the rpm/tachometer (capped at 10000),
the arc distance `+0x1BC` and the frame's motion, producing a world delta that the
tick's transforms write back as the car-frame velocity at `+0xF0`; the signed forward
speed integrates at `+0x1B8` with steps limited by engine force (`+0x218` scaled by
grip) and clamped while braking. The master tick applies a quadratic drag
(`v^2 / +0x218`) to the forward force, the yaw force and the three fall speeds, adds
gravity (−0x59 per frame) to the vertical velocity while airborne, and commits the
velocity into the fixed-point world position at `+0xA0/+0xA4/+0xA8` (27-bit wrap on
X/Z, carry bit on Y).

Terrain contact is `ResolveGroundContact` (`0x21B3D0`): the seven contact points
(table `0x29CA80` in car space — three body points, four wheels) are transformed to
world space and ray-tested against the world mesh. `UpdateWheelSuspension`
(`0x21AF28`) compares each point's contact height against the suspension travel at
`+0x114 + i*0x10` (travel window `0xA3D7` ≈ 1.28 or `0x51EB` ≈ 0.64 in 32768
fixed-point, chosen by mode bit 0x400), snaps grounded points, returns a contact
bitmask (bits 0–6 per point, bit 0x100 near-ground), and runs the three body springs
(positions `+0x1DC/+0x1E0/+0x1E4`, rest 0x1000; velocities `+0x1E8/+0x1EC/+0x1F0`).
While a body point is off the ground its fall speed (`+0x1C0/+0x1C4/+0x1C8`)
accumulates the course gravity (~97 per frame); all three above zero means airborne,
and at touchdown a fall ≥ 0xB3 fires the landing rumble (pattern 1, strength
fall / 16). The contact words stored at `+0x19C..+0x1B4` carry the ground's material
(bits 8–10: 0 asphalt, 1 dirt/sand, 3 grass, 4 snow, 5 smooth/boards/ice, 6 water),
the tyre sound (bits 4–6, table `0x29CBA8`) and the grip class (low bits, indexing
`+0x21C`) — `-1` only when a point is pushed into a wall, never in the air.

Collision comes from three places. `QueryCarSurfaces` (`0x219ED8`) rotates the
course's collision quads into the car frame and tests them via
`TestQuadLateralMask` (`0x219CB0`), producing quadrant bits (1 front-left,
2 front-right, 4 rear-left, 8 rear-right); while a side scrapes a wall or obstacle
the game rumbles patterns 3/4/5 (strength 255, every 2–3 frames) using the
mask-to-pattern table at `0x2EAE60`. `ApplyCollisionImpulse` (`0x219660`) then maps a
collision code 0–15 (contact bits | surface bits) to velocity and yaw impulses in the
car frame and repositions the car; code 15 is a crash (flag, print, rumble).
Car-vs-car collision is a separate task, `UpdateCarCarCollisionsTask` (`0x2179D8`),
which pairs all cars, tests four check points against each car's bounding volume
(`TestCarCollisionBox` `0x2175D0`), and applies the response in
`HandleCarCollisionResponse` (`0x217830`) — impact rumble (pattern 6, strength 125),
recoil or interaction triggers, and on a hard crash `ResetVehicleState` (`0x21C998`),
which zeroes the velocity `+0xF0`, the suspension/steering accumulators and the
odometer and restores the springs' 0x1000 rest.

## Key functions

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x218458` | SpawnCarEntityTask | Creates a car entity: state, physics matrix, parts, spawn placement; assigns `0x21BA70` as the update callback | `0x20E708`, `0x20ED30`, `0x20F950`, `0x210430` | `0x2180C0`, `0x2182B0`, `0x21BA70`, `0x24FCD8`, `0x2574A0`, `0x258E50` |
| `0x2180C0` | LoadCarPerformanceSpecs | Loads equipped parts' parameters (gears, engine, tires, brakes) into the car's physics fields | `0x218458`, `0x21BA70`, `0x21CA90` | — |
| `0x2182B0` | SpawnCarAtDoor | Spawn position, ground elevation and heading for a car at a building entrance | `0x218458`, `0x21BA70`, `0x21CA90` | `0x274AF8` |
| `0x21BA70` | UpdateVehiclePhysics | Master per-frame vehicle tick: input, drag, motion commit, ground contact, surfaces, collision impulses, body matrix, suspension visuals | `0x218458` | `0x203A50`, `0x208008`, `0x217F10`, `0x2180C0`, `0x2182B0`, `0x219660`, `0x219ED8`, `0x21A990`, `0x21A310`, `0x21ACD0`, `0x21B3D0`, `0x21D2D8`, `0x21D358`, `0x258E50`, `0x26D700` |
| `0x21A990` | AcquireVehicleInput | Routes player vs AI input per car, keeps the input ring buffer, tops nitro `+0x23C` | `0x21BA70` | `0x250068`, `0x258E50`, `0x21A820` |
| `0x21A820` | gs_controls_player_control | Player control word from the pads and the per-port action map; horn sound | `0x21A990` | `0x258E50` |
| `0x21A310` | gs_controls_car_update | Per-car engine/brake/steering step: engine model call, steering ramp, brake ramp + curve, traction, motion, tire sound, steering damping | `0x21BA70` | `0x217C68`, `0x218EE0`, `0x21A088`, `0x21D2D8`, `0x27E840` (memset) |
| `0x21A088` | UpdateSteeringDamping | Steering command slew (±0x200, ±8/frame), yaw offset integration, countersteer half-rate + skid bit, speed-scaled centering clamp ±0x400 | `0x21A310` | — |
| `0x218EE0` | AdvanceVehicleMotion | Advances rpm/tachometer (cap 10000) and arc distance `+0x1BC`; writes the frame's world delta | `0x21A310` | `0x203A50`, `0x274B10`, `0x27EEE0` |
| `0x217C68` | UpdateCarTireSound | Tyre skid/friction sound and pitch by speed, slip and surface | `0x21A310` | `0x2586A0`, `0x258E50`, `0x259070` |
| `0x21B3D0` | ResolveGroundContact | Transforms the 7 contact points to world, ray-tests terrain per point, runs wheel suspension, sets airborne/landing | `0x21BA70` | `0x208008`, `0x208080`, `0x2194B8`, `0x21AF28`, `0x258E50`, VU0 helpers |
| `0x21AF28` | UpdateWheelSuspension | Per-point contact/travel test, body spring/damper integration, fall-speed gravity, landing rumble (pattern 1, strength fall/16) | `0x21B3D0` | `0x20ACB8` (rumble_start) |
| `0x219ED8` | QueryCarSurfaces | Rotates collision quads into the car frame, tests via `0x219CB0`, fires wall-scrape rumbles 3/4/5 | `0x21BA70` | `0x219CB0`, `0x23DE30`, `0x272C38`, `0x272A48` |
| `0x219CB0` | TestQuadLateralMask | Per-quad lateral band test; returns quadrant bitmask 1/2/4/8 | `0x219ED8` | `0x272728` |
| `0x219660` | ApplyCollisionImpulse | Maps collision code 0–15 to velocity/yaw impulses in car frame, repositions; code 15 = crash | `0x21BA70` | `0x21D2D8`, `0x258E50`, `0x20ACB8`, `0x27EA30` |
| `0x2179D8` | UpdateCarCarCollisionsTask | Car-vs-car pairs: broadphase + narrowphase + impact response | `0x20F950`, `0x210430` | `0x2175D0`, `0x217830`, `0x21D310` |
| `0x2175D0` | TestCarCollisionBox | Transforms 4 check points of one car into another's frame, tests bounding extents | `0x2179D8` | `0x272958`, `0x272728` |
| `0x217830` | HandleCarCollisionResponse | Collision rumble, sound, impact recoil or interaction triggers between two cars | `0x2179D8` | `0x21C998`, `0x21D310`, `0x258E50`, `0x20ACB8` |
| `0x21C998` | ResetVehicleState | Zeroes velocity `+0xF0`, suspension/steering accumulators, odometer; springs back to 0x1000 rest | `0x217830`, `0x240CD8`, `0x24FB80`, `0x2631A0` | `0x258E50` |
| `0x21ACD0` | UpdateSuspensionFlex | Visual suspension floats `+0x80/+0x88` from speed/arc; lean `+0x84` from steering | `0x21BA70` | — |
| `0x21CE08` | SelectScene3dHandler | Installs the engine/gearbox model pointer at `0x3356D8`: 0 → `0x21CF80`, nonzero → `0x21CE48` | `0x20F950`, `0x210430` | — |
| `0x21CE28` | SetAICarController | Installs the AI car-control fn ptr `DAT_003356dc` (default `0x21D0B8` on null) | `0x20E708`, `0x20F950` | — |
| `0x217F10` | UpdateContinuousSoundEffect | Continuous horn/boost sound with countdown | `0x21BA70` | `0x258E50` |
| `0x2194B8` | SteerHeadingVector | Heading vector for ground contact: rotate about Y, pivot by flags 0x40/0x80, lift Y, normalize | `0x21B3D0` | `0x2727E8`, `0x272C38`, `0x272728`, `0x272A48` |
| `0x21D2D8` | TransformVecByF15Matrix | VU0 f15 vector-by-matrix transform (rows 0–2, no translation) | `0x219660`, `0x21A310`, `0x21BA70` | — |
| `0x21D358` | GetSpeedFromF15Vector | f15 vector → float speed in display units (length * 216.0) | `0x21BA70` + HUD callers | — |
| `0x21D310` | VecSubtractAndLength | Subtracts two f15 vectors, stores the Euclidean length in the result's w | `0x217830`, `0x2179D8` | — |

Fixed-point world-position plumbing shared with the map/scene code:
`0x21C6A0` DecomposeWorldPosition, `0x21C728` ToRelativeVec4,
`0x21C7F0` FixedPosToFloatVec4, `0x21C880`/`0x21C8D0` grid-cell index,
`0x21C928` StoreIndexedPosition. Replay snapshots of the control state:
`0x21CBC8` SaveControlStateForReplay / `0x21CCB8` RestoreControlStateFromReplay
(15 KB table, driven from `0x20F3B8`).

## Data structures / globals

### Car state object (`gs_controls_car_object`; array at `0x177AC50` + k * 0x270)

| Offset | Type | Meaning |
|---|---|---|
| `+0x00` | f15 matrix | Car heading/orientation matrix used by the car-frame transforms `[inferred]` |
| `+0x40` | matrix | Body matrix, rebuilt each frame from heading + suspension tilt (`0x208008`) |
| `+0x74` | float | Body height offset from the springs (minus rest 0x1000, scaled) |
| `+0x80/+0x84/+0x88` | floats | Visual suspension flex / lean (`0x21ACD0`) |
| `+0x90/+0x94/+0x98` | floats | World position (X, Y, Z); `+0x9C` scale (1.0) |
| `+0xA0/+0xA4/+0xA8` | fixed-point | World position commit target: 27-bit wrap on X/Z, carry bit `0x2000000` on Y |
| `+0xB0` | f15 matrix | Physics matrix for the motion/velocity transforms `[inferred]` |
| `+0xF0` | vec4 | Velocity in car frame; `+0xF4` vertical (gravity −0x59/frame airborne; zeroed when any body point is grounded) |
| `+0x100` | vec4 | Previous-frame copy of `+0xF0` (frame motion delta = current − previous) |
| `+0x114 + i*0x10` | floats | Per-point suspension state, 7 points |
| `+0x184` | ptr | Mode block; word at `+8` bits: 0x4/0x8 speed band hysteresis (300 units), 0x40 water force, 0x100 wading flag, 0x400 travel-limit mode, 0x1000 wings, 0x2000 horn/boost sound `[inferred]` |
| `+0x198` | u16 | Status word: bit 0 = port; bits 0–1 kind (2 = the spawned/debug car); bit 0x200 gates odometer accumulation; mask 0xFB0 silences landing rumbles `[inferred]` |
| `+0x19C/+0x1A0/+0x1A4` | u32 | Body contact words (front, rear left, rear right): bits 8–10 material, bits 4–6 tyre sound, low bits grip class, −1 = pushed into a wall |
| `+0x1A8..+0x1B4` | u32 | The four wheels' contact words |
| `+0x1B8` | s32 | Signed forward speed scalar used by physics |
| `+0x1BC` | s32 | Arc distance |
| `+0x1C0/+0x1C4/+0x1C8` | s32 | Fall speeds of the three body points: 0 grounded, + gravity (~97/frame) airborne; all > 0 = airborne |
| `+0x1CC` | s16 | Curvature from speed and steering angle |
| `+0x1CE` | s16 | Steering angle input: ±1/frame held to ±0x20, self-center ±4/frame |
| `+0x1D0` | u16 | rpm/tachometer, 0..~10000 |
| `+0x1D2` | s16 | Steering command, clamped ±0x200, slewed ±8/frame |
| `+0x1D4` | s16 | Heading accumulator |
| `+0x1D6` | s16 | Integrated yaw offset, centered by `0x1EC000 / speed` clamp ±0x400 |
| `+0x1D8` | u32 | \|speed\| (display value) |
| `+0x1DC/+0x1E0/+0x1E4` | s32 | Body spring positions, rest 0x1000 |
| `+0x1E8/+0x1EC/+0x1F0` | s32 | Body spring velocities |
| `+0x1F8` | u32 | Flags; bit 0x20 set when the yaw delta opposes the yaw offset (countersteer/skid) `[inferred]` |
| `+0x1FE` | u8 | Brake ramp 0..32 (frames held) |
| `+0x1FF` | u8 | Gear 1..5 (in/out of the engine model) |
| `+0x200` | ptr | Brake force curve table: force = `curve[ramp - 1] * 10000 >> 5` |
| `+0x204` | s32 | Odometer accumulation of \|speed\| |
| `+0x210/+0x212` | s16/u8 | Per-frame countdowns |
| `+0x213` | u8 | Wading (in water) flag |
| `+0x218` | s32 | Weight/engine stat: drag divisor and engine-force multiplier |
| `+0x21C + (contact & 7) * 2` | s16[8] | Grip halfwords indexed by the contact word's grip class |
| `+0x22C` | u16 | Nitro amount (topped by `0x21A990`) |
| `+0x242` | s16 | Steering angle multiplier |
| `+0x244` | u8 | Wing position (0x20/0x40 control bits move it down/up when mode bit 0x1000) |
| `+0x245` | u8 | Horn sound id `[inferred]` |
| `+0x247` | u8 | Force-mode byte used with sys `+0x10` bit 1 for the 0x500/0x600 drag divisor `[inferred]` |

### Globals

- `0x3356D8` — engine/gearbox model function pointer (Ghidra name
  `gs_game_scene3d_handler`, misleading): called by `0x21A310` with
  (gear `+0x1FF`, car-frame speed, control word, nitro `+0x22C`, rpm `+0x1D0`),
  returns the new gear. Values `0x21CE48` / `0x21CF80`, installed by `0x21CE08`.
- `0x3356DC` (`DAT_003356dc`) — AI car-control function pointer, default
  `0x21D0B8`, installed by `0x21CE28`; consumed by `0x21A990`.
- `0x3356D0` / `0x3356D4` — debug draw gates set by `0x21BA70` when the sys debug
  byte `+0xB` is set and the car's `+0x198` equals 2 `[inferred: debug overlays]`.
- `0x1780A48` (`gs_controls_action_map`) — per-port driving action button masks
  (u16, stride 0x3448): `+0` gas, `+2` brake, `+4` reverse, `+6` jet, `+8`/`+0xA`
  wing up/down, `+0xC` horn, `+0xE` view, `+0x10` lights, `+0x12` navigator.
- `0x29C538` — the 11 button choices the game's setup offers.
- `0x33592C` — held pad masks (u16 + port*2); edges at `0x335930`.
- `0x29CA80` — the 7 contact-point offsets in car space.
- `0x29CBA8` — tyre-sound table selected by contact-word bits 4–6.
- `0x2EAE60` — quadrant-mask → wall-scrape rumble pattern (1/4/5 → 4 left,
  2/8/10 → 3 right, 3/6/7/9 → 5 front; rear-only 12 and 11/13/14 silent).
- `0x20ACB8` (`gs_controls_rumble_start`) — (sys, port, pattern, strength);
  gated by sys `+0xC + port` and the vibration switch `0x33590E`; scripts at
  `0x29AD60` → `0x2EA3D0`.
- `0x177AC50` — the car object array (stride 0x270).
- `0x332C08/0x332C0C/0x332C10/0x332C14/0x332C18` — float scales: spawn position,
  fixed→float position, body tilt, body tilt, travel-limit height offset.
- sys/scene object (param 2 of the tick): `+0x28` flags (0x4A000 freeze gates,
  0x400 spawn/reset), `+0xC` pad block (per-port status byte 7 gates the analog
  stick; masks at `+0x20`/`+0x30`; stick byte at `+0x32`), `+0xB` debug byte,
  `+0x10` mode byte, `+0x14` frame/timer counter.

## Control flow

Per frame (task pool `0x177A0D0`, walker `0x204F98` from the main loop
`0x2003B8`/`0x200534`), each car entity's callback `0x21BA70` runs:

1. Early-out on sys flags (`+0x28` & 0x4A000) or an inactive car (`+0x198` == 0).
2. On the spawn/reset frame (sys 0x400 bit, car kind 2): `0x2182B0` placement,
   `0x2180C0` parts, state reset (springs to 0x1000, gear 1, zeroed velocity,
   rpm, brake ramp, steering).
3. Form the frame's motion delta (`+0xF0` − `+0x100`, transformed by `+0xB0`);
   copy `+0xF0` into `+0x100`; gravity −0x59 on `+0xF4` while airborne.
4. `0x21A990` input → control word (player `0x21A820` or AI `DAT_003356dc`);
   horn/boost sound (`0x217F10`) with the nitro force capped at 0xCF69; water
   force ±0x59 by gas/reverse while wading.
5. Quadratic drag `v^2 / +0x218` on the forward force, yaw force and the three
   fall speeds; speed-band hysteresis at 300 display units (`0x21D358`).
6. Wings (`+0x244` by control 0x20/0x40 when mode bit 0x1000) and direct steering
   into the heading accumulator (`steer * force >> 9`).
7. `0x21A310` `gs_controls_car_update`: engine model via `0x3356D8` (gear/rpm),
   steering angle ramp and curvature, brake ramp + curve, traction
   (`0x59 * grip >> 8`), `0x218EE0` motion, `0x217C68` tyre sound, speed
   integration at `+0x1B8`, yaw delta, `0x21A088` steering damping.
8. Commit the velocity into the fixed-point world position `+0xA0..+0xA8`
   (scaled `<< 4 / 0x19`, 27-bit wrap); refresh the float position `+0x90..+0x98`.
9. `0x21B3D0` ground contact (7 points, terrain ray tests, `0x21AF28` suspension,
   fall speeds, landing rumble) → contact bitmask.
10. `0x219ED8` surface quads + `0x219660` collision impulses (codes 0–15).
11. Rebuild the body matrix `+0x40` from the heading and the spring differences;
    body height `+0x74`; odometer `+0x204`; `0x21ACD0` suspension visuals.
12. Debug (sys `+0xB`, car kind 2): physics vectors via `0x203A50`, HUD numbers
    via `0x26D700`.

Car-vs-car collisions run as their own task (`0x2179D8`, from `0x20F950`/`0x210430`)
over all car pairs each frame, separate from the per-car tick. Replays snapshot and
restore the 15 KB control-state table (`0x21CBC8`/`0x21CCB8` from `0x20F3B8`).

## Open questions

- The two engine/gearbox models `0x21CE48` and `0x21CF80` are not functions in the
  Ghidra inventory (no decompile available); the gear/rpm model's internals — and
  which scenes install which model (`0x21CE08`'s flag semantics) — are unknown.
- The default AI controller `0x21D0B8` is likewise not in the inventory; how AI
  steering derives from waypoint progress (`0x250068`) is known only from the notes.
- Which of the steer bits 0x2000/0x8000 is left vs right is not established.
- The constant cluster — 0x59 (89: gravity/force scale, spring coefficient), 0x28BE
  (10430), 0xAB85 (43909), 0x1B4C14B6, 0x1EC000, 0x600/0x500, 0xCF69 (nitro cap) —
  is inferred from the algebra; exact units are unknown.
- `+0x1FC` (a bool from `sys+0x14 − 0xE484 > 0x1944C`, inverted by control bit
  0x10) is observed but unexplained `[inferred: a timed demo/state window]`.
- The mapping of grounded contact points to the fall-speed words in `0x21AF28`'s
  contact loop (points 0–4 → `+0x1C0`, points 5/6 → `+0x1C4`/`+0x1C8`) is taken
  from the decompile and may not be the intended semantics `[inferred]`.
- `+0x198` bit meanings beyond the port bit (bit 1/kind, 0x100, 0x200, the 0xFB0
  rumble gate mask) are partially inferred.
- `+0x1B8` vs the car-frame speed passed by the tick: one is the integrated speed
  and the other the target/observed speed; which role each plays exactly is
  `[inferred]`.
- What `0x26D700` renders in this call path (TSV: "DrawRaceHudNumbers", packs the
  timer and counters into numeric HUD slots) — a debug telemetry readout
  `[inferred]`.
- The wing term passed into ground contact (`wings * force >> 11`, or `>> 8`
  wading) is plausibly downforce `[inferred]`.
- `0x21CA90` `UpdateObjectPhysicsState` (another integrator calling
  `0x2180C0`/`0x2182B0`) and `0x24E1A0` `UpdateCarControlState` both touch the car
  object; their relationship to the `0x21BA70` tick (other car kinds? menus?) is
  unknown.
