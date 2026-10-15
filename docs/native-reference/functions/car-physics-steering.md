# Vehicle Physics & Steering

The per-frame vehicle tick: input acquisition, steering damping, suspension flex, wheel ground contact, motion integration, and state reset — plus replay control-state save/restore and car spawning/spec loading.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x002180C0` | `LoadCarPerformanceSpecs` | Loads equipped parts performance parameters (gears, engine, tires, brakes) into car physics state fields. | `0x00218458`, `0x0021BA70`, `0x0021CA90` | — |
| `0x002182B0` | `SpawnCarAtDoor` | Calculates initial spawn position, ground elevation, and heading angle for a car at a building entrance door. | `0x00218458`, `0x0021BA70`, `0x0021CA90` | `0x00274AF8` |
| `0x00218458` | `SpawnCarEntityTask` | Initializes car entity state, physics matrix, equipped parts, spawn position, and assigns car update callback FUN_0021ba70. | `0x0020E708`, `0x0020ED30`, `0x0020F950`, `0x00210430` | `0x002180C0`, `0x002182B0`, `0x0021BA70`, `0x0024FCD8`, `0x002574A0`, `0x00258E50`, `0x0027E840`, `0x00272A48` |
| `0x00218EE0` | `AdvanceVehicleMotion` | Advances car odometer (+0x1d0 cap 10000) and arc dist (+0x1bc) by speed; writes world delta to out vec; sets state flag bit1 [inferred] | `0x0021A310` | `0x00203A50`, `0x00274B10`, `0x0027EEE0` |
| `0x002194B8` | `SteerHeadingVector` | Rotates dir vec about Y by angle, shrinks X/Z by 1/1.0625 with +/-0.125 pivot by flags 0x40/0x80, lifts Y, normalizes [inferred] | `0x0021B3D0` | `0x002727E8`, `0x00272C38`, `0x00272728`, `0x00272A48` |
| `0x0021A088` | `UpdateSteeringDamping` | Rate-limits steering cmd (+0x1d2, clamp +/-512), integrates yaw offset (+0x1d6), applies speed-scaled centering clamp [inferred] | `0x0021A310` | — |
| `0x0021A990` | `AcquireVehicleInput` | Routes player gs_controls_player_control or AI fnptr DAT_003356dc, keeps input ring buffer, tops nitro +0x23c [inferred] | `0x0021BA70` | `0x00250068`, `0x00258E50`, `0x0021A820` |
| `0x0021ACD0` | `UpdateSuspensionFlex` | Advances suspension floats (+0x80,+0x88) from speed/arc via per-mode constants; sets +0x84 lean from steering (+0x1ce) [inferred] | `0x0021BA70` | — |
| `0x0021AF28` | `UpdateWheelSuspension` | Per wheel compares contact height vs travel (+0x114), updates spring/damping, starts rumble; returns contact bits [inferred] | `0x0021B3D0` | `0x0020ACB8` |
| `0x0021B3D0` | `ResolveGroundContact` | Transforms 7 wheel offsets to world, ray-tests terrain per wheel, runs suspension 0x21af28, sets airborne/landing [inferred] | `0x0021BA70` | `0x00208008`, `0x00208080`, `0x002194B8`, `0x0021AF28`, `0x00258E50`, `0x002727A0`, `0x002727E8`, `0x00272868`, `0x00272940`, `0x00272728` |
| `0x0021BA70` | `UpdateVehiclePhysics` | Master vehicle tick: input, ground-contact, surface, collision substeps, velocity drag/clamp, commits world position [inferred] | `0x00218458` | `0x00203A50`, `0x00208008`, `0x00217F10`, `0x002180C0`, `0x002182B0`, `0x00219660`, `0x00219ED8`, `0x0021A990`, `0x0021ACD0`, `0x0021B3D0`, `0x0021D2D8`, `0x0021D358`, `0x00258E50`, `0x0026D700`, `0x0027EEE0`, `0x0021A310`, `0x0027E840` |
| `0x0021C998` | `ResetVehicleState` | Zeroes car runtime state (velocity +0xf0, suspension/steering accumulators, odometer) and resets damping scalars to 0x1000 | `0x00217830`, `0x00240CD8`, `0x0024FB80`, `0x002631A0` | `0x00258E50` |
| `0x0021CA90` | `UpdateObjectPhysicsState` | Integrates position/velocity from float scale, calls FUN_002180c0, resets many state fields, optionally FUN_00258e50 [inferred]. | — | `0x002180C0`, `0x002182B0`, `0x00258E50` |
| `0x0021CBC8` | `SaveControlStateForReplay` | Copies the 15KB control-state table into replay scratch buffer and rewinds three replay stream pointers for recording [inferred] | `0x0020F3B8` | — |
| `0x0021CCB8` | `RestoreControlStateFromReplay` | Prints replay buffer sizes, restores control-state table from replay scratch memory and rewinds stream pointers [inferred] | `0x0020F3B8` | `0x0027EA30` |
| `0x0021CE08` | `SelectScene3dHandler` | Installs one of two scene-3D handler entry points (0x21ce48 or 0x21cf80) into gs_game_scene3d_handler based on a flag | `0x0020F950`, `0x00210430` | — |
| `0x0021CE28` | `SetAICarController` | Installs AI car-control fn ptr DAT_003356dc (used by vehicle input dispatch 0x21a990); defaults to 0x21d0b8 when passed null | `0x0020E708`, `0x0020F950` | — |
| `0x0024E1A0` | `UpdateCarControlState` | Large per-car update using gs_controls_car_object; calls physics/collision helpers and memsets temporaries [inferred]. | `0x002490A0` | `0x002494E8`, `0x00249520`, `0x00249568`, `0x002495B8`, `0x002495C0`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0027E840` |

