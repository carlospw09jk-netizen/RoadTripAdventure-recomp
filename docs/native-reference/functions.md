# Native Function Reference

> Generated from the existing decompiler inventory (`functions.tsv`) with maintained names from `config/game_symbols.toml`. Descriptions report existing RE observations; no new decompilation was performed.

## Table of Contents

- [Boot, PS2 Runtime & Core Initialization](#boot-ps2-runtime--core-initialization) (80)
- [Task Scheduler & State-Machine Runtime](#task-scheduler--state-machine-runtime) (91)
- [Text, Dialogues & General UI Primitives](#text-dialogues--general-ui-primitives) (117)
- [Car Physics, Controls & Steering](#car-physics-controls--steering) (79)
- [Collision, Terrain & Spatial Tests](#collision-terrain--spatial-tests) (6)
- [Chase Cameras & View Transitions](#chase-cameras--view-transitions) (22)
- [Sound RPC (SNDMOD) & Music](#sound-rpc-sndmod--music) (56)
- [Loading, Filesystem & Texture Upload](#loading-filesystem--texture-upload) (32)
- [Rendering, VIF/VU & GS Packet Helpers](#rendering-vifvu--gs-packet-helpers) (79)
- [World Rendering, Props & Effects](#world-rendering-props--effects) (6)
- [Maps & Minimap Rendering](#maps--minimap-rendering) (17)
- [Day/Night, Lighting & Environment](#daynight-lighting--environment) (19)
- [Progression, Collection Flags & World Queries](#progression-collection-flags--world-queries) (69)
- [Hub, Town, House & Event Scripts](#hub-town-house--event-scripts) (2)
- [Shop, Garage, Tuning & Paint Editor](#shop-garage-tuning--paint-editor) (23)
- [Carousel, Quick Race & Two-Player Results](#carousel-quick-race--two-player-results) (22)
- [Race Results, Rankings & Licenses](#race-results-rankings--licenses) (24)
- [Mini-games: Golf, Stunts, Sports & Slots](#mini-games-golf-stunts-sports--slots) (69)
- [Debug Text & Diagnostics](#debug-text--diagnostics) (18)
- [Math, Tables, Strings & Utility Helpers](#math-tables-strings--utility-helpers) (56)
- [Unclassified Engine & Gameplay Helpers](#unclassified-engine--gameplay-helpers) (36)

## Reading Conventions

- **Description** preserves the inventory’s evidence-based summary. `[inferred]` remains wherever it was recorded.
- **Callers / callees** are direct addresses from the inventory; `—` means none was recorded.
- **Key globals / structures** lists only explicitly referenced globals, tables, buffers, or offset-bearing bases from the same note. `None identified` means the one-line inventory note did not name one.

# Boot, PS2 Runtime & Core Initialization

Startup syscalls, IOP/SIF setup, memory pools, controller foundations, and top-level initialization.

## 0x002000B8 Ps2ExitThreadId

**Description:** Tiny stub: invokes PS2 syscall 4 (ExitThreadId) and returns; called from 0x0027DB20.

**Callers:** `0x0027db20`  
**Callees:** —
**Key globals / structures:** `0x0027DB20`

## 0x002000C0 Ps2InitGsDoubleBuffer

**Description:** Executes syscall 0x23 then resets GS and sets double buffers: 640x448 (arg 0) or 512x256, storing DMODE2/6 words. [inferred]

**Callers:** `0x00200008`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002000C8 gsInitDisplay

**Description:** Maintained symbol-map entry: sceGsResetGraph(0,1,2,1) + two sceGsSetDefDBuffDc 640x224 CT32, Z24 (DAT_00333288 = 0)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `DAT_00333288`

## 0x002001B8 Ps2InitSystem

**Description:** Prints E-game banner, reboots IOP via image 0x2E9A58, resets SIF/CD/DMA, loads 7 file modules 0x2E9A70-0x2E9B00, inits MC+PAD, calls 0x00226398.

**Callers:** `0x002003b8`  
**Callees:** `0x00226398`, `0x0028c950`, `0x0027ea30`, `0x002789c0`, `0x00278e98`, `0x002701a8`, `0x00288e88`, `0x002797a8`, `0x00273598`, `0x002737f8`, `0x0028bc50`, `0x00287668`, `0x0028c0e0`, `0x0028ce50`, `0x0028ce18`
**Key globals / structures:** `0x2E9A58`, `0x2E9A70`, `0x2E9B00`, `0x00226398`

## 0x00200A60 uiDrawPadGlyph

**Description:** Maintained symbol-map entry: PS2 pad icons by pad mask: 1/2/4/8 = L2 R2 L1 R1 (24x12), 0x10/0x20/0x40/0x80 = triangle circle cross square (20x20)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204A00 UiDestroyRemovedNodes

**Description:** Walks linked list from param_1+0x10: for nodes with flag bit1 calls destructor fn-ptr (+0x2C), frees child object fields 0x10/0x14 via FUN_00205018 and unlinks node. [inferred]

**Callers:** `0x002105b8`  
**Callees:** `0x00205018<arg_value>`
**Key globals / structures:** None identified in the inventory note.

## 0x00204C10 ClearTaskPool

**Description:** Clears active and linked-task fields for 48 fixed task slots.

**Callers:** `0x002003b8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204D20 InitializeTask

**Description:** Initializes a task record with supplied fields and clears remaining state.

**Callers:** `0x0021e328`, `0x0021e4d8`, `0x0021e690`, `0x0021ed90`, `0x0021ee98`, `0x0021ef98`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204F58 ClearTaskPoolFlag

**Description:** Clears bit 1 on each of 48 pooled task entries.

**Callers:** `0x0020d120`, `0x002105b8`, `0x0026ca48`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002052D0 StartMenuFlow

**Description:** Spawns initial menu tasks, configures the task callback, and resets menu state.

**Callers:** `0x002003b8`  
**Callees:** `0x00204ed0`, `0x00258578`, `0x00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00205C20 uiMenuCreate

**Description:** Maintained symbol-map entry: guess: allocates a uiMenuWindow

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020A8A8 ReadPadState

**Description:** Reads controller state and updates the corresponding input data.

**Callers:** `0x0020ab10`  
**Callees:** `0x00273bc8`, `0x00273d68`, `0x00273fa8`, `0x00273b48`, `0x00274250`, `0x00274198`, `0x002740e0`
**Key globals / structures:** None identified in the inventory note.

## 0x0020E140 InitFlagsIfNotLocked

**Description:** If flag 0x100000 clear: sets 0x400000 when count<0x40, calls FUN_002080f8 and FUN_00204310, sets 0x80000000 [inferred].

**Callers:** —  
**Callees:** `00204310`, `002080f8`
**Key globals / structures:** `0x100000`, `0x400000`, `0x80000000`

## 0x0020F330 ResetRaceRanks

**Description:** Resets race participant slot status and adjusts car finish positions against eliminated ranks into DAT_01787016 [inferred].

**Callers:** `0x0020F3B8`  
**Callees:** —
**Key globals / structures:** `DAT_01787016`

## 0x00214220 ResetControllerConfigDefaults

**Description:** Resets controller button bindings to defaults, checks for mapping collisions, and updates config fields [inferred].

**Callers:** `0x00214470`  
**Callees:** `0x00213C40`, `0x00214078`, `0x00229F20`
**Key globals / structures:** None identified in the inventory note.

## 0x002174E0 UpdateOptionsMenu

**Description:** Per-frame options menu: init state on first call, resets buffer on page change, runs settings page, kills task on exit.

**Callers:** —  
**Callees:** `00216ff8`, `00216738`, `00204de8`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x0021C998 ResetVehicleState

**Description:** Zeroes car runtime state (velocity +0xf0, suspension/steering accumulators, odometer) and resets damping scalars to 0x1000

**Callers:** `0x00217830`, `0x00240CD8`, `0x0024FB80`, `0x002631A0`  
**Callees:** `0x00258E50`
**Key globals / structures:** None identified in the inventory note.

## 0x0021CA90 UpdateObjectPhysicsState

**Description:** Integrates position/velocity from float scale, calls FUN_002180c0, resets many state fields, optionally FUN_00258e50 [inferred].

**Callers:** —  
**Callees:** `002180c0`, `002182b0`, `00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x0021CCB8 RestoreControlStateFromReplay

**Description:** Prints replay buffer sizes, restores control-state table from replay scratch memory and rewinds stream pointers [inferred]

**Callers:** `0x0020F3B8`  
**Callees:** `0x0027EA30`
**Key globals / structures:** None identified in the inventory note.

## 0x0021D3E0 UNKNOWN_0021D3E0

**Description:** VU0 stub: loads matrix rows then vlqi/qmtc2/ctc2 sequence with no memory output; unclear [inferred]

**Callers:** `0x0022AEB0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021E328 CycleCameraPresetState

**Description:** Cycles camera state +0x25 mod 3, picks matching preset from 24-entry table and relaunches task 0x238b18 with its index [inferred]

**Callers:** `0x0021E4D8`, `0x0021EE98`, `0x0021EF98`  
**Callees:** `0x00204D20`
**Key globals / structures:** `0x238b18`

## 0x0021E4D8 InitCameraTaskState

**Description:** Camera boot handler: spawns cam task 0x238b18 or loads preset type 0x10/0x20 from table, then chains next handler fn (+0xc) [inferred]

**Callers:** `0x0021E690`  
**Callees:** `0x00204D20`, `0x00204C48`
**Key globals / structures:** `0x238b18`

## 0x0021E690 RunCarCameraTask

**Description:** Per-frame car camera task: selects target and preset, eases FOV, computes chase basis, builds camera, runs terrain collision [inferred]

**Callers:** `0x0020EBE8`, `0x0021EC58`, `0x0021ED90`  
**Callees:** `0x00204D20`, `0x0021C7F0`, `0x0021C880`, `0x0021DC18`, `0x0021E070`, `0x0021E4D8`, `0x0021EC58`, `0x0021F198`, `0x0021F910`, `0x0021FAE8`, `0x0021FB20`, `0x00274938`, `0x00274A10`, `0x0021F698`, `0x002729D0`
**Key globals / structures:** None identified in the inventory note.

## 0x0021EE98 SwitchCameraPresetType10

**Description:** After cam timer +0x8 hits 0xc0, loads preset type 0x10 from table for this player and chains handler to 0x21ef98 [inferred]

**Callers:** `0x0021E4D8`, `0x0021EF98`  
**Callees:** `0x00204D20`
**Key globals / structures:** `0x21ef98`

## 0x0021EF98 SwitchCameraPresetType20

**Description:** After cam timer +0x8 hits 0xc0, loads preset type 0x20 from table for this player and chains handler to 0x21ee98 [inferred]

**Callers:** `0x0021E4D8`, `0x0021EE98`  
**Callees:** `0x00204D20`
**Key globals / structures:** `0x21ee98`

## 0x0021F198 ApplyFreeCameraInput

**Description:** Moves free-cam by tuning deltas per button bits (0x1/4/0x1000-0x8000), scales zoom, adjusts pitch/yaw when mode is 7 [inferred]

**Callers:** `0x0021E690`  
**Callees:** `0x0026D880`
**Key globals / structures:** None identified in the inventory note.

## 0x0021FAE8 LoadCameraPreset

**Description:** Copies the 32-byte camera preset record at index param_2 (stride 32) from table 0x29ce00 into the camera state slot [inferred]

**Callers:** `0x0021E690`  
**Callees:** —
**Key globals / structures:** `0x29ce00`

## 0x00223200 ResetShowroomLighting

**Description:** Selects 5-cycle DMA, uploads 16-light At16 env, and resets DAT_003356e8 (showroom render init step). [inferred]

**Callers:** `0x0022aeb0`, `0x0022bfd0`, `0x0022c280`, `0x0022cc38`, `0x00216738`  
**Callees:** `0x00226f70`, `0x00226940`
**Key globals / structures:** `DAT_003356e8`

## 0x00224CD0 SpawnCarEmitterEntry

**Description:** Allocates the next free slot in DAT_0172ff70 (DAT_0172ff7c==0 enables), seeds it with arg3/arg4 (type 8, subtype 2), marks cls flag. [inferred]

**Callers:** `0x00225120`  
**Callees:** `0x0021d358`, `0x00273268`, `0x002729d0`, `0x00272728`, `0x00272758`
**Key globals / structures:** `DAT_0172ff70`, `DAT_0172ff7c`

## 0x00224EC0 AdvanceCarEmitterStep

**Description:** Frame-step a slot's lifecycle: writes ty/tz via vu0ApplyMatrix, may advance type-7 puff param, registers new slot if free. [inferred]

**Callers:** `0x00225120`  
**Callees:** `0x00223b30`, `0x002729d0`, `0x00272728`, `0x00272758`
**Key globals / structures:** None identified in the inventory note.

## 0x00225588 ResetCarEffectSlots

**Description:** Zeroes effect slot tables (DAT_0172ff70, DAT_01730770, DAT_017307f0) — render-init clear. [inferred]

**Callers:** `0x00221300`  
**Callees:** `0x0027e840`
**Key globals / structures:** `DAT_0172ff70`, `DAT_01730770`, `DAT_017307f0`

## 0x00227C68 InitControlsBindings

**Description:** Bootstraps control pads remap/wpad tables via FUN_0023c418, writes pre-canned bit values into the bindings array, and pre-registers 0x80 phases via FUN_0023df38. [inferred]

**Callers:** `0x0026b848`  
**Callees:** `0x0023c418`, `0x0023d2c0`, `0x0023d7a8`, `0x0023df38`
**Key globals / structures:** None identified in the inventory note.

## 0x00227F90 DrawSaveSlotEntry

**Description:** Draws text rows for one save listing (player name, track, time, free-blocks) using FUN_00204310/FUN_00202150 with row offset param_2. [inferred]

**Callers:** `0x002280d8`, `0x0022a050`  
**Callees:** `0x00201e48`, `0x00202150`, `0x00204310`, `0x00204568`
**Key globals / structures:** None identified in the inventory note.

## 0x002282C0 MemcardGetInfoTask

**Description:** sceMcGetInfo async wrapper: on phase 0 kicks sceMcGetInfo; waits via sceMcSync, classifies type/files into a result byte; timeouts abort. [inferred]

**Callers:** `0x00228b10`, `0x00228f40`, `0x002291c8`, `0x0022a320`  
**Callees:** `0x0027a1a8`, `0x0027a2e0`, `0x0027ea30`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00229AF8 MemcardSpaceCheckDialog

**Description:** Inserts localized prompts ("Memory card 8MB... is not inserted", "Do you want to start?") via FUN_002058a8 and chains to FUN_0022a320 based on result code/state. [inferred]

**Callers:** `0x0020a520`  
**Callees:** `0x00204c48`, `0x002058a8`, `0x00205c08`, `0x00205c20`
**Key globals / structures:** None identified in the inventory note.

## 0x00229FD8 InitProgressRamDefaults

**Description:** Initializes progress/save state: resets two subsystems, sets counter 1000, copies 150 slot pointers, sets gs_progress_ram_address.

**Callers:** `0x00209848`, `0x00211170`, `0x0026b848`, `0x0026d438`  
**Callees:** `0x002277b0`
**Key globals / structures:** `gs_progress_ram_address`

## 0x0022A168 MemoryCardOpenWriteTask

**Description:** Memory-card step: opens the card file via sceMcOpen; on error reports the code and kills the task, else installs the next handler.

**Callers:** `0x002291c8`  
**Callees:** `0x00204de8`, `0x0027ea30`, `0x00279b08`
**Key globals / structures:** None identified in the inventory note.

## 0x0022A218 FormatMemoryCardTask

**Description:** Memory-card step: draws a dialog, runs sceMcFormat then sceMcSync, reports the result to the parent and kills the task.

**Callers:** `0x002291c8`  
**Callees:** `0x00204310`, `0x00204568`, `0x00204de8`, `0x0027ea30`, `0x0027a798`, `0x0027a1a8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022ABA0 AdvanceAnimationPhaseState

**Description:** Transitions on mode word +2 and phase +0x27, resets phases, rebuilds matrices and plays SE 0x34/0x35. [inferred]

**Callers:** `0x0022aeb0`  
**Callees:** `0x00258e50`, `0x00272c38`, `0x00273268`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0022AD70 ClassifyObjectContext

**Description:** Classifies an object from bytes at +0x21/+0x23 and a field at +0x20, returning codes 0-5. [inferred]

**Callers:** `0x0022aeb0`, `0x00246250`, `0x00269e88`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022C518 StartCourseEventTask

**Description:** Probes FUN_0023cd58 for an unlock flag, installs handlers 0x22aeb0/0x22c630, resets the tick and spawns task 0x208270. [inferred]

**Callers:** `0x002105b8`, `0x00211708`, `0x00211808`, `0x0026be50`, `0x0026d108`  
**Callees:** `0x002080f8`, `0x0022aeb0`, `0x0023cd58`, `0x00204c48`
**Key globals / structures:** `0x22aeb0`, `0x22c630`, `0x208270`

## 0x0022D1E0 OpenCourseSelectEntry

**Description:** Awards stamp 0x28 when eligible, resets progress entries via FUN_0023d2c0/0023df38, then chains into task 0x22cd38.

**Callers:** `0x002105b8`, `0x00211708`, `0x00211808`  
**Callees:** `0x0022c648`, `0x0022cd38`, `0x0023d2c0`, `0x0023d628`, `0x0023df38`, `0x0023c8a8`
**Key globals / structures:** `0x22cd38`

## 0x0022D288 ResetRecordTables

**Description:** Clears record tables DAT_01787068 and DAT_01786ff0 to INT_MAX and seeds best record DAT_00335700 from progress by slot/type.

**Callers:** `0x0022ddd0`  
**Callees:** —
**Key globals / structures:** `DAT_01787068`, `DAT_01786ff0`, `DAT_00335700`

## 0x0022D620 UpdateZoneFromRects

**Description:** Classifies an object's X/Y against three rectangles from param_4, storing zone id 1-4 at +0x19a. [inferred]

**Callers:** `0x0022ddd0`, `0x0022eb78`  
**Callees:** `0x0022d390`
**Key globals / structures:** None identified in the inventory note.

## 0x002329A0 SetVehicleMode

**Description:** Resets a state block and assigns mode-specific sprite widths and colors [inferred]

**Callers:** `0x0022dae8`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00233158 SetObjectState

**Description:** Resets a per-object state table and fills it with a sprite/timer config [inferred]

**Callers:** `0x0025a120`, `0x0025ff00`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234128 SetBumperConfig

**Description:** Resets a bumper state struct and sets width, alpha, and sprite data [inferred]

**Callers:** `0x0022ddd0`, `0x0025a120`, `0x0025dbb8`, `0x0025eb70`, `0x00263e68`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002341F8 SetTailConfig

**Description:** Resets a tail state struct and fills lifetime, alpha, and sprite values [inferred]

**Callers:** `0x0022ddd0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234378 SetDashConfig

**Description:** Resets a dash state block and writes animation, alpha, and sprite data [inferred]

**Callers:** `0x0022ddd0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234450 SetEngineConfig

**Description:** Resets an engine state block and writes animation, alpha, and sprite data [inferred]

**Callers:** `0x0022ddd0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234528 ResetStateBlock

**Description:** Zeroes a large state struct and initializes a base object config [inferred]

**Callers:** `0x0022ecb8`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234618 SetBannerConfig

**Description:** Resets a banner/object state and sets a width, sprite pointer, and frame count [inferred]

**Callers:** `0x002631a0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234708 SetRoadData

**Description:** Resets a road state block and fills the pointer, size, and animation data [inferred]

**Callers:** `0x00254d20`, `0x0025e278`, `0x0025e630`, `0x0025f6a0`, `0x00262678`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002347F0 SetRoadData2

**Description:** Resets a variant road state block with a different asset pointer and mode [inferred]

**Callers:** `0x002631a0`, `0x00263e68`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002348D8 SetRoadData3

**Description:** Resets a road state block and sets a larger mode-specific animation config [inferred]

**Callers:** `0x0025fbf8`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002349C8 SetRoadData4

**Description:** Resets a road state block and sets the animation mode/value for another variant [inferred]

**Callers:** `0x00254d20`, `0x002595f0`, `0x0025e278`, `0x0025e630`, `0x00262678`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234AB0 SetRoadData5

**Description:** Resets a road state block and stores a sprite pointer and mode-specific values [inferred]

**Callers:** `0x00260638`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234B98 SetRoadData6

**Description:** Resets a road state block and sets the asset pointer and frame mode [inferred]

**Callers:** `0x00262678`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234C80 SetFlagConfig

**Description:** Resets a flag state entry and writes a sprite pointer and duration value [inferred]

**Callers:** `0x0025a120`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234D10 ResetMode12

**Description:** Clears a fresh mode-12 object block and seeds its base state fields [inferred]

**Callers:** `0x0025f6a0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234E08 ResetMode15

**Description:** Clears a mode-15 object block and seeds its base state fields [inferred]

**Callers:** `0x0025fbf8`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234E50 ResetMode16

**Description:** Clears a mode-16 object block and seeds its base state fields [inferred]

**Callers:** `0x00260638`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234F68 SetTrackMeta4

**Description:** Resets a track animation block and writes sprite pointer and duration data [inferred]

**Callers:** `0x00251b60`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235008 ResetMode18

**Description:** Clears a mode-18 object block and seeds its type/state values [inferred]

**Callers:** `0x00260dc0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235050 ResetMode19

**Description:** Clears a mode-19 object block and seeds its type/state values [inferred]

**Callers:** `0x0025a120`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235098 ResetMode1A

**Description:** Clears a mode-1A object block and seeds its type/state values [inferred]

**Callers:** `0x00254430`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002350F0 SetHudState

**Description:** Resets a HUD state struct and chooses between compact and tall layouts [inferred]

**Callers:** `0x00254430`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002351A0 SetHudState2

**Description:** Resets a second HUD state block with a different sprite pointer and size [inferred]

**Callers:** `0x00254430`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235238 SetHudState3

**Description:** Resets a HUD state block and writes sprite/duration/alpha fields [inferred]

**Callers:** `0x00254430`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002352D8 ResetMode1B

**Description:** Clears a mode-1B object block and seeds its base ID/state values [inferred]

**Callers:** `0x00254d20`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002353D0 ResetMode1E

**Description:** Clears a mode-1E object block and seeds its base state fields [inferred]

**Callers:** `0x00262678`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235B58 DrawRaceResultsScreen

**Description:** Builds the race-results screen: allocates state, lays out labels, draws standings, then ends the task on button input. [inferred]

**Callers:** `0x0020f3b8`  
**Callees:** `0x00201e48`, `0x00203fc8`, `0x00204310`, `0x00204568`, `0x002080f8`, `0x0020df50`, `0x00235880`, `0x002359e8`, `0x00237c48`, `0x0023d8c8`, `0x00204de8`, `0x00226f70`
**Key globals / structures:** None identified in the inventory note.

## 0x00237508 EnterLicenseScreen

**Description:** Exits on cancel, else allocates state, computes player points, sets +0x27 stage, installs FUN_002364b0 and runs it. [inferred]

**Callers:** `0x0020f3b8`  
**Callees:** `0x002080f8`, `0x0020df50`, `0x00235880`, `0x00236238`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00237878 DrawLocationList

**Description:** Draws the scrolling location list from gs_locations_table and adjusts the +2 scroll offset from d-pad input; closes on confirm. [inferred]

**Callers:** `0x0026bc08`  
**Callees:** `0x00203fc8`, `0x0022ea08`, `0x0023d628`, `0x00204de8`
**Key globals / structures:** `gs_locations_table`

## 0x0023D2C0 SetFlaggedSlotBit

**Description:** Sets bit param_2 in 5-word per-slot flag set for group param_3; fills first free word, bit set in words 0-4 in order

**Callers:** `00227c68`, `00229f70`, `0022d1e0`, `00239378`, `00239f68`, `002406f8`, `0025be98`, `0025c1d8`, `0025c300`, `0025c528`, `0025c818`, `0025ca08`, `0025d3d8`, `0025d4e8`, `00268e40`, `00269568`, `00269900`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023DD30 RecordTaskResult

**Description:** When param nonzero records DAT_00335730 into table DAT_017870c4 and, for index 6, updates min DAT_01780a10; resets DAT_00335730 [inferred]

**Callers:** `0023ad00`  
**Callees:** —
**Key globals / structures:** `DAT_00335730`, `DAT_017870c4`, `DAT_01780a10`

## 0x0023EE70 SubmitConditionalTextureLayout

**Description:** Builds three texture packets and conditionally resets a transform and copies a vector. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x00207B50`, `0x002729D0`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x0024FB80 InitCameraTransforms

**Description:** Resets camera/transform state, clearing angle field and initializing two 4x4 matrices to identity [inferred]

**Callers:** `00250f00`, `00254430`, `00254d20`, `0025a120`, `0025eb70`, `00260dc0`, `00262678`  
**Callees:** `0021c998`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x00254D20 CarShopVisitSceneTask

**Description:** Task allocating 0x8cd0 state that plays bgm 0xa100201, spawns a subtask, and waits for the car (DAT_0177ace4) to reach a shop position or a saved-car match before advancing flags and killing tasks. [inferred]

**Callers:** —  
**Callees:** `0x002080f8`, `0x0020df50`, `0x0021c928`, `0x00233860`, `0x00233ef0`, `0x00234708`, `0x002349c8`, `0x002352d8`, `0x0023d8c8`, `0x0024fb80`, `0x00258fb0`, `0x00258ec8`, `0x00204de8`, `0x00204c48`, `0x0027e5f8`, `0x0027e840`, `0x002729d0`
**Key globals / structures:** `0xa100201`, `DAT_0177ace4`

## 0x002595F0 StreetSceneTask

**Description:** Task allocating 0x8cc0 state that plays bgm 0xa0d0201, spawns task 0x2357c0, places cars at fixed coordinates (730,98) and (730,102), and drives a dialog/arrive state machine. [inferred]

**Callers:** —  
**Callees:** `0x002080f8`, `0x0020df50`, `0x0021c928`, `0x00232d68`, `0x00232f08`, `0x00233860`, `0x002349c8`, `0x00234d58`, `0x0023d8c8`, `0x00259498`, `0x00258ec8`, `0x00204de8`, `0x00204c48`, `0x0027e840`
**Key globals / structures:** `0xa0d0201`, `0x2357c0`

# Task Scheduler & State-Machine Runtime

Fixed task records, handler transitions, lifecycle cleanup, and reusable state-machine plumbing.

## 0x002003B8 gameMainLoop

**Description:** Maintained symbol-map entry: endless frame loop: vifBeginPacket(0), tasks, uiBatchFlush, frameBlend, kick, swap buffers

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002028B0 uiDrawDialogueFrameA

**Description:** Maintained symbol-map entry: big message window frame, upper half; only the dialogue task 0x23B168 calls it

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x23B168`

## 0x00204D60 DestroyTaskRecord

**Description:** Runs task cleanup callback, recursively releases linked tasks, and clears the task reference.

**Callers:** `0x00209cb0`, `0x0020a2b0`, `0x0020e708`, `0x0020f3b8`, `0x002105b8`, `0x00211170`, `0x00211320`, `0x0025a120`, `0x00268e40`, `0x00269568`, `0x00269900`, `0x0026be50`, `0x0026ccd0`  
**Callees:** `0x00205018`
**Key globals / structures:** None identified in the inventory note.

## 0x00204ED0 SetTaskFlag

**Description:** Sets bit 1 in a task's status byte.

**Callers:** `0x002052d0`, `0x0020a2b0`, `0x0020e708`, `0x0020ebe8`, `0x0020ed30`, `0x0020f950`, `0x00210430`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204EE0 SetTaskListFlag

**Description:** Sets bit 1 on each task in a linked list.

**Callers:** `0x002105b8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204F18 ClearTaskListFlag

**Description:** Clears bit 1 on each task in a linked list.

**Callers:** `0x002105b8`, `0x00246250`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00205018 DestroyTaskTree

**Description:** Invokes cleanup callback, recursively destroys child/sibling tasks, and clears task state.

**Callers:** `0x00204a00`, `0x00204d60`, `0x00205018`, `0x00204de8`  
**Callees:** `0x00205018`
**Key globals / structures:** None identified in the inventory note.

## 0x00205088 UpdateMenuFlowTask

**Description:** Updates input/menu state and spawns the task for the selected menu mode.

**Callers:** `0x002052d0`  
**Callees:** `0x0020a660`, `0x0020ab10`, `0x0020e018`, `0x00258578`, `0x00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x002083C0 AdvanceTimedTask

**Description:** Advances a timed task callback and kills the task when its timer reaches its terminal value.

**Callers:** `0x0022bb88`, `0x0023be88`  
**Callees:** `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00208428 PlayTaskSound

**Description:** Plays a task-associated sound using its stored sound identifier.

**Callers:** `0x002083c0`  
**Callees:** `0x00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x002086B0 InitializeMenuPanelTask

**Description:** Initializes a menu panel task and sets up its transition state.

**Callers:** `0x0020a2b0`  
**Callees:** `0x002063c8`, `0x0021d718`, `0x0021dae8`, `0x00204de8`, `0x00204c48`, `0x00226f70`
**Key globals / structures:** None identified in the inventory note.

## 0x00208848 RunMenuPanelTask

**Description:** Updates menu panel state, spawning sub-tasks and applying transitions.

**Callers:** `0x0020a2b0`  
**Callees:** `0x0021f910`, `0x00225790`, `0x00258e50`, `0x0021f698`, `0x00204de8`, `0x00204c48`, `0x002261a0`, `0x00227048`, `0x00226f70`, `0x00226b18`, `0x002267a8`, `0x002266e8`, `0x00226a18`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x00209848 RunMenuSelectionTask

**Description:** Processes menu selection and updates task/input state.

**Callers:** `0x00205088`  
**Callees:** `0x00203240`, `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x002063c8`, `0x00206b00`, `0x00207d80`, `0x002080f8`, `0x0021d718`, `0x0021d788`, `0x0021dae8`, `0x00227b28`, `0x00229fd8`, `0x00258e50`, `0x00204de8`, `0x00204c48`, `0x00207b50`
**Key globals / structures:** None identified in the inventory note.

## 0x00209CB0 RunMenuNavigationTask

**Description:** Processes menu navigation state and updates menu presentation.

**Callers:** `0x0020a2b0`  
**Callees:** `0x00203240`, `0x00204d60`, `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x002063c8`, `0x00206b00`, `0x00207d80`, `0x0021d718`, `0x0021d788`, `0x0021dae8`, `0x00258e50`, `0x00204de8`, `0x00204c48`, `0x00207b50`
**Key globals / structures:** None identified in the inventory note.

## 0x0020A150 UpdateMenuOptionTask

**Description:** Updates a menu option task and dispatches its transition state.

**Callers:** `0x0020a2b0`  
**Callees:** `0x00204310`, `0x002080f8`, `0x00258f30`, `0x00258fb0`, `0x00258ec8`, `0x00204de8`, `0x00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x0020A2B0 DispatchMenuTask

**Description:** Selects and spawns the next menu task based on task and mode state.

**Callers:** `0x00205088`, `0x002052d0`  
**Callees:** `0x00204d60`, `0x00204ed0`, `0x00258e50`, `0x00258f50`, `0x00204de8`, `0x00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x0020A520 UpdateMenuConfirmTask

**Description:** Spawns a confirmation task and processes its completion state.

**Callers:** `0x0020a2b0`  
**Callees:** `0x00204de8`, `0x00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x0020A580 UpdateMenuScreenTask

**Description:** Starts a screen task and advances its timer/transition state.

**Callers:** `0x00209cb0`  
**Callees:** `0x002080f8`, `0x00204de8`, `0x00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x0020E030 SendTexturePacketsDMA

**Description:** Walks packet list until 'p' marker, adjusts texture field, waits DMA channel 1, then calls sendTexturePackets.

**Callers:** —  
**Callees:** `002006f0`, `00270180`, `00270a20`, `00207b50`
**Key globals / structures:** None identified in the inventory note.

## 0x0020E708 UpdateTownBlockTasks

**Description:** Updates town/world block tasks based on active block index, spawning block entities or destroying inactive blocks.

**Callers:** `0x00210430`  
**Callees:** `0x00204D60`, `0x00204ED0`, `0x0021CE28`, `0x00221258`, `0x0023CD58`, `0x0023DB80`, `0x00204C48`, `0x0027EA30`
**Key globals / structures:** None identified in the inventory note.

## 0x0020EBE8 SpawnGameViewTasks

**Description:** Spawns HUD and camera viewport tasks for 1-player (mode 0/2) or 2-player split screen (mode 1) [inferred].

**Callers:** `0x0020F950`, `0x00210430`  
**Callees:** `0x00204ED0`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x0020ED30 SpawnRaceRivalTasks

**Description:** Spawns rival car tasks for race modes and populates race participant slot table DAT_01787008 [inferred].

**Callers:** `0x0020F950`  
**Callees:** `0x00204ED0`, `0x0023DB80`, `0x00204C48`
**Key globals / structures:** `DAT_01787008`

## 0x0020F3B8 UpdateRaceGameTask

**Description:** Main per-frame task handler for race mode; updates day/night lighting, checks pause/finish state, and spawns exit transitions.

**Callers:** `0x0020F950`  
**Callees:** `0x00204D60`, `0x002080F8`, `0x0020F330`, `0x0021CBC8`, `0x0021CCB8`, `0x0022E5F8`, `0x00237C48`, `0x00238950`, `0x0023D8C8`, `0x00258E50`, `0x00258F70`, `0x00258F90`, `0x00258FF0`, `0x00259050`, `0x00258EC8`, `0x00204DE8`, `0x00204C48`, `0x0026E4F0`, `0x00226248`, `0x00225988`
**Key globals / structures:** None identified in the inventory note.

## 0x0020F950 StartRaceModeTask

**Description:** Initializes race mode: sets race course table, spawns HUD, rivals, and environment tasks, and assigns race update callback.

**Callers:** `0x0020A150`, `0x002105B8`, `0x00211170`, `0x00211320`, `0x0026B1B8`, `0x0026B470`, `0x0026B5C8`  
**Callees:** `0x00204ED0`, `0x002080D8`, `0x0020EBE8`, `0x0020ED30`, `0x0021CE08`, `0x0021CE28`, `0x00225790`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x00210158 SpawnTownEntities

**Description:** Spawns resident and landmark tasks for a town based on resident table pointers and progress state [inferred].

**Callers:** `0x002105B8`, `0x0026CA48`  
**Callees:** `0x002080D8`, `0x0023DB80`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x00210430 StartTownModeTask

**Description:** Sets town mode state, spawns viewport HUD, town block manager, player car, and sound tasks [inferred].

**Callers:** `0x002105B8`, `0x0026CA48`  
**Callees:** `0x00204ED0`, `0x0020EBE8`, `0x0021CE08`, `0x00225790`, `0x002590B0`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x002105B8 UpdateTownExplorationTask

**Description:** Main loop task callback for town exploration; updates day/night time, checks mail, dialogue, shop, and race entry states.

**Callers:** `0x00211808`  
**Callees:** `0x00204A00`, `0x00204D60`, `0x00204EE0`, `0x00204F18`, `0x00204F58`, `0x00210158`, `0x00210430`, `0x0021C6A0`, `0x00225790`, `0x0022E718`, `0x0023D628`, `0x0023D778`, `0x0023D7E0`, `0x0023D878`, `0x0023DB80`, `0x0023DE10`, `0x0023DFC0`, `0x00258B30`, `0x00258E50`, `0x00258FF0`, `0x00259050`, `0x002590B0`, `0x00211154`, `0x00204DE8`, `0x00204C48`, `0x0027EA30`, `0x00225988`
**Key globals / structures:** None identified in the inventory note.

## 0x00211170 UpdateQuickRaceMenuTask

**Description:** Handles menu flow and race selection for Quick Race mode, spawning selection UI and launching the race [inferred].

**Callers:** `0x00205088`  
**Callees:** `0x00204D60`, `0x00229FD8`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x00211320 UpdateTwoPlayerMenuTask

**Description:** Handles menu flow for 2-player race mode, setting up player selections and launching 2-player race task [inferred].

**Callers:** `0x00205088`  
**Callees:** `0x00204D60`, `0x002080F8`, `0x00237AC0`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x00211530 SpawnTasksFromCarTable

**Description:** Fills 0x177ea60 table with per-entry addresses from DAT tables and spawns task 0x20dc50 via gs_tasks_spawn [inferred].

**Callers:** —  
**Callees:** `0023db80`, `00204c48`
**Key globals / structures:** `0x177ea60`, `0x20dc50`, `gs_tasks_spawn`

## 0x00211648 SpawnTasksFromResidentTable

**Description:** Builds address table from gs_houses_residents entries and spawns task 0x20dc50 via gs_tasks_spawn [inferred].

**Callers:** —  
**Callees:** `00204c48`
**Key globals / structures:** `gs_houses_residents`, `0x20dc50`, `gs_tasks_spawn`

## 0x00211708 SpawnTownDialogueTasks

**Description:** Spawns resident dialogue and camera tasks for current town resident or town center [inferred].

**Callers:** `0x0026BE50`, `0x0026D628`  
**Callees:** `0x0023DB80`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x00211808 StartTownExplorationTask

**Description:** Initializes town resident tasks and sets task callback to UpdateTownExplorationTask (FUN_002105b8).

**Callers:** `0x0026BE50`, `0x0026D370`, `0x0026D678`  
**Callees:** `0x0023DB80`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x00211948 FadeOutMenuTask

**Description:** Advances a screen fade step by calling FUN_002080f8, and notifies parent task before terminating at step 0x40.

**Callers:** `0x00211170`, `0x00211320`  
**Callees:** `0x002080F8`, `0x00204DE8`
**Key globals / structures:** None identified in the inventory note.

## 0x00214AD0 UpdatePauseMenuTask

**Description:** Updates pause menu navigation, processes selected menu option, and renders menu background, borders, and items [inferred].

**Callers:** `0x00217148`  
**Callees:** `0x00203240`, `0x002119A8`, `0x00211F68`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x00212AC8`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x00214EF8 UpdatePauseSubmenuTask

**Description:** Handles navigation and renders dialog layout for a secondary pause submenu screen [inferred].

**Callers:** `0x00217148`  
**Callees:** `0x002119A8`, `0x00211F68`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x00212AC8`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`
**Key globals / structures:** None identified in the inventory note.

## 0x00215220 UpdatePauseConfirmDialogTask

**Description:** Updates input and renders dialog frames and options for pause menu confirmation prompt (restart/quit) [inferred].

**Callers:** `0x00217148`  
**Callees:** `0x002119A8`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x002135C8`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x002156D0 UpdateAudioSettingsMenuTask

**Description:** Updates audio settings menu input and renders volume sliders, time of day display, and sound option controls [inferred].

**Callers:** `0x00217148`  
**Callees:** `0x002114E0`, `0x002119A8`, `0x00211F68`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x002137B0`, `0x0024EB10`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0027E840`
**Key globals / structures:** None identified in the inventory note.

## 0x002161E0 UpdateGarageMenuTask

**Description:** Displays garage customization items, item descriptions, and player travel mileage in pause menu [inferred].

**Callers:** `0x00217148`  
**Callees:** `0x00202150`, `0x00202350`, `0x00203F40`, `0x00205350`, `0x00205C20`, `0x00205DA0`, `0x002119A8`, `0x00211F68`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x00213A78`, `0x0022E780`, `0x0023D488`, `0x0023D5B0`, `0x0024EA00`, `0x0024F3B8`, `0x0024F530`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0027EEE0`, `0x0027EF70`, `0x0027F2F4`
**Key globals / structures:** None identified in the inventory note.

## 0x00216E00 UpdateWorldMapMenuTask

**Description:** Displays the pause menu world map and renders the player position marker icon at converted world coordinates.

**Callers:** `0x00217148`  
**Callees:** `0x0024F530`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x00258E50`, `0x0027E840`
**Key globals / structures:** None identified in the inventory note.

## 0x002173D0 PromptConfigConfirmationTask

**Description:** Displays a confirmation popup dialog with options and returns the selected index to the parent task.

**Callers:** `0x00214470`  
**Callees:** `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x00204DE8`
**Key globals / structures:** None identified in the inventory note.

## 0x002179D8 UpdateCarCarCollisionsTask

**Description:** Iterates over all car pairs, performs broadphase and narrowphase collision checks, and applies impact physics and reactions.

**Callers:** `0x0020F950`, `0x00210430`  
**Callees:** `0x002175D0`, `0x00217830`, `0x0021D310`
**Key globals / structures:** None identified in the inventory note.

## 0x00218458 SpawnCarEntityTask

**Description:** Initializes car entity state, physics matrix, equipped parts, spawn position, and assigns car update callback FUN_0021ba70.

**Callers:** `0x0020E708`, `0x0020ED30`, `0x0020F950`, `0x00210430`  
**Callees:** `0x002180C0`, `0x002182B0`, `0x0021BA70`, `0x0024FCD8`, `0x002574A0`, `0x00258E50`, `0x0027E840`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x0021ED90 CameraWaitOrCutsceneState

**Description:** Wait state: resumes main update when player flag clears, else builds fixed scene cam (types 0x17/0x18) from stored matrices [inferred]

**Callers:** `0x0021E690`, `0x00238B18`  
**Callees:** `0x00204D20`, `0x0021E690`, `0x0021F698`, `0x002729D0`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x002201C0 DrawFieldTerrainTiles

**Description:** For drawSceneField: streams 5 neighbour tile GS tasks by FUN_0021c8d0, updates DAT_003335b8 tile-visible flags, then debug-overlays regs/coordinates. [inferred]

**Callers:** `0x00220938`  
**Callees:** `0x00203a50`, `0x0021c8d0`, `0x0027eee0`, `0x00204c48`
**Key globals / structures:** `DAT_003335b8`

## 0x00228510 MemcardReadSlotTask

**Description:** Opens RDONLY via sceMcOpen then issues sceMcRead into the per-slot progress buffer (0x3448 bytes); on result, chains to FUN_002283e0. [inferred]

**Callers:** `0x00228b10`, `0x00228f40`, `0x002291c8`, `0x0022a320`  
**Callees:** `0x00279b08`, `0x00279e98`, `0x0027a1a8`, `0x0027ea30`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00228708 MemcardCreateSlotTask

**Description:** Multi-step memcard-create state machine (mkdir → icon.sys → choroQ.ico → save file), writing icon/header/save blocks via sceMcOpen/Write/Close. [inferred]

**Callers:** `0x002291c8`, `0x0022a168`  
**Callees:** `0x00204310`, `0x00204568`, `0x00258e50`, `0x00279b08`, `0x00279c40`, `0x00279c78`, `0x00279fb8`, `0x0027a1a8`, `0x0027ea30`, `0x00204c48`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00228B10 LoadSaveSlotTask

**Description:** Async state machine that wraps MemcardGetInfo (0x2282c0) followed by MemcardReadSlot (0x228510) and the save-confirm dialog (0x2280d8) — load selected slot. [inferred]

**Callers:** `0x00228d10`  
**Callees:** `0x00204c48`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** `0x2282c0`, `0x228510`, `0x2280d8`

## 0x00228F40 LoadOrCreateCardSlotTask

**Description:** State machine: probes slot occupancy (info+read), then dispatches between overwrite/create/load paths via FUN_00227380 chooser and kill targets. [inferred]

**Callers:** `0x00209848`  
**Callees:** `0x00204c48`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x002291C8 CreateOrOverwriteSlotTask

**Description:** Full memcard-create sub-flow with overwrite-support: spawns info/read/create (0x28708) and confirm (0x280d8) tasks based on per-byte sub-status. [inferred]

**Callers:** `0x00229538`, `0x00229758`  
**Callees:** `0x00204c48`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** `0x28708`, `0x280d8`

## 0x00229758 CommitSaveSlotTask

**Description:** Copies slot buffer to RAM staging (DAT_00110000) on entry, then asynchronously calls FUN_002291c8 with packed slot index, copies back on finish. [inferred]

**Callers:** `0x00248bf0`  
**Callees:** `0x00204c48`, `0x00204de8`
**Key globals / structures:** `DAT_00110000`

## 0x0022A050 RunConfirmDialogTask

**Description:** Menu task: first tick spawns a UI panel and clamps two globals to 3; later ticks poll it and on success kill the task. [inferred]

**Callers:** `0x00209848`  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x00227f90`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022A2F8 CompleteTaskSuccess

**Description:** Trivial stub: sets the parent's done flag and result 1, then kills the current task.

**Callers:** `0x00229af8`  
**Callees:** `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022A320 RunCardOpSequenceTask

**Description:** Sequencer: spawns sub-task 0x2282c0, maps its result (1->1, other->2) to the parent, or spawns 0x228510 on result 2. [inferred]

**Callers:** `0x00229af8`  
**Callees:** `0x00204de8`, `0x00204c48`
**Key globals / structures:** `0x2282c0`, `0x228510`

## 0x0022BB88 RunSaveProgressTask

**Description:** Writes course/progress records to the save buffer, spawns follow-up tasks, rebuilds a menu panel, prints Acc/Mue debug. [inferred]

**Callers:** `0x0022aeb0`, `0x00248580`  
**Callees:** `0x00205350`, `0x00205ad0`, `0x00205c20`, `0x00205da0`, `0x0023c418`, `0x00204de8`, `0x00204c48`, `0x0027ea30`
**Key globals / structures:** None identified in the inventory note.

## 0x0022BFD0 RunSceneIntroTask

**Description:** First tick sets up transforms, camera, light and VIF upload; later ticks render and finish on input bits 0x50. [inferred]

**Callers:** `0x0023b168`, `0x002665b8`  
**Callees:** `0x00206650`, `0x002075f0`, `0x0021d718`, `0x0021dae8`, `0x0021f910`, `0x00223200`, `0x002236d8`, `0x00225790`, `0x00272c38`, `0x00273268`, `0x0021f698`, `0x00204de8`, `0x00204c48`, `0x002261a0`, `0x00226f70`, `0x002266e8`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0022C418 RunFadeOutWaitTask

**Description:** Raises a fade flag via FUN_002080f8 and sets bit 0x400000; past tick 63 with input idle it finishes with result 5.

**Callers:** `0x0022aeb0`, `0x0022cd38`  
**Callees:** `0x002080f8`, `0x00258f50`, `0x00204de8`
**Key globals / structures:** `0x400000`

## 0x0022C4A8 RunFadeHoldTask

**Description:** Sets the fade flag and bit 0x400000 while tick is 0 or input is active, unless tick equals 64, then kills itself. [inferred]

**Callers:** `0x0020f950`  
**Callees:** `0x002080f8`, `0x00204de8`
**Key globals / structures:** `0x400000`

## 0x0022C5F0 FinishTaskOnInputIdle

**Description:** When controller state +0xf is zero, sets the parent's done flag/result to 1 and kills the current task.

**Callers:** `0x0022bfd0`  
**Callees:** `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022C8A0 RunSelectionMenuTask

**Description:** Builds a menu of up to 10 unlocked progress entries, polls it, then reports the chosen index (SE 0x1a) or cancel (0x1b). [inferred]

**Callers:** `0x0022cd38`  
**Callees:** `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x0023d488`, `0x0023d5c8`, `0x00243468`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022CA28 RunCourseSelectMenuTask

**Description:** Scans 20 (area,course) pairs, builds a bitmask of available courses, shows them by name and returns the chosen entry. [inferred]

**Callers:** `0x0022cd38`  
**Callees:** `0x00205350`, `0x00205ad0`, `0x00205c20`, `0x00205da0`, `0x00208228`, `0x0023d5f8`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022CD38 RunCourseSelectHubTask

**Description:** First tick builds camera/light and music, then dispatches menu choices to 0x22c8a0/0x22ca28 and applies the course. [inferred]

**Callers:** `0x0022d1e0`  
**Callees:** `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x0021f910`, `0x00225790`, `0x0022cc38`, `0x0023c7b0`, `0x0023d5b0`, `0x00258e50`, `0x00258f30`, `0x00258fb0`, `0x00272c38`, `0x00274af8`, `0x0021f698`, `0x00258ec8`, `0x00204de8`, `0x00204c48`, `0x002261a0`, `0x00272a48`
**Key globals / structures:** `0x22c8a0`, `0x22ca28`

## 0x0022DAE8 RunCountdownTimerTask

**Description:** Fades in over 64 ticks, triggers SE 0x2d at tick 60, a flag at 240, then after 300 ticks ends the event and kills itself. [inferred]

**Callers:** `0x0022ddd0`, `0x00235868`  
**Callees:** `0x002080f8`, `0x002329a0`, `0x00258e50`, `0x00258f30`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022DC68 FinishOnCarApproach

**Description:** Scans cars; the first within 110 units sets rank bits, switches handler to 0x22eac8 and kills this task. [inferred]

**Callers:** `0x0022ddd0`  
**Callees:** `0x00258fb0`, `0x00258ff0`, `0x00204de8`
**Key globals / structures:** `0x22eac8`

## 0x0022EAC8 RunFadeOutExitTask

**Description:** Fades out over 64 ticks via FUN_002080f8, then clears globals, reports result -1, kills the task and sets a completion bit.

**Callers:** `0x0022dc68`  
**Callees:** `0x002080f8`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022EE00 KillTask

**Description:** Sets a task slot's timer/kill state and tears it down when it overflows [inferred]

**Callers:** `0x0022eb78`  
**Callees:** `0x002080f8`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00233A80 UpdateTimedPhaseTask

**Description:** Per-frame task state machine: counts 60-frame phases, sets up draw state per phase, kills task on timeout.

**Callers:** `002357f0`, `00235838`, `00235808`, `00235850`, `00235820`  
**Callees:** `002080f8`, `00258e50`, `00258f30`, `00204de8`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00233F10 InitHudConfig

**Description:** Initializes a HUD config block with size, timing, and callback data [inferred]

**Callers:** `0x0022ddd0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00233FC8 InitHudConfig2

**Description:** Initializes a second HUD config block with different widths and callback data [inferred]

**Callers:** `0x0025a120`, `0x0025e630`, `0x0025eb70`, `0x0025f6a0`, `0x0025fbf8`, `0x0025ff00`, `0x00260638`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00236858 UpdateRankingsAndAdvance

**Description:** Computes 8 score sums, sorts them, draws ranking rows and advances round state; kills task when done [inferred].

**Callers:** `00237388`  
**Callees:** `00202150`, `00203fc8`, `00204310`, `00204568`, `0022e7f0`, `0022ea08`, `0023dc58`, `00258e50`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00239150 DisplayTextDialog

**Description:** Builds a text object with two appended strings and submits it, or polls completion and ends the task with its result. [inferred]

**Callers:** `0x00248a70`, `0x00248bf0`  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00239378 ShowAwardMessage

**Description:** Displays an award/status message with a list of seven values, then kills the task when the display completes. [inferred]

**Callers:** `0x00239748`  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x0023d2c0`, `0x0023da58`, `0x0023db00`, `0x00258e50`, `0x00258f70`, `0x00258fd0`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00239508 ShowResidentMessage

**Description:** Shows a message, then updates the gs_houses_residents pointer table and player slot before ending the task. [inferred]

**Callers:** `0x00239748`  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x00239748`, `0x0023d390`, `0x0023db00`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** `gs_houses_residents`

## 0x00239A80 DrawCourseList

**Description:** Lists courses in a range with names and ranks and, on selection, spawns the message task at 0x23bb00. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x0023d628`, `0x00258e50`, `0x00204de8`, `0x00204c48`
**Key globals / structures:** `0x23bb00`

## 0x00239F68 ShowRankedMessage

**Description:** Builds a message string via FUN_00243468 and a type switch (cases 1-7) and displays it, ending the task when done. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x0023d2c0`, `0x00243468`, `0x00258e50`, `0x00258f70`, `0x00258fd0`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023A320 HandleNumberSpinner

**Description:** Edits a 0-99 counter with up/down input (0x4000/0x1000), draws it, and ends the task on confirm. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x00201e48`, `0x00202350`, `0x00203e70`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023B168 dialogueTask

**Description:** Maintained symbol-map entry: message window task ('next msg #%d')

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023BB00 ShowMessageAndWait

**Description:** Displays a single message string and waits for it to finish, then ends the task with result 0. [inferred]

**Callers:** `0x00239a80`, `0x00256640`  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023BB88 UNKNOWN_0x0023BB88

**Description:** Task step: first run loads two string tables into a text object via FUN_002058a8; later runs poll FUN_00205c08 and kill task [inferred]

**Callers:** `00256640`  
**Callees:** `002058a8`, `00205c08`, `00205c20`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023BC90 UNKNOWN_0x0023BC90

**Description:** Sets task handler to FUN_00239260, marks state byte +0xd=1, and calls FUN_00239260 once [inferred]

**Callers:** `0023b168`  
**Callees:** `00239260`
**Key globals / structures:** None identified in the inventory note.

## 0x0023BCD0 UNKNOWN_0x0023BCD0

**Description:** Task step reading tab-terminated text records from a pointer; advances the cursor, then waits on FUN_00205c08 and kills on -2 [inferred]

**Callers:** `0023b168`  
**Callees:** `002058a8`, `00205ad0`, `00205c08`, `00205c20`, `00258e50`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023BE18 UNKNOWN_0x0023BE18

**Description:** Spawns task 0x25bcf0 on first run; when flag +0x24 set, copies a byte into parent +4, sets flag, kills self

**Callers:** `0023b168`  
**Callees:** `00204de8`, `00204c48`
**Key globals / structures:** `0x25bcf0`

## 0x0023BE88 UNKNOWN_0x0023BE88

**Description:** Spawns task 0x2083c0 with count from table indexed by byte at +0x1c; on state 0x40 copies byte+1 to parent and kills self

**Callers:** `0023b168`  
**Callees:** `00204de8`, `00204c48`
**Key globals / structures:** `0x2083c0`

## 0x0023DDB8 FindCurrentTaskIndex

**Description:** Returns index 0-5 of DAT_00335734 in the DAT_002a2b30 table; 0 when first entry matches or not found [inferred]

**Callers:** `0023ad00`  
**Callees:** —
**Key globals / structures:** `DAT_00335734`, `DAT_002a2b30`

## 0x002406F8 UpdateProximityEvent

**Description:** Runs a proximity-triggered multi-state event, moving its vector state and spawning or killing related tasks. [inferred]

**Callers:** —  
**Callees:** `0x002080D8`, `0x0021D358`, `0x0023D2C0`, `0x00258E50`, `0x002727C0`, `0x00272958`, `0x00274B10`, `0x00240BBC`, `0x00204DE8`, `0x0027EA30`, `0x002729D0`
**Key globals / structures:** None identified in the inventory note.

## 0x002412E0 UpdateConditionalEffects

**Description:** Spawns or removes three conditional tasks and invokes enabled effect handlers each frame. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00221258`, `0x0023E018`, `0x0023F540`, `0x00204C48`, `0x0027EA30`
**Key globals / structures:** None identified in the inventory note.

## 0x00248FA0 UpdateTaskTimerLimit

**Description:** Advances task timer by 8 per step up to 16, kills task on overflow; flag-dependent path [inferred].

**Callers:** —  
**Callees:** `00203990`, `00258e50`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00254CC8 GetTuningParamFromCarSlot

**Description:** Returns a tuning-table value from car object (offset 600 + slot*12 + 0x74) after calling FUN_00253680, only when task flag bit 2 is set. [inferred]

**Callers:** —  
**Callees:** `0x00253680`
**Key globals / structures:** None identified in the inventory note.

## 0x0026B018 AdjusterMenuTask

**Description:** Task creating 0x32F8D0/E/F0 menu items, selects via FUN_00205C08; status 2 toggles 0x10000 mode + plays sfx 0x1A, 0 exits task. [inferred]

**Callers:** —  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x002080f8`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** `0x32F8D0`, `0x10000`

## 0x0026B848 UNKNOWN_0x0026B848

**Description:** Handles the global race-choice menu and dispatches the selected subtask for the current mode.

**Callers:** `00205088`  
**Callees:** `002058a8`, `00205c08`, `00205c20`, `00227c68`, `00229fd8`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x0026BE50 ProcessLocationArrival

**Description:** Hub arrival: builds location asset list, writes per-resident status bytes, picks interaction target, spawns follow-up tasks. [inferred]

**Callers:** `0x0026C460`  
**Callees:** `0x00204D60`, `0x00205350`, `0x00205C20`, `0x00205DA0`, `0x00211708`, `0x00211808`, `0x0023CD58`, `0x0023D5F8`, `0x0023DB80`, `0x0023DEB8`, `0x00204DE8`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

# Text, Dialogues & General UI Primitives

Font glyphs, dialog/text construction, HUD primitives, menus, and generic input widgets.

## 0x002006F0 fatal

**Description:** Maintained symbol-map entry: prints a message and stops (assert)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002008C0 uiDrawSmallText

**Description:** Maintained symbol-map entry: 10x20 small font (atlas v = 0xB4 + 20 * row)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00200DC0 uiDrawIcon3

**Description:** Maintained symbol-map entry: three small icons (palette banks 2, 2, 5); guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00200FE8 uiKanaGlyphUV

**Description:** Maintained symbol-map entry: Shift-JIS below 0x889F to atlas (u, v) of a 20x20 glyph

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002012F8 uiKanjiGlyphUV

**Description:** Maintained symbol-map entry: Shift-JIS from 0x889F to atlas (u, v) of a 20x20 glyph

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00201420 uiDrawGlyphScaled

**Description:** Maintained symbol-map entry: one character scaled (sprite built in three parts)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002017B0 uiDrawGlyph

**Description:** Maintained symbol-map entry: one character (x, y2, shade, str): Latin 16x24 cells, Shift-JIS 20x20, returns next x

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00201C80 uiDrawTextRight

**Description:** Maintained symbol-map entry: right-aligned string

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00201E48 uiDrawNumber16

**Description:** Maintained symbol-map entry: 16x20 digits, right aligned at x

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00201FD0 uiDrawNumber16Y2

**Description:** Maintained symbol-map entry: uiDrawNumber16 with y - 2

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00202020 GsDrawDigitsCounter

**Description:** Writes GS prim packets into DMA buffer to render number param_1 as 200 (digit%10<4) or 220 (else) sprites at x-2,y via FUN_002063C8. [inferred]

**Callers:** `0x002207d0`, `0x00264498`  
**Callees:** `0x002063c8`
**Key globals / structures:** None identified in the inventory note.

## 0x00202150 uiDrawNumber10

**Description:** Maintained symbol-map entry: 10x20 digits with thousands commas

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00202350 uiDrawPanel9

**Description:** Maintained symbol-map entry: window frame: 3x3 sprites (x, y, w, h, style = palette bank 7/0xE/8)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00202B70 uiDrawDialogueFrameB

**Description:** Maintained symbol-map entry: big message window frame, lower half

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00202D10 DrawDialogTextLine

**Description:** Draws text line in dialog packet chain: 3.5 sprite slabs via FUN_002063C8/C0/C8, shadow via FUN_00201FD0, then glyph loop FUN_002017B0 over chars incl wide-lead bytes. [inferred]

**Callers:** `0x0022e780`, `0x0022e7c0`, `0x0022e7f0`, `0x0022e8c0`, `0x0022edb8`, `0x00268e40`  
**Callees:** `0x002017b0`, `0x00201fd0`, `0x002063c8`
**Key globals / structures:** None identified in the inventory note.

## 0x00203098 uiDrawBar3

**Description:** Maintained symbol-map entry: 3-slice horizontal bar (bank 0xB)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00203240 uiDrawHintBar

**Description:** Maintained symbol-map entry: bottom hint line in the language of DAT_00333284 plus a panel

**Callers:** —  
**Callees:** —
**Key globals / structures:** `DAT_00333284`

## 0x00203810 uiBatchFlush

**Description:** Maintained symbol-map entry: appends TEXA, patches NLOOP, vifDirect(base, count + 7), switches UiBatch buffer

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002038F8 uiBatchInit

**Description:** Maintained symbol-map entry: both UiBatch buffers: header with TEX0 0x2013780E65321A40 (HUD atlas 0x1A40, CLUT 0x1BC0)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x2013780E`

## 0x00203990 uiSetTexa

**Description:** Maintained symbol-map entry: TEXA entry: TA0 = x, AEM 1, TA1 = x / 2

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002039E8 uiSetPaletteBank

**Description:** Maintained symbol-map entry: TEX2_1 entry: PSMT4, CBP 0x1BC0, CT16, CSA = bank, CLD 0 (cached in uiBatchPalette)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00203A50 uiDebugText8

**Description:** Maintained symbol-map entry: 8x8 debug font (21 callers, debug overlays)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00203B78 uiDrawTime

**Description:** Maintained symbol-map entry: h:mm:ss.cc from frames (0x7FFFFFFF = dashes)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x7FFFFFFF`

## 0x00203CD0 uiDrawArrow

**Description:** Maintained symbol-map entry: small arrow icon (bank 0xD); guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00203D90 uiDrawBarWithArrow

**Description:** Maintained symbol-map entry: uiDrawBar3 plus uiDrawArrow; guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00203E70 uiDrawIcon20

**Description:** Maintained symbol-map entry: 20x20 icon n of the atlas row v = 0xB4

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00203F40 uiTextWidth

**Description:** Maintained symbol-map entry: width of a string from uiGlyphWidths (Japanese 20)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00203FC8 uiDrawText

**Description:** Maintained symbol-map entry: string at (x, y2) with palette bank

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204100 uiDrawTextRaw

**Description:** Maintained symbol-map entry: string at (x, y) with palette bank

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204230 uiDrawTextShade

**Description:** Maintained symbol-map entry: string with a shade argument, no palette write

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204310 uiDrawTextCell

**Description:** Maintained symbol-map entry: string on the 20x12 cell grid

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204460 uiDrawNumberAuto

**Description:** Maintained symbol-map entry: uiDrawNumber10 sized by the digit count

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00204568 uiDrawPanelCells

**Description:** Maintained symbol-map entry: uiDrawPanel9 from cell coordinates

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002045D0 uiDrawTextScaled

**Description:** Maintained symbol-map entry: string with uiDrawGlyphScaled

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002046C0 uiDrawNumberFixed

**Description:** Maintained symbol-map entry: 16x20 digits, fixed digit count

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00205350 uiMenuWindow

**Description:** Maintained symbol-map entry: generic list window widget (panel, rows, scroll bar, cursor, open/close fade); 10 callers

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002058A8 uiMenuAddItem

**Description:** Maintained symbol-map entry: guess: appends a row to a uiMenuWindow

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00205C08 uiMenuRun

**Description:** Maintained symbol-map entry: guess: calls uiMenuWindow

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00205DA0 LayoutTextItems

**Description:** Measures and lays out a bounded sequence of text items, updating positions and item state.

**Callers:** `0x002161e0`, `0x0022bb88`, `0x0022ca28`, `0x00264498`, `0x00268e40`, `0x00269568`, `0x00269f48`, `0x0026be50`, `0x0026c460`  
**Callees:** `0x00203f40`, `0x002058a8`
**Key globals / structures:** None identified in the inventory note.

## 0x00205FB0 MeasureTextWidth

**Description:** Recursively sums glyph widths for a text sequence, skipping marked entries.

**Callers:** `0x002058a8`, `0x00205fb0`  
**Callees:** `0x00203f40`, `0x00205fb0`
**Key globals / structures:** None identified in the inventory note.

## 0x00206040 uiSpriteWriteWrap

**Description:** Maintained symbol-map entry: uiSpriteWrite with REPEAT / REGION_REPEAT wrap modes

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002063C8 uiSpriteWrite

**Description:** Maintained symbol-map entry: 5 A+D entries of one sprite: CLAMP_1 region = atlas cell, UV, XYZ2, UV, XYZ2 (Z 0xFFFFFE)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0xFFFFFE`

## 0x00206650 uiSpriteWriteZ

**Description:** Maintained symbol-map entry: uiSpriteWrite with an explicit Z

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002068D0 uiSpriteWriteWH

**Description:** Maintained symbol-map entry: uiSpriteWrite with separate source and destination size

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00206B00 uiSpriteWriteFull

**Description:** Maintained symbol-map entry: uiSpriteWrite for pictures

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002075F0 uiSetSpriteXY

**Description:** Maintained symbol-map entry: writes only the two XYZ2 positions of an entry

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002076B8 uiSetSpriteUV

**Description:** Maintained symbol-map entry: writes only the two UV values of an entry

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002078A0 uiSetSpriteClamp

**Description:** Maintained symbol-map entry: writes only the CLAMP_1 entry

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00207998 uiHideSprite

**Description:** Maintained symbol-map entry: turns the two XYZ2 selectors into XYZ3 (no kick)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002079A8 uiMoveSprite

**Description:** Maintained symbol-map entry: moves entry n of a retained batch

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00207A28 uiSetQuadXY

**Description:** Maintained symbol-map entry: writes the four packed corners of a gifQuadTag quad

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00207A88 uiFlatVertexWrite

**Description:** Maintained symbol-map entry: flat colour vertex pair {RGBAQ, XYZ2}

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00211F68 DrawMenuListItems

**Description:** Draws menu text labels and options (e.g. Pause menu items) positioned within menu grid cells.

**Callers:** `0x00214AD0`, `0x00214EF8`, `0x002156D0`, `0x002161E0`  
**Callees:** `0x0024F530`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`
**Key globals / structures:** None identified in the inventory note.

## 0x00212720 DrawMenuCursorAndIcons

**Description:** Draws menu cursor arrows with blinking animation and item icons corresponding to selected grid elements.

**Callers:** `0x00214AD0`, `0x00214EF8`, `0x00215220`, `0x002156D0`, `0x002161E0`, `0x00216738`  
**Callees:** `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0027E840`
**Key globals / structures:** None identified in the inventory note.

## 0x00212AC8 UpdateVerticalMenuNavigation

**Description:** Updates vertical menu cursor navigation, handles scrolling, plays navigation sound effects, and steps transition animations.

**Callers:** `0x00214AD0`, `0x00214EF8`  
**Callees:** `0x00206D28`, `0x00258E50`
**Key globals / structures:** None identified in the inventory note.

## 0x002135C8 HandleConfirmationMenuInput

**Description:** Handles navigation, confirmation, and cancel input for a three-option dialog menu [inferred].

**Callers:** `0x00215220`  
**Callees:** `0x00206D28`, `0x00258E50`
**Key globals / structures:** None identified in the inventory note.

## 0x0021D0C0 TestSegmentInBoxWindow

**Description:** Clips a segment between two 3D pts to a +/-110 X window, tests clipped Z span against +/-110 of param_2; returns visibility [inferred]

**Callers:** `0x0021FA38`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00223490 UploadCarColorPalette

**Description:** Loads two colours from 0x29d2d0/0x29d2e0 and kicks a 2-cycle DMA (texture/colour palette apply for car). [inferred]

**Callers:** `0x0022aeb0`  
**Callees:** `0x00226f70`, `0x00226a18`
**Key globals / structures:** `0x29d2d0`, `0x29d2e0`

## 0x00227380 MemcardChooserDialogHandler

**Description:** State machine over dozens of dialog phase IDs (0..0x1f): loads text labels into a chooser (FUN_002058a8) by enum, advances phases or marks confirm/cancel. [inferred]

**Callers:** `0x00209848`, `0x00228b10`, `0x00228f40`, `0x002291c8`  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x002280D8 SaveSlotConfirmDialog

**Description:** Phase handler for the save-confirmation dialog (overlay choose): prompts overwrite question via FUN_002058a8, draws save preview via FUN_00227f90, completes or cancels. [inferred]

**Callers:** `0x00228b10`, `0x002291c8`  
**Callees:** `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x00227f90`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00228D10 ShowSaveLoadSpinner

**Description:** Spinner/dialog phase that shows localized "Loading..." text per language and transitions into FUN_00228b10 once confirmed. [inferred]

**Callers:** `0x00209520`, `0x0020a580`  
**Callees:** `0x00204c48`, `0x00204de8`, `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x00229538 ShowSaveCreateSpinner

**Description:** Spinner/dialog phase with localized "Saving..." prompt that transitions into FUN_002291c8 once the user confirms. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x00204c48`, `0x00204de8`, `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x0022E7F0 DrawScoreAndRemaining

**Description:** Draws score and clamped score-param_1 (returns -1 underflow, 1 overflow) plus a digit glyph via FUN_00202d10.

**Callers:** `0x002364b0`, `0x00236858`, `0x002564e8`, `0x00256640`, `0x00261638`, `0x00261de0`, `0x00268e40`, `0x00269568`, `0x00269900`  
**Callees:** `0x00202d10`, `0x00203e70`
**Key globals / structures:** None identified in the inventory note.

## 0x0022E8C0 DrawAltScoreAndRemaining

**Description:** Draws clamped (score - param_1) at (0x1b8,0x4a) with a glyph strip, returning -1/0/1 for under/ok/overflow.

**Callers:** `0x0023a460`, `0x00246250`  
**Callees:** `0x00202d10`, `0x00203e70`
**Key globals / structures:** None identified in the inventory note.

## 0x0022E960 DrawTextListRows

**Description:** Draws up to four text rows from FUN_00243848 via FUN_00204310, laying a panel with FUN_00204568 for the first. [inferred]

**Callers:** `0x00246250`  
**Callees:** `0x00204310`, `0x00204568`, `0x00243848`
**Key globals / structures:** None identified in the inventory note.

## 0x00235880 InitScreenLayout

**Description:** Initialises a fixed HUD/result layout: sets header fields and builds six text/panel widgets with hard-coded screen coordinates. [inferred]

**Callers:** `0x00235b58`, `0x00237388`, `0x00237508`  
**Callees:** `0x00206040`, `0x002063c8`, `0x002075f0`, `0x0021d718`, `0x0021d7b0`, `0x0021dae8`
**Key globals / structures:** None identified in the inventory note.

## 0x002364B0 DrawLicenseResultsScreen

**Description:** Draws the license-award screen (per-player results, list rows, "A/B-License" text) and applies award/state changes on confirm. [inferred]

**Callers:** `0x00237508`  
**Callees:** `0x00203fc8`, `0x00204310`, `0x00204568`, `0x0022e7f0`, `0x00235f10`, `0x00236328`, `0x00237c48`, `0x0023cb68`, `0x0023d628`, `0x0023dc58`, `0x0023df38`, `0x0023dfa0`, `0x0023c8a8`, `0x00204de8`, `0x00226f70`
**Key globals / structures:** None identified in the inventory note.

## 0x00236D28 DrawRowsAndUpdateState

**Description:** Draws 8 rows of text/icon entries using per-row tables, toggles state byte at +0x25 when input bit 0x40 set [inferred].

**Callers:** `00237388`  
**Callees:** `00202150`, `00203fc8`, `00204310`, `00204568`, `0022ea08`, `00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x00238950 AwardRaceResult

**Description:** Chooses a win/lose outcome value for message types '&' (0x26) and '(' (0x28) from race stats and passes it to FUN_0023d8c8. [inferred]

**Callers:** `0x0020f3b8`  
**Callees:** `0x0023d8c8`
**Key globals / structures:** None identified in the inventory note.

## 0x00239260 DisplayMenuMessage

**Description:** Builds a five-line text object from the player table and submits it; on completion returns a byte from the results table. [inferred]

**Callers:** `0x0023b168`, `0x0023bc90`  
**Callees:** `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00239748 ShowSequencedMessages

**Description:** Steps through message pages via FUN_0023dbc8 and chains to FUN_00239378 or FUN_00239508 based on the returned page. [inferred]

**Callers:** `0x00239508`, `0x0023b168`  
**Callees:** `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x00239378`, `0x00239508`, `0x0023db00`, `0x0023dbc8`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00239900 ShowPlayerSelect

**Description:** Builds a player-name selection text (players 0-2), submits it, and returns the chosen player index or -1. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x0023db00`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023A140 ShowPrizeScreen

**Description:** Converts the stored prize value into an on-screen amount (divide by 0x649) and draws the payout message until confirm. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x00201e48`, `0x00204310`, `0x00204568`, `0x0023dc58`, `0x00258e50`, `0x00258f70`, `0x00258fd0`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023A460 HandleAmountAdjuster

**Description:** Digit-by-digit amount editor (up/down, x10/div10, min/max steps) that commits money changes on confirm. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x00201e48`, `0x00203e70`, `0x00204310`, `0x00204568`, `0x0022e8c0`, `0x0023dc58`, `0x00258e50`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023A8E8 RenderEscapeText

**Description:** Word-wraps and draws a string, expanding '$X' codes (money, time, course name, etc.) and advancing the byte cursor. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x002017b0`, `0x002039e8`, `0x00204230`, `0x00204460`, `0x0027eee0`
**Key globals / structures:** None identified in the inventory note.

## 0x0023DFA0 SubmitMessage31

**Description:** Submits its argument to the shared message routine using fixed identifiers 0x1F and 0x31. [inferred]

**Callers:** `0x002364B0`  
**Callees:** `0x0023C6B0`
**Key globals / structures:** None identified in the inventory note.

## 0x0023DFC0 SubmitMessage2B

**Description:** Submits its argument to the shared message routine using fixed identifiers 0x50 and 0x2B. [inferred]

**Callers:** `0x002105B8`  
**Callees:** `0x0023C6B0`
**Key globals / structures:** None identified in the inventory note.

## 0x0023E140 texAnimCopyA

**Description:** Maintained symbol-map entry: per-frame GS local-to-local copies (template 0x2A3490, 3 copies: scrolling T4 windows); first of 26 sites, see docs/native-textures.md

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x2A3490`

## 0x0023E508 SubmitAnimatedTextureLayout

**Description:** Builds animated texture packets and transform matrices, then submits 13 packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x00272C38`, `0x00207B50`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x0023E7E0 SubmitThreeTexturePackets

**Description:** Builds three frame-indexed texture packets and submits 13 packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023E8D0 SubmitFiveTexturePackets

**Description:** Builds five frame-indexed texture packets and submits 21 packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023E9C0 SubmitOscillatingTextureLayout

**Description:** Updates three oscillating transforms, builds texture packets, and submits five packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x0023E018`, `0x00272AE8`, `0x00272B90`, `0x00272C38`, `0x00207B50`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x0023ECA0 SubmitEightTexturePackets

**Description:** Builds eight texture packet entries, advances one oscillating frame, and submits 33 packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x0023E018`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023EFB0 SubmitFourTexturePackets

**Description:** Builds four frame-indexed texture packets and submits 17 packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023F398 SubmitDualAnimatedTextureLayout

**Description:** Builds two frame-driven texture packet groups, advances an oscillator, and submits nine packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x0023E018`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023F540 SubmitSingleTexturePacketA

**Description:** Builds one frame-indexed texture packet from layout A and submits nine packets. [inferred]

**Callers:** `0x002412E0`  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023F630 SubmitSingleTexturePacketB

**Description:** Builds one frame-indexed texture packet from layout B and submits nine packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023F720 SubmitThreeTexturePacketsB

**Description:** Builds three frame-indexed texture packets from layout B and submits 13 packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023F810 SubmitFourTexturePacketsB

**Description:** Builds four frame-indexed texture packets from layout B and submits 17 packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023FAD0 SubmitFiveTexturePacketsC

**Description:** Builds five frame-indexed texture packets from layout C and submits 21 packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023FBC0 SubmitSingleTexturePacketC

**Description:** Builds one frame-indexed texture packet from layout C and submits nine packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023FCB0 SubmitConditionalTexturePackets

**Description:** Builds one or two texture packet groups according to a shared state query, then submits nine packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x0023D778`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0023FF58 SubmitSingleTexturePacketD

**Description:** Builds one frame-indexed texture packet from layout D and submits nine packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x00240048 SubmitSingleTexturePacketE

**Description:** Builds one frame-indexed texture packet from layout E and submits nine packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x00240138 SubmitSingleTexturePacketF

**Description:** Builds one frame-indexed texture packet from layout F and submits nine packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x00241688 SubmitAnimatedTexturePacketA

**Description:** Builds a frame-indexed texture packet from layout A and submits five packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x00241760 SubmitAnimatedTexturePacketB

**Description:** Builds a frame-indexed texture packet from layout B and submits five packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x00241840 SubmitAnimatedTexturePacketC

**Description:** Builds a frame-indexed texture packet from layout C and submits five packets. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x00241918 SubmitCounterTexturePacketA

**Description:** Builds a texture packet indexed by the effect counter and submits five packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x002419F0 SubmitCounterTexturePacketB

**Description:** Builds a texture packet indexed by the effect counter and submits five packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x00241AC8 SubmitCounterTexturePacketC

**Description:** Builds a texture packet indexed by the effect counter and submits five packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0024E7D8 CopyDialogueTextBounded

**Description:** Copies string with 2-byte escape handling up to a limit, NUL-terminates; dialogue text copy [inferred].

**Callers:** —  
**Callees:** `0027f430`
**Key globals / structures:** None identified in the inventory note.

## 0x0024EB10 uiArenaRotQuads

**Description:** Maintained symbol-map entry: rotated quads as 4-vertex strips (VU0 matrix)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024EE30 uiArenaRotQuads2

**Description:** Maintained symbol-map entry: second rotated quad emitter

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F148 uiArenaFlatRects

**Description:** Maintained symbol-map entry: flat colour vertices into an arena (linestrip tag, unused path)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F530 DrawTextHalfY

**Description:** Wrapper that halves the vertical coordinate before delegating to text drawing routine FUN_00203fc8 [inferred]

**Callers:** `00211f68`, `002161e0`, `00216e00`, `00252880`, `00252cb0`, `0025ab28`, `0025b0c0`, `002652b8`, `00265568`, `00216738`  
**Callees:** `00203fc8`
**Key globals / structures:** None identified in the inventory note.

## 0x0024F558 uiArenaInit

**Description:** Maintained symbol-map entry: arena limit = size - 0x10, used = 0 (limit 0 = hidden when arena[2] != 0)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F578 uiArenaSubmit

**Description:** Maintained symbol-map entry: vifDirect(arena + 0x10, used >> 4)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F5C8 uiArenaBeginBatch

**Description:** Maintained symbol-map entry: reserves 0x60 and writes gifBeginSpriteBatch with TEX0 from a texture table

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F640 uiArenaSetTexture

**Description:** Maintained symbol-map entry: reserves 0x20: TEX0 + TEX0 (CLD 0, CSA = bank) from a texture table

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F6D0 uiArenaSetColour

**Description:** Maintained symbol-map entry: reserves 0x20: RGBAQ A+D

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F728 uiArenaDataTag

**Description:** Maintained symbol-map entry: reserves 0x10: sprite data tag for n sprites

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F7C8 uiArenaSprites

**Description:** Maintained symbol-map entry: n sprites from 32-byte records, Z = 0xFFFFFF - (z + base)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0xFFFFFF`

## 0x0024F928 uiArenaSpritesWrap

**Description:** Maintained symbol-map entry: uiArenaSprites with wrap modes, 40-byte records

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024FAD0 DrawOptionSlider

**Description:** Draws an options menu slider or selector bar at adjusted coordinates with optional cursor highlight [inferred]

**Callers:** `00216738`  
**Callees:** `00203098`, `00203cd0`
**Key globals / structures:** None identified in the inventory note.

## 0x00259138 SpawnDoorMenuEntity

**Description:** Iterates the doors table (0x2be43c) matching route-event id at param_2+0x54, picking entries from gs_doors_table_address with special cases for doors 9 and 0xc (route/gear dialogs); spawns associated entity. [inferred]

**Callers:** `0x00210430`  
**Callees:** `0x00203fc8`, `0x0023d778`, `0x0023de10`, `0x0023deb8`
**Key globals / structures:** `0x2be43c`, `gs_doors_table_address`

## 0x0026AD48 DrawAdjusterDebug

**Description:** Text-dumps adjuster state: two shorts at 0xB0/0xB2/0xB4 and floats 0x90/0x94/0x98 via FUN_0027EEE0+FUN_00203A50. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** None identified in the inventory note.

# Car Physics, Controls & Steering

Per-vehicle integration, player controls, AI path following, drivetrain gauges, and steering helpers.

## 0x00206F28 IntersectPointWithMesh

**Description:** Tests a point against mesh triangles and records the nearest qualifying intersection.

**Callers:** `0x00207280`, `0x002084d0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020E5B8 SaveAIDataFile

**Description:** Formats 'AI Save %s' path, opens file, writes non-zero-trimmed buffer in byte-sized sceWrite calls, closes.

**Callers:** `0026d020`  
**Callees:** `0027eee0`, `0027ea30`, `00289148`, `00288ec0`, `00289770`
**Key globals / structures:** None identified in the inventory note.

## 0x00216FF8 InitPauseMenuOptions

**Description:** Initializes the pause menu state structure and populates available menu option lists based on feature unlock flags.

**Callers:** `0x002174E0`, `0x00217148`  
**Callees:** `0x0027E840`
**Key globals / structures:** None identified in the inventory note.

## 0x002175D0 TestCarCollisionBox

**Description:** Transforms 4 collision check points of one car into another car's frame and tests against bounding volume extents.

**Callers:** `0x002179D8`  
**Callees:** `0x00272958`, `0x00272728`
**Key globals / structures:** None identified in the inventory note.

## 0x00217C68 UpdateCarTireSound

**Description:** Updates tire skid and surface friction sound effects and pitch based on vehicle speed, slip, and surface type.

**Callers:** `0x0021A310`  
**Callees:** `0x002586A0`, `0x00258E50`, `0x00259070`
**Key globals / structures:** None identified in the inventory note.

## 0x002180C0 LoadCarPerformanceSpecs

**Description:** Loads equipped parts performance parameters (gears, engine, tires, brakes) into car physics state fields.

**Callers:** `0x00218458`, `0x0021BA70`, `0x0021CA90`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00218EE0 AdvanceVehicleMotion

**Description:** Advances car odometer (+0x1d0 cap 10000) and arc dist (+0x1bc) by speed; writes world delta to out vec; sets state flag bit1 [inferred]

**Callers:** `0x0021A310`  
**Callees:** `0x00203A50`, `0x00274B10`, `0x0027EEE0`
**Key globals / structures:** None identified in the inventory note.

## 0x002194B8 SteerHeadingVector

**Description:** Rotates dir vec about Y by angle, shrinks X/Z by 1/1.0625 with +/-0.125 pivot by flags 0x40/0x80, lifts Y, normalizes [inferred]

**Callers:** `0x0021B3D0`  
**Callees:** `0x002727E8`, `0x00272C38`, `0x00272728`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x0021A088 UpdateSteeringDamping

**Description:** Rate-limits steering cmd (+0x1d2, clamp +/-512), integrates yaw offset (+0x1d6), applies speed-scaled centering clamp [inferred]

**Callers:** `0x0021A310`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021A990 AcquireVehicleInput

**Description:** Routes player gs_controls_player_control or AI fnptr DAT_003356dc, keeps input ring buffer, tops nitro +0x23c [inferred]

**Callers:** `0x0021BA70`  
**Callees:** `0x00250068`, `0x00258E50`, `0x0021A820`
**Key globals / structures:** `gs_controls_player_control`, `DAT_003356dc`

## 0x0021ACD0 UpdateSuspensionFlex

**Description:** Advances suspension floats (+0x80,+0x88) from speed/arc via per-mode constants; sets +0x84 lean from steering (+0x1ce) [inferred]

**Callers:** `0x0021BA70`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021B3D0 ResolveGroundContact

**Description:** Transforms 7 wheel offsets to world, ray-tests terrain per wheel, runs suspension 0x21af28, sets airborne/landing [inferred]

**Callers:** `0x0021BA70`  
**Callees:** `0x00208008`, `0x00208080`, `0x002194B8`, `0x0021AF28`, `0x00258E50`, `0x002727A0`, `0x002727E8`, `0x00272868`, `0x00272940`, `0x00272728`
**Key globals / structures:** `0x21af28`

## 0x0021BA70 UpdateVehiclePhysics

**Description:** Master vehicle tick: input, ground-contact, surface, collision substeps, velocity drag/clamp, commits world position [inferred]

**Callers:** `0x00218458`  
**Callees:** `0x00203A50`, `0x00208008`, `0x00217F10`, `0x002180C0`, `0x002182B0`, `0x00219660`, `0x00219ED8`, `0x0021A990`, `0x0021ACD0`, `0x0021B3D0`, `0x0021D2D8`, `0x0021D358`, `0x00258E50`, `0x0026D700`, `0x0027EEE0`, `0x0021A310`, `0x0027E840`
**Key globals / structures:** None identified in the inventory note.

## 0x0021CBC8 SaveControlStateForReplay

**Description:** Copies the 15KB control-state table into replay scratch buffer and rewinds three replay stream pointers for recording [inferred]

**Callers:** `0x0020F3B8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021CE28 SetAICarController

**Description:** Installs AI car-control fn ptr DAT_003356dc (used by vehicle input dispatch 0x21a990); defaults to 0x21d0b8 when passed null

**Callers:** `0x0020E708`, `0x0020F950`  
**Callees:** —
**Key globals / structures:** `DAT_003356dc`, `0x21a990`, `0x21d0b8`

## 0x0021E070 ResolveCameraTerrainCollision

**Description:** When camera points (+0x100/+0x110) clip terrain per height query, steps camera pitch +/-128 and rebuilds camera until both clear ground

**Callers:** `0x0021E690`  
**Callees:** `0x0021F698`
**Key globals / structures:** None identified in the inventory note.

## 0x0021EC58 RunCameraZoomTransition

**Description:** Zoom transition: ramps zoom short +0x177ea06 by 512/frame until counter +0x8 hits 0x80, then resumes main cam handler [inferred]

**Callers:** `0x0021E690`  
**Callees:** `0x0021C7F0`, `0x0021DC18`, `0x0021F698`
**Key globals / structures:** `0x177ea06`

## 0x002230C8 UploadPaintBgColors

**Description:** Loads 4 RGBA paintIntensity entries indexed from RAM palette bytes and vifUploadColours them (background swap). [inferred]

**Callers:** `0x00244f60`  
**Callees:** `0x00226a18`
**Key globals / structures:** None identified in the inventory note.

## 0x00223150 UploadPackedColors

**Description:** Decodes 6 luminance indices from packed 16-bit mask, expands to paintIntensity RGBA, then vifUploadColours two 12-byte halves. [inferred]

**Callers:** `0x00244f60`  
**Callees:** `0x00226a18`
**Key globals / structures:** None identified in the inventory note.

## 0x00225790 UpdateSceneLightsAndFog

**Description:** Sets fog/sky distance+colour via scene enum, uploads light matrix and 3 light-pair matrices from tod table; called by many render scenes. [inferred]

**Callers:** `0x00208848`, `0x0020f950`, `0x00210430`, `0x002105b8`, `0x0022aeb0`, `0x0022bfd0`, `0x0022c280`, `0x0022cd38`, `0x00246250`, `0x002455c0`, `0x00216738`  
**Callees:** `0x00272e08`, `0x002729e0`
**Key globals / structures:** None identified in the inventory note.

## 0x0022AEB0 shopScene

**Description:** Maintained symbol-map entry: shop room scene: GetShopPacketArea 0x37 qw backdrop + cars

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022C3A8 getShopPacketArea

**Description:** Maintained symbol-map entry: returns 0x1785FF0 (fatal above 0x1000 qw)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x1785FF0`

## 0x0022C648 stampBookBuild

**Description:** Maintained symbol-map entry: guess: builds the stamp book packet (13 stamp sprites, hidden unless collected)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022CC38 drawModelAt177F358

**Description:** Maintained symbol-map entry: calls the model at 0x177F358 (+0x10); unknown scene

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x177F358`

## 0x00232D68 SetWheelSprite

**Description:** Init a wheel/vehicle sprite entry with width, pointer, and animation data [inferred]

**Callers:** `0x00254430`, `0x002595f0`, `0x0025eb70`, `0x0025ff00`, `0x00260638`, `0x00260dc0`, `0x002638c0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235570 UpdateMaxCounter846C

**Description:** Raises global DAT_01780a2c to the object's +0x846c value when larger; returns 1 on a new maximum. [inferred]

**Callers:** `0x002631a0`  
**Callees:** —
**Key globals / structures:** `DAT_01780a2c`

## 0x00235698 UpdateMaxCounter847C

**Description:** Raises global DAT_01780a28 to the object's +0x847c value when larger; returns 1 on a new maximum. [inferred]

**Callers:** `0x00263e68`  
**Callees:** —
**Key globals / structures:** `DAT_01780a28`

## 0x0023CD58 CheckUnlockCondition

**Description:** Returns bool whether a progress gate is satisfied: table lookup of (char,char) pairs plus special cases on params and time value [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023D5B0 CallFindFlaggedSlot

**Description:** Thin wrapper: tail-calls FindFlaggedSlotIndex (FUN_0023bf28)

**Callers:** `002161e0`, `0022cd38`  
**Callees:** `0023bf28`
**Key globals / structures:** None identified in the inventory note.

## 0x0023D5C8 CallCountFlaggedSlots

**Description:** Thin wrapper: tail-calls CountFlaggedSlots (FUN_0023c150)

**Callers:** `0022c8a0`  
**Callees:** `0023c150`
**Key globals / structures:** None identified in the inventory note.

## 0x0023D818 PopByteQueue

**Description:** Pops and returns first byte from the DAT_01782a64 queue, shifting remaining entries; returns 0 when empty

**Callers:** `00265568`  
**Callees:** —
**Key globals / structures:** `DAT_01782a64`

## 0x0023D8E8 TestFlagFc68

**Description:** Tests bit (id-1) in 128-bit flag pair DAT_0177fc68/fc70; id 0 checks all-set sentinel [inferred]

**Callers:** `0023ad00`, `002660f8`, `002665b8`  
**Callees:** —
**Key globals / structures:** `DAT_0177fc68`

## 0x0023DB80 MatchSlotPairState

**Description:** Returns 0/1/2 based on whether slot pair entries DAT_0177fdd4/fdd6 match params (char pair) [inferred]

**Callers:** `0020e708`, `0020ed30`, `00210158`, `002105b8`, `00211530`, `00211708`, `00211808`, `0023ad00`, `0026be50`, `0026d108`  
**Callees:** —
**Key globals / structures:** `DAT_0177fdd4`

## 0x0023DC58 SpendTimerBudget

**Description:** Subtracts param_1 from slot timer DAT_0177fdb4[param_2]; fails if insufficient, caps at 999999, plays FUN_00258e50(0x305) [inferred]

**Callers:** `002364b0`, `00236858`, `0023a140`, `0023a460`, `002564e8`, `00261638`, `00261de0`, `00268e40`, `00269568`, `00269900`  
**Callees:** `00258e50`
**Key globals / structures:** `DAT_0177fdb4`

## 0x0023DE50 TestProgressFlagForPair

**Description:** Looks up (char,char) pair in 25-entry table at 0x2a2b71; returns progress bit for that entry, false if not found

**Callers:** `0023ad00`  
**Callees:** —
**Key globals / structures:** `0x2a2b71`

## 0x00240CD8 UpdateTrafficHazard

**Description:** Checks a trigger region and nearby vehicles, then changes a timed hazard state and vehicle properties. [inferred]

**Callers:** —  
**Callees:** `0x0021C998`, `0x0023D628`, `0x0023D778`, `0x00240BE0`, `0x00204DE8`, `0x0027EA30`
**Key globals / structures:** None identified in the inventory note.

## 0x00241D38 drawCoins

**Description:** Maintained symbol-map entry: Choro Q coins (coin table 0x2A3970)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x2A3970`

## 0x00241F68 UpdateNearbyCarEffect

**Description:** Tracks nearby cars against a selected position and updates a proximity-driven visual/audio effect. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x0021D358`, `0x00258E50`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x002490A0 UpdateRaceHud

**Description:** Updates and dispatches HUD rendering functions for each player vehicle via jump table PTR_LAB_002a8fb0 [inferred]

**Callers:** `0020f950`, `00210430`  
**Callees:** `0024e1a0`, `0027e840`
**Key globals / structures:** `PTR_LAB_002a8fb0`

## 0x00249458 CalculateSpeedometerValue

**Description:** Computes vehicle speedometer display value from velocity vector, clamping to 186 max and scaling by input range [inferred]

**Callers:** —  
**Callees:** `0021d358`
**Key globals / structures:** None identified in the inventory note.

## 0x002494E8 CalculateSpeedometerDigits

**Description:** Calculates vehicle speed from velocity vector and converts the integer speed to BCD digit format [inferred]

**Callers:** `0024e1a0`  
**Callees:** `0021d358`, `0024f4a0`
**Key globals / structures:** None identified in the inventory note.

## 0x00249520 CalculateTachometerValue

**Description:** Computes tachometer RPM display gauge value from vehicle engine RPM, clamping to 10000 and scaling by param_2 [inferred]

**Callers:** `0024e1a0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00249568 CalculateBoostGaugeValue

**Description:** Calculates boost/gauge bar value from vehicle meter field at 0x23c, clamping to 0x40000 and scaling [inferred]

**Callers:** `0024e1a0`  
**Callees:** —
**Key globals / structures:** `0x40000`

## 0x002495B8 GetVehicleGear

**Description:** Returns the 1-byte current transmission gear or state at vehicle offset 0x1ff [inferred]

**Callers:** `0024e1a0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002495C0 GetVehicleTransmissionMode

**Description:** Maps the transmission/shift mode byte at vehicle offset 0x244 (-1, 1, or other) to 1, 2, or 0 [inferred]

**Callers:** `0024e1a0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002495E8 GetVehicleRaceTime

**Description:** Returns the 32-bit time/counter field at vehicle structure offset 0x14 [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00249670 GetRaceTimeRank

**Description:** Evaluates race elapsed time at offset 0x14 against duration thresholds and returns a rank index [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024E1A0 UpdateCarControlState

**Description:** Large per-car update using gs_controls_car_object; calls physics/collision helpers and memsets temporaries [inferred].

**Callers:** `002490a0`  
**Callees:** `002494e8`, `00249520`, `00249568`, `002495b8`, `002495c0`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `0027e840`
**Key globals / structures:** `gs_controls_car_object`

## 0x0024FCD8 CarAiInit

**Description:** Initializes car AI state, allocating 0x100-byte AI buffer and binding course waypoint path tables [inferred]

**Callers:** `00218458`  
**Callees:** `0025fbc0`, `0027e840`, `0027ea30`
**Key globals / structures:** None identified in the inventory note.

## 0x0024FED8 ComputeWaypointTargetAngle

**Description:** Computes heading angles from vehicle position to waypoint segment endpoints via atan2 and packs results [inferred]

**Callers:** `002508e0`  
**Callees:** `00274af8`
**Key globals / structures:** None identified in the inventory note.

## 0x00250068 UpdateVehicleWaypointProgress

**Description:** Tracks vehicle position against course waypoint segments, updating current waypoint index and lateral error [inferred]

**Callers:** `0021a990`  
**Callees:** `00203a50`, `00274938`, `00274a10`, `00274af8`, `0027eee0`
**Key globals / structures:** None identified in the inventory note.

## 0x00250738 DrawWaypointDebugGrid

**Description:** Renders on-screen debug text grid for nearby AI waypoint segments and node attributes [inferred]

**Callers:** `0026d020`  
**Callees:** `00203a50`, `0027eee0`
**Key globals / structures:** None identified in the inventory note.

## 0x002508E0 CalculateAiSteeringAndThrottle

**Description:** Calculates AI steering angle and throttle response based on target waypoint heading and current vehicle speed [inferred]

**Callers:** `0020e708`  
**Callees:** `0024fed8`
**Key globals / structures:** None identified in the inventory note.

## 0x00250DD0 CopyAiRouteTables

**Description:** Copies 16 default AI route/path tables (0x100 bytes each) from DAT_002a9b80 into destination buffer [inferred]

**Callers:** `002277b0`, `0023da58`  
**Callees:** —
**Key globals / structures:** `DAT_002a9b80`

## 0x00250F00 InitMiniGameBallState

**Description:** Initializes mini-game ball entity position, camera, and velocity physics state for the selected hole/stage [inferred]

**Callers:** `00251b60`  
**Callees:** `0021c928`, `0024fb80`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00250FE0 CalculateBallLaunchImpulse

**Description:** Calculates mini-game ball hit impulse vector based on player aim and power meter [inferred]

**Callers:** `00251b60`  
**Callees:** `002727c0`, `002727e8`, `00272958`, `00272988`, `00272a38`, `00274b10`, `002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x00251190 ApplyCupAttraction

**Description:** Checks mini-game ball proximity to hole and applies attraction/sink velocity when near the cup [inferred]

**Callers:** `00251b60`  
**Callees:** `002727c0`, `002727e8`, `00272958`, `00272988`, `00274938`, `00274a10`, `00274b10`, `002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x00251440 UpdateBallPhysics

**Description:** Simulates mini-game ball physics: terrain collision, bounce reflection, friction, rotation, and stop check [inferred]

**Callers:** `00251b60`  
**Callees:** `00258e50`, `002727c0`, `002727e8`, `00272940`, `00272958`, `00272988`, `00272ce0`, `002732b8`, `00274b10`, `00272728`, `002729d0`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x00251B60 UpdateGolfMiniGame

**Description:** Main task state machine for the 9-hole golf mini-game, managing stages, player shot input, and scorecard [inferred]

**Callers:** —  
**Callees:** `00207f88`, `00208008`, `002080f8`, `0020df50`, `00233338`, `00233860`, `00233ef0`, `00234e98`, `00234f20`, `00234f38`, `00234f50`, `00234f68`, `00242d20`, `00243398`, `0024fb38`, `00250f00`, `00250fe0`, `00251190`, `00251440`, `00252608`, `00252738`, `00253218`, `00258e50`, `00258f30`, `00258f50`, `00258fb0`, `002727c0`, `002727e8`, `00272958`, `00273268`, `00274b10`, `00242a88`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`, `002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x00252540 NormalizeQuatAndRotate

**Description:** VU0 quaternion helper: normalizes param (default identity), computes projection and rotation [inferred].

**Callers:** —  
**Callees:** `002727c0`, `002727e8`, `00272958`, `002732b8`
**Key globals / structures:** None identified in the inventory note.

## 0x00252608 InitGolfCourseTable

**Description:** Initializes 9-hole golf course pars, distances, high scores, and clears player scorecard array [inferred]

**Callers:** `00251b60`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00252738 RecordGolfHoleScore

**Description:** Records stroke count for current golf hole against par, updates total score, and saves course records [inferred]

**Callers:** `00251b60`  
**Callees:** `0023d8c8`
**Key globals / structures:** None identified in the inventory note.

## 0x00252880 DrawGolfHoleIntro

**Description:** Displays golf hole preview banner showing hole number, par, and yardage before play begins [inferred]

**Callers:** `00251b60`  
**Callees:** `00202350`, `0024f530`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `0024f928`, `00258e50`, `0027eee0`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00252CB0 DrawGolfScorecard

**Description:** Renders 9-hole golf scorecard table displaying hole pars, player scores, and relative totals [inferred]

**Callers:** `00251b60`  
**Callees:** `00202350`, `00203f40`, `0024f530`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `0024f928`, `00258e50`, `0027eee0`, `00204de8`, `0027f2f4`
**Key globals / structures:** None identified in the inventory note.

## 0x00253218 SetGolfCurrentHole

**Description:** Stores the active golf hole index at offset 0x8008 in the golf state structure [inferred]

**Callers:** `00251b60`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00253230 SetAiSteeringTarget

**Description:** Computes and sets AI vehicle steering input flags based on angle delta to target and speed limit [inferred]

**Callers:** `00253448`, `00253680`  
**Callees:** `0021d358`
**Key globals / structures:** None identified in the inventory note.

## 0x00253448 SteerAiToDefaultWaypoint

**Description:** Steers AI vehicle toward default waypoint target coordinates when navigating course sections [inferred]

**Callers:** `00253680`  
**Callees:** `00253230`, `002727e8`, `00272940`, `00272958`, `00272970`, `00272988`, `00274af8`, `00274b10`
**Key globals / structures:** None identified in the inventory note.

## 0x00253680 UpdateAiCourseSteering

**Description:** Directs AI vehicle navigation across course waypoints, choosing between target node and default path [inferred]

**Callers:** `00254cc8`  
**Callees:** `00253230`, `00253448`, `00272958`, `00272970`, `00274af8`, `00274b10`
**Key globals / structures:** None identified in the inventory note.

## 0x00253B80 HandleCarObjectCollisions

**Description:** Checks collision between object and all player/NPC cars, applying bounce impulse and triggering rumble [inferred]

**Callers:** `00254430`  
**Callees:** `0021d358`, `002727e8`, `00272940`, `00272958`, `00272970`, `00272988`, `00272a38`, `00274b10`, `0020acb8`
**Key globals / structures:** None identified in the inventory note.

## 0x00253E90 ApplyArenaWallCollisions

**Description:** Tests object position against rectangular arena boundary walls, reflecting velocity with bounce damping [inferred]

**Callers:** `00254430`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00254290 RotateObjectByWrappedYaw

**Description:** Normalizes an angle float into a [min,max] range, builds a vu0 rotation matrix, and applies it to an object transform and its child, normalizing both. [inferred]

**Callers:** `0x00254430`  
**Callees:** `0x002727e8`, `0x00272ce0`, `0x0027e840`, `0x00272728`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x00254430 TuningGarageSceneTask

**Description:** Large task state machine for a 2-player garage/tuning scene: allocates 0x8d70 state, plays bgm, spawns 6 car objects, runs timed stages (menus, engine sounds, camera fade) then kills tasks. [inferred]

**Callers:** —  
**Callees:** `0x00207f88`, `0x002080f8`, `0x0020df50`, `0x0021c928`, `0x00232d68`, `0x00232f08`, `0x00233860`, `0x00233ef0`, `0x00235098`, `0x002350f0`, `0x002351a0`, `0x00235238`, `0x0023d8c8`, `0x00242d20`, `0x00243398`, `0x0024fb80`, `0x00253b80`, `0x00253e90`, `0x00254290`, `0x00258e50`, `0x00258f30`, `0x00258f50`, `0x00258fb0`, `0x00272940`, `0x00273268`, `0x002427d0`, `0x00258ec8`, `0x00204de8`, `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00259A08 UpdateNpcTailCarTransform

**Description:** If not paused, computes distance between an NPC object (param_1+0x1c ptr) and car position 0x177ace0; under 1.5 it snaps flags and rebuilds the NPC's transform/matrix from car velocity and position sectors. [inferred]

**Callers:** `0x0025a120`  
**Callees:** `0x00207ec8`, `0x0021d358`, `0x002727c0`, `0x00272958`, `0x00272ae8`, `0x00272b90`, `0x00272c38`, `0x00274b10`, `0x002729d0`, `0x00272a48`
**Key globals / structures:** `0x177ace0`

## 0x0026C588 creditsScene

**Description:** Maintained symbol-map entry: guess: ending credits packet (vifDirect 0x1772670, 0x9D qw)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x1772670`

## 0x00272728 vu0ApplyMatrix

**Description:** Maintained symbol-map entry: libvu0 (by shape)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00272758 vu0MulMatrix

**Description:** Maintained symbol-map entry: libvu0 (by shape)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002729D0 vu0CopyVector

**Description:** Maintained symbol-map entry: libvu0 (by shape)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002729E0 vu0CopyMatrix

**Description:** Maintained symbol-map entry: libvu0 (by shape)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00272A48 vu0UnitMatrix

**Description:** Maintained symbol-map entry: libvu0 (by shape)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

# Collision, Terrain & Spatial Tests

Vehicle/object contact, polygon and arena tests, terrain response, and spatial proximity helpers.

## 0x00217830 HandleCarCollisionResponse

**Description:** Triggers collision rumble, collision sound effect, and impact recoil or interaction triggers between two cars.

**Callers:** `0x002179D8`  
**Callees:** `0x0021C998`, `0x0021D310`, `0x00258E50`, `0x0020ACB8`
**Key globals / structures:** None identified in the inventory note.

## 0x00219660 ApplyCollisionImpulse

**Description:** Maps collision code 0-15 to velocity/yaw impulses in car frame and repositions; code 15 = crash: prints, sets flag, rumble [inferred]

**Callers:** `0x0021BA70`  
**Callees:** `0x0021D2D8`, `0x00258E50`, `0x0020ACB8`, `0x0027EA30`
**Key globals / structures:** None identified in the inventory note.

## 0x0022D7B8 RankCarsByProgress

**Description:** Insertion-sorts the 24 car objects by lap progress/zone/position and assigns incrementing rank numbers at +0x247.

**Callers:** `0x0022ddd0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022EB78 UpdateZoneCheckFrame

**Description:** Runs zone test 0x22d620 on all 24 cars, tallies results into param_1+0x25, and on finish switches handler to 0x22ee00. [inferred]

**Callers:** `0x0022ecb8`  
**Callees:** `0x0022d620`, `0x00233860`, `0x00258fb0`
**Key globals / structures:** `0x22d620`, `0x22ee00`

## 0x00240BE0 IsPointInsideQuad

**Description:** Returns whether the position at offset 0x90 lies inside a clockwise four-point 2D polygon. [inferred]

**Callers:** `0x00240CD8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00259498 CheckCarArrivedAtZone

**Description:** Checks whether race car index param_2 has arrived (position thresholds DAT_003330ac-0xb4, speed 117/83, parked flags at 0x177ad40); sets route bit 0x800 and per-player flag bytes on param_1. [inferred]

**Callers:** `0x002595f0`  
**Callees:** —
**Key globals / structures:** `DAT_003330ac`, `0x177ad40`

# Chase Cameras & View Transitions

Camera block construction, follow/selection logic, transforms, and scripted view movement.

## 0x00207EC8 TransformRectangle

**Description:** Builds rectangle bounds and applies a matrix/vector transform.

**Callers:** `0x0021f340`, `0x00259a08`, `0x0025d7c8`, `0x0026abd0`  
**Callees:** `0x00272988`, `0x00272a28`
**Key globals / structures:** None identified in the inventory note.

## 0x00207F88 BuildTransformMatrix

**Description:** Constructs a transform matrix from basis vectors using VU matrix operations.

**Callers:** `0x00223b30`, `0x00251b60`, `0x00254430`, `0x002609d0`  
**Callees:** `0x002727a0`, `0x002727e8`, `0x002729d0`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x00208008 BuildYawTransform

**Description:** Builds a matrix from a direction vector and copies a translation vector.

**Callers:** `0x0021b3d0`, `0x0021ba70`, `0x0021dc18`, `0x00251b60`  
**Callees:** `0x002727a0`, `0x002727e8`, `0x002729d0`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x00213A78 HandleToggleMenuInput

**Description:** Manages menu transition states and toggles status bit 0x80 on selection or cancellation [inferred].

**Callers:** `0x002161E0`  
**Callees:** `0x00258E50`
**Key globals / structures:** None identified in the inventory note.

## 0x0021D2D8 TransformVecByF15Matrix

**Description:** VU0: transforms f15 fixed-point vector by 4x4 f15 matrix rows 0-2 (no translation), writes fixed-point result [inferred]

**Callers:** `0x00219660`, `0x0021BA70`, `0x0021A310`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021DC18 ComputeChaseCameraBasis

**Description:** Smoothed chase-cam offset from car yaw and cam pitch with per-slot spring limits; outputs 2 normalized basis vectors [inferred]

**Callers:** `0x0021E690`, `0x0021EC58`  
**Callees:** `0x00208008`, `0x00208080`, `0x002727E8`, `0x00274938`, `0x00274A10`, `0x002729D0`
**Key globals / structures:** None identified in the inventory note.

## 0x0021F098 BuildMenuSceneCamera

**Description:** For scene 0x16 eases spin angle DAT_0029ccd0 and zoom from state then builds fixed menu cam; else uses stored matrix ptrs [inferred]

**Callers:** `0x0020EBE8`  
**Callees:** `0x0021F698`, `0x002729D0`, `0x00272A48`
**Key globals / structures:** `DAT_0029ccd0`

## 0x0021F340 ComposeCameraMatrix

**Description:** Builds 4x4 cam matrix from euler angles at src+0x14, premultiplies optional matrices, writes result plus 4 vectors [inferred]

**Callers:** `0x0021F698`  
**Callees:** `0x00207EC8`, `0x00272B90`, `0x00272C38`, `0x00272728`, `0x002729D0`, `0x00272758`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x0021F450 BuildCameraViewMatrices

**Description:** Builds camera basis from forward/up vectors and emits frustum/projection matrices with X axis mirrored into output matrices [inferred]

**Callers:** `0x0021F698`  
**Callees:** `0x002727A0`, `0x002727E8`, `0x00272868`, `0x00272728`, `0x002729D0`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x0021F910 SetViewDistanceGlobals

**Description:** Writes two float view-distance globals (DAT_003356e0/e4) set from camera modes and the options draw-distance page [inferred]

**Callers:** `0x00208848`, `0x0021E690`, `0x0022AEB0`, `0x0022BFD0`, `0x0022C280`, `0x0022CD38`, `0x00244988`, `0x00246250`, `0x00216738`  
**Callees:** —
**Key globals / structures:** `DAT_003356e0`

## 0x002236F8 DrawCarShowroomRig

**Description:** Gets showroom view (FUN_00226180), centres rig at (0,-0.5,0) via FUN_00273038, then drawCarShowroom — the curated showroom pose. [inferred]

**Callers:** `0x0022aeb0`, `0x0022cc38`  
**Callees:** `0x00226180`, `0x00273038`, `0x00221f70`
**Key globals / structures:** None identified in the inventory note.

## 0x00223760 SelectCarAttachmentPoint

**Description:** Selects an attachment-point xy (from arg's wheel/bumper fields) by case on arg3, then vu0ApplyMatrix through view matrix param. [inferred]

**Callers:** `0x00244f60`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226180 GetSceneCamLookatIndex

**Description:** Returns the active CAM lookat points table: 1=>&DAT_0029e950, else default &DAT_0029e9b0 — pointer getter for the cars/props rig. [inferred]

**Callers:** `0x002236f8`, `0x00222f08`  
**Callees:** —
**Key globals / structures:** `DAT_0029e950`, `DAT_0029e9b0`

## 0x00226298 TransformMatrixAndSubmit

**Description:** Multiplies global matrix at 0x177f5e0 by input matrix, then submits via FUN_0021d3b8.

**Callers:** —  
**Callees:** `0021d3b8`, `00272758`
**Key globals / structures:** `0x177f5e0`

## 0x0022A408 ApplyMotionClipFrame

**Description:** Steps animation clip param_3 by a time delta, accumulating rotation deltas and rebuilding the transform matrix; 0 ends the clip.

**Callers:** `0x0022a6b8`  
**Callees:** `0x00207f38`, `0x00272940`, `0x00272c38`, `0x00272728`, `0x002729d0`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0022C280 RenderShowcaseView

**Description:** Initializes object matrices, camera and light, uploads via VIF and renders the model with mode param_3.

**Callers:** `0x00248580`  
**Callees:** `0x0021f910`, `0x00223200`, `0x002236d8`, `0x00225790`, `0x00272c38`, `0x00273268`, `0x0021f698`, `0x002261a0`, `0x002266e8`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x002340C0 SetStageConfig

**Description:** Stores stage-local state values and per-slot transform metadata [inferred]

**Callers:** `0x0022d8c8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00238B18 SelectAndBuildCamera

**Description:** Picks nearest camera block for the current car and builds the camera with follow/zoom flags; type -1 delegates to FUN_0021ed90. [inferred]

**Callers:** `0x0021e328`, `0x0021e4d8`, `0x0021e690`, `0x0021ee98`, `0x0021ef98`  
**Callees:** `0x0021ed90`, `0x00274b10`, `0x0021f698`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0023F0A0 UpdateRotatingEffect

**Description:** Updates effect entities and two opposing rotation transforms based on a periodic input state. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x0023D778`, `0x00272C38`, `0x002729D0`, `0x00272A48`
**Key globals / structures:** None identified in the inventory note.

## 0x00240228 UpdateCameraTransition

**Description:** Initializes and advances a multi-state camera transition with bounded horizontal and vertical motion. [inferred]

**Callers:** —  
**Callees:** `0x002406E8`
**Key globals / structures:** None identified in the inventory note.

## 0x0024FC30 LoadAndTransformVec4

**Description:** Loads 4 shorts into a stack vector, transforms via FUN_00272a28 and FUN_00272988 with global matrix.

**Callers:** —  
**Callees:** `00272988`, `00272a28`
**Key globals / structures:** None identified in the inventory note.

## 0x0024FC98 ScaleShortByMatrix

**Description:** Loads one short into a vector, transforms via FUN_00272a28 and FUN_00272988, returns first component.

**Callers:** —  
**Callees:** `00272988`, `00272a28`
**Key globals / structures:** None identified in the inventory note.

# Sound RPC (SNDMOD) & Music

SIF RPC transport and command wrappers for SNDMOD, BGM, voices, UI sounds, and volume control.

## 0x002137B0 HandleAudioSettingsMenuInput

**Description:** Handles audio options menu input, updating BGM and sound effect volume levels and settings [inferred].

**Callers:** `0x002156D0`  
**Callees:** `0x00206D28`, `0x002589A0`, `0x00258A48`, `0x00258AF0`, `0x00258B30`, `0x00258E50`, `0x002590B0`
**Key globals / structures:** None identified in the inventory note.

## 0x00217F10 UpdateContinuousSoundEffect

**Description:** Manages start, loop, and stop sound playback for continuous audio effects (horn or boost) with countdown timer [inferred].

**Callers:** `0x0021BA70`  
**Callees:** `0x00258E50`
**Key globals / structures:** None identified in the inventory note.

## 0x0022C630 PlayEventSoundStub

**Description:** One-line handler that plays sound id 0x8035 via FUN_00258e50.

**Callers:** `0x0022c518`  
**Callees:** `0x00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x00241BA8 UpdateMapCoinCollection

**Description:** Checks nearby map coins, plays their collection sound, clears collected bits, and queues a special byte. [inferred]

**Callers:** `0x00210430`  
**Callees:** `0x002080F8`, `0x0023D968`, `0x0023DBF0`, `0x0023DF38`, `0x00258E50`, `0x002727C0`, `0x00272958`, `0x00274B10`
**Key globals / structures:** None identified in the inventory note.

## 0x0024FB38 SetGamePausedState

**Description:** Toggles pause flag 0x2000 at param_1+0x28 and calls sound pause (00258ff0) or resume (00259050) [inferred]

**Callers:** `00251b60`, `0025a120`, `00260dc0`  
**Callees:** `00258ff0`, `00259050`
**Key globals / structures:** None identified in the inventory note.

## 0x00256AF8 InitSoundRpcDriver

**Description:** Boot-time sound init: binds RPC to IOP server 0x1234567, sends port/config calls (cmds 0,5,4), sets DGA/DMA2 registers, and registers bgm/SE file names via RPC cmds 6/7/0x32. [inferred]

**Callers:** `0x002003b8`  
**Callees:** `0x002575e0`, `0x00279290`, `0x002793d0`, `0x0027e840`, `0x00287cd8`, `0x00287ea8`, `0x0027f1e0`
**Key globals / structures:** `0x1234567`

## 0x002574A0 SoundRpcPlaySequence

**Description:** Sends RPC cmd 0x23 with a parameter then cmd 0x22 with a single entry to the sound driver, likely loading/starting a sequence. [inferred]

**Callers:** `0x00218458`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x002575E0 SoundRpcRegisterFiles

**Description:** Registers two file names per call via RPC cmd 0x32 at slot index*2 and *2+1, copying strings with sizes into the driver request buffer. [inferred]

**Callers:** `0x00256af8`  
**Callees:** `0x00287ea8`, `0x0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x00257760 BindAndCallRpcInit

**Description:** Binds SIF RPC 0x175e840 with key 0x1234567 and issues initial RPC call.

**Callers:** —  
**Callees:** `00287cd8`, `00287ea8`
**Key globals / structures:** `0x175e840`, `0x1234567`

## 0x002577C0 CallRpcCmd1

**Description:** Issues SIF RPC command 1 on service 0x175e840.

**Callers:** —  
**Callees:** `00287ea8`
**Key globals / structures:** `0x175e840`

## 0x00257800 SendDialogueWindowRpc4

**Description:** Packs dialogue window/cursor addresses into RPC buffer and sends RPC command 4.

**Callers:** —  
**Callees:** `00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x002578A8 SendDialogueWindowRpc5

**Description:** Packs dialogue window/cursor addresses into RPC buffer and sends RPC command 5.

**Callers:** —  
**Callees:** `00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00257950 SendRpcWithStringArg

**Description:** Clears 0x40-byte RPC buffer, copies string argument via strcpy, sends RPC with dialogue state.

**Callers:** —  
**Callees:** `0027e840`, `00287ea8`, `0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x00257A28 SendMusicPlayRpc

**Description:** Same packing as SendRpcWithStringArg; called from gs_music_play_bgm to send BGM play RPC [inferred].

**Callers:** `00258ec8`  
**Callees:** `0027e840`, `00287ea8`, `0027f1e0`
**Key globals / structures:** `gs_music_play_bgm`

## 0x00257BA8 SoundRpcGetStatus

**Description:** Sends RPC cmd 8 with arg 1 to the sound driver and returns the 32-bit value returned in DAT_0175f880 (status/result query). [inferred]

**Callers:** `0x00208f50`  
**Callees:** `0x00287ea8`
**Key globals / structures:** `DAT_0175f880`

## 0x00257C68 SendRpcCmd9Value

**Description:** Packs 12-bit value and pointer into RPC buffer, sends RPC command 9.

**Callers:** `00259070`  
**Callees:** `00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00257D18 SendRpcCmd9Pair

**Description:** Packs two 32-bit args into RPC buffer, sends RPC command 9.

**Callers:** —  
**Callees:** `00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00257DD0 SndRpcCmdA

**Description:** Stages 0x40-byte request from args (incl. window/cursor globals) and sends sound-engine RPC cmd 0xA; no reply. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00257E80 SndRpcCmdB

**Description:** Stages 0x40-byte request from caller-supplied stack tail and sends sound-engine RPC cmd 0xB; no reply. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00257F30 MusicRpcPlayBgm

**Description:** Sends RPC cmd 0xe with two params to the sound driver; shared backend for all BGM-slot wrappers. [inferred]

**Callers:** `0x00258f30`, `0x00258f50`, `0x00258f70`, `0x00258f90`, `0x00258fb0`, `0x00258fd0`, `0x00258ec8`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00257FD8 MusicRpcCmd15

**Description:** Sends RPC cmd 0xf with one param to the sound driver; lowest-level stop/reset wrapper used by BGM control. [inferred]

**Callers:** `0x00258eb0`, `0x00258ff0`, `0x00258ec8`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258080 SndRpcQueryCmd10

**Description:** Fills 0x40 send buffer from arg tail, sends RPC cmd 0x10 with 0x40 readback at 0x175f880; returns first dword. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** `0x175f880`

## 0x00258138 SndRpcReadBlock1F

**Description:** RPC cmd 0x1F reads 0x40 bytes into 0x175f880 then copies all 64 bytes to caller's out struct. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** `0x175f880`

## 0x00258218 SndRpcCmdCArgs

**Description:** Builds 0x40 request (dword0=0x3F1 plus 9 args from tail) and sends sound-engine RPC cmd 0xC; no reply. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258308 SndRpcPlayOrStop

**Description:** Arg 0 (& 0x8000FFFF mask) -> RPC cmd 1 (stop all); nonzero arg staged as 0x40 request for RPC cmd 0xD (play). [inferred]

**Callers:** `0x00258e50`  
**Callees:** `0x00287ea8`
**Key globals / structures:** `0x8000FFFF`

## 0x002583F0 SetVolumeDgaRegs

**Description:** Writes two DGA2 registers (0x981, 0xa81) with the given volume values via FUN_002793d0; used by the options volume screen for BGM and SE. [inferred]

**Callers:** `0x002090c0`  
**Callees:** `0x002793d0`
**Key globals / structures:** None identified in the inventory note.

## 0x002584B0 GsSetupFrameContext

**Description:** Emit 3 packet items via FUN_002793D0 (codes 0x8050/0x8130/0x8070, ctx=arg&1); likely GS framebuffer/scissor setup. [inferred]

**Callers:** —  
**Callees:** `0x002793d0`
**Key globals / structures:** None identified in the inventory note.

## 0x00258578 SoundInitVolumes

**Description:** Stop/reset sound then sets both channels 0 and 1 to value 0x1ccc via FUN_002585f0 (RPC 0x21); called during system init. [inferred]

**Callers:** `0x00205088`, `0x002052d0`  
**Callees:** `0x002585b0`, `0x002585f0`
**Key globals / structures:** None identified in the inventory note.

## 0x002585B0 SoundRpcStopAll

**Description:** Sends a single RPC cmd 0x20 to the sound server (stop-all/flush voices). [inferred]

**Callers:** `0x00258578`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x002585F0 SoundRpcSetVolume

**Description:** Sends RPC cmd 0x21 with three params (id, value, value) to the sound server, setting a channel/voice volume pair. [inferred]

**Callers:** `0x00258578`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x002586A0 SndRpcCmd22

**Description:** RPC cmd 0x22 with 0x40 request; count arg forced to 1 when zero; called from scene code. [inferred]

**Callers:** `0x00217c68`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258768 SndRpcCmd24

**Description:** Stages 0x40 request whose first dword is the arg; sends sound-engine RPC cmd 0x24. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258810 SoundRpcCmd25

**Description:** Sends RPC cmd 0x25 with one param to the sound server; start/open voice command used when clearing sound state. [inferred]

**Callers:** `0x00258ff0`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x002588B8 SoundRpcCmd26

**Description:** Sends RPC cmd 0x26 with one param to the sound server; used in pairs for channels 0 and 1 (likely fadeout/stop per channel). [inferred]

**Callers:** `0x00259050`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258960 SndRpcCmd30

**Description:** Fire-and-forget sound-engine RPC cmd 0x30, no payload. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x002589A0 SoundRpcCmd31

**Description:** Sends RPC cmd 0x31 with two equal params to the sound server; used to push a master volume value derived from global amp. [inferred]

**Callers:** `0x002137b0`, `0x002590b0`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258A48 SoundRpcCmd33PlaySe

**Description:** Sends RPC cmd 0x33 with two params to the sound server; plays a sound by index (first param) given a volume/voice param. [inferred]

**Callers:** `0x002137b0`, `0x002590b0`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258AF0 SoundRpcCmd34

**Description:** Sends a parameterless RPC cmd 0x34 to the sound server (commit/trigger after Cmd33). [inferred]

**Callers:** `0x002137b0`, `0x002590b0`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258B30 SoundRpcCmd35

**Description:** Sends a parameterless RPC cmd 0x35 to the sound server (sound-system reset/stop). [inferred]

**Callers:** `0x002105b8`, `0x002137b0`  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258B70 SndRpcCmd36

**Description:** Fire-and-forget sound-engine RPC cmd 0x36, no payload. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258BB0 SndRpcCmd37

**Description:** Fire-and-forget sound-engine RPC cmd 0x37, no payload. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258BF0 SndRpcCmdCStruct

**Description:** Copies 56-byte struct (7 qwords incl window/cursor fields) to request buffer and sends sound-engine RPC cmd 0xC. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258CA8 SndRpcCmdD

**Description:** Stages 0x40 request (first dword = arg) and sends RPC cmd 0xD; same opcode as play path of 0x00258308. [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** `0x00258308`

## 0x00258D50 SndRpcReadStatBlock

**Description:** RPC cmd 0xFFF0 reads 0x300 bytes to 0x175f880, copies 96 qwords to caller buffer (8-aligned fast path). [inferred]

**Callers:** —  
**Callees:** `0x00287ea8`
**Key globals / structures:** `0x175f880`

## 0x00258E50 PlayUiSound

**Description:** Plays a UI sound effect: calls FUN_00258308 with the given id OR 0x40400000 (float-encoded flag); ubiquitous beep/click wrapper with 125+ callers.

**Callers:** `0x00206d28`, `0x002083c0`, `0x00208428`, `0x00208848`, `0x00209520`, `0x00209848`, `0x00209cb0`, `0x0020a2b0`, `0x0020f3b8`, `0x002105b8`, `0x00212ac8`, `0x002135c8`, `0x002137b0`, `0x00213a78`, `0x00214470`, `0x00216e00`, `0x00217830`, `0x00217c68`, `0x00217f10`, `0x00218458`, `0x00219660`, `0x0021a990`, `0x0021b3d0`, `0x0021ba70`, `0x0021c998`, `0x0021ca90`, `0x00227380`, `0x00228708`, `0x00228b10`, `0x00228d10`, `0x002291c8`, `0x00229538`, `0x0022a6b8`, `0x0022aba0`, `0x0022aeb0`, `0x0022c630`, `0x0022c8a0`, `0x0022ca28`, `0x0022cd38`, `0x0022d390`, `0x0022dae8`, `0x00233a80`, `0x00236858`, `0x00236d28`, `0x00237080`, `0x00239260`, `0x00239378`, `0x00239508`, `0x00239748`, `0x00239900`  
**Callees:** `0x00258308`
**Key globals / structures:** `0x40400000`

## 0x00258EB0 BgmStop1

**Description:** Trivial wrapper calling FUN_00257fd8(1) (RPC cmd 0xf), likely stopping/asserting BGM channel 1. [inferred]

**Callers:** `0x00262678`  
**Callees:** `0x00257fd8`
**Key globals / structures:** None identified in the inventory note.

## 0x00258F30 PlayBgmSlot1

**Description:** Wrapper calling FUN_00257f30(1,0) — plays BGM slot 1 via RPC cmd 0xe (used at scene starts). [inferred]

**Callers:** `0x0020a150`, `0x0022aeb0`, `0x0022cd38`, `0x0022dae8`, `0x0022ecb8`, `0x00233a80`, `0x00251b60`, `0x00254430`, `0x00260dc0`, `0x002631a0`, `0x0026cb60`, `0x0026d2c0`, `0x002455c0`  
**Callees:** `0x00257f30`
**Key globals / structures:** None identified in the inventory note.

## 0x00258F50 PlayBgmSlot2

**Description:** Wrapper calling FUN_00257f30(2,0) — plays BGM slot 2. [inferred]

**Callers:** `0x0020a2b0`, `0x0022c418`, `0x00251b60`, `0x00254430`  
**Callees:** `0x00257f30`
**Key globals / structures:** None identified in the inventory note.

## 0x00258F70 PlayBgmSlot3

**Description:** Wrapper calling FUN_00257f30(3,0) — plays BGM slot 3. [inferred]

**Callers:** `0x0020f3b8`, `0x00239378`, `0x00239f68`, `0x0023a140`, `0x0026ccd0`  
**Callees:** `0x00257f30`
**Key globals / structures:** None identified in the inventory note.

## 0x00258F90 PlayBgmSlot4

**Description:** Wrapper calling FUN_00257f30(4,0) — plays BGM slot 4. [inferred]

**Callers:** `0x0020f3b8`  
**Callees:** `0x00257f30`
**Key globals / structures:** None identified in the inventory note.

## 0x00258FB0 PlayBgmSlot5

**Description:** Wrapper calling FUN_00257f30(5,0) — plays BGM slot 5 (used at menu/scene transitions). [inferred]

**Callers:** `0x0020a150`, `0x0022aeb0`, `0x0022cd38`, `0x0022dc68`, `0x0022eb78`, `0x00251b60`, `0x00254430`, `0x00254d20`, `0x0025a120`, `0x0025eb70`, `0x00261ac8`, `0x002631a0`, `0x0026cb60`, `0x002455c0`  
**Callees:** `0x00257f30`
**Key globals / structures:** None identified in the inventory note.

## 0x00258FD0 PlayBgmSlot6

**Description:** Wrapper calling FUN_00257f30(6,0) — plays BGM slot 6. [inferred]

**Callers:** `0x00239378`, `0x00239f68`, `0x0023a140`  
**Callees:** `0x00257f30`
**Key globals / structures:** None identified in the inventory note.

## 0x00258FF0 SoundStopAllChannels

**Description:** Stops both voices via FUN_00258810(0/1), sends BGM cmd with param 1, then clears all two sound handle slots in DAT_00335728; standard sound teardown. [inferred]

**Callers:** `0x0020f3b8`, `0x002105b8`, `0x0022dc68`, `0x0024fb38`, `0x0025a120`, `0x0025dbb8`, `0x0025e278`, `0x0025e630`, `0x0025eb70`, `0x0025f6a0`, `0x0025fbf8`, `0x0025ff00`, `0x00260638`, `0x00262678`, `0x002631a0`, `0x00263710`, `0x002638c0`, `0x00263e68`, `0x0026ccd0`  
**Callees:** `0x00257fd8`, `0x00258810`
**Key globals / structures:** `DAT_00335728`

## 0x00259050 SoundFadeOutChannels

**Description:** Calls FUN_002588b8 for channels 0 and 1 (RPC cmd 0x26 pair) — fades out/stops all sound channels. [inferred]

**Callers:** `0x0020f3b8`, `0x002105b8`, `0x0024fb38`, `0x00262678`  
**Callees:** `0x002588b8`
**Key globals / structures:** None identified in the inventory note.

## 0x00259070 SoundStopVoiceByHandle

**Description:** Stops the voice whose handle is stored at (&DAT_00335728)[slot] by calling FUN_00257c68 with it, if nonzero. [inferred]

**Callers:** `0x00217c68`  
**Callees:** `0x00257c68`
**Key globals / structures:** `DAT_00335728`

## 0x002590B0 ApplySoundVolumes

**Description:** Computes scaled master volume from global amp DAT_01780a3e and pushes it via FUN_002589a0, then optionally plays a SE (DAT_01780a3d) followed by FUN_00258af0. [inferred]

**Callers:** `0x00210430`, `0x002105b8`, `0x002137b0`  
**Callees:** `0x002589a0`, `0x00258a48`, `0x00258af0`
**Key globals / structures:** `DAT_01780a3e`, `DAT_01780a3d`

# Loading, Filesystem & Texture Upload

Async CD reads, scene/model asset loaders, GS upload chains, and texture relocation/copies.

## 0x00207B50 sendTexturePackets

**Description:** Maintained symbol-map entry: sends a file's GS upload chain (CNT tags up to END)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00207D80 gifChainEndTag

**Description:** Maintained symbol-map entry: one END DMA tag with qwc, for sendTexturePackets

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00207D98 eeQuadBuild

**Description:** Maintained symbol-map entry: EE-side transformed quad (shop scene) into a packed ST/RGBAQ/XYZF2 strip

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002080F8 uiFade

**Description:** Maintained symbol-map entry: full-screen flat quad with an RGBA colour (alpha = strength); 51 callers

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00208120 frameBlend

**Description:** Maintained symbol-map entry: DIRECT packet (0x29A930 + variant * 0x90): the frame buffer over itself, FIX 0x20 or 0x40 (loading blur)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29A930 + variant`

## 0x00208270 fadeOutTask

**Description:** Maintained symbol-map entry: task: fade to black over 0x40 / 0x20 ticks

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002082D8 fadeInTask

**Description:** Maintained symbol-map entry: task: fade in

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00208358 flashTask

**Description:** Maintained symbol-map entry: task: white flash

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00208A88 titlePressStart

**Description:** Maintained symbol-map entry: title picture scene (PRESS START BUTTON)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00209520 titleNewGameContinue

**Description:** Maintained symbol-map entry: title menu scene (New Game / Continue)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020AD18 copyCarModel

**Description:** Maintained symbol-map entry: car slot a -> b: memmove of the model blob and a GS local-to-local copy (T8 128x128 + 16x16 CLUT) through the template 0x29AD90

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29AD90`

## 0x0020AEF0 loadCarPartsSet

**Description:** Maintained symbol-map entry: SYS entries 1..4: EFFECT.GSL (absolute 16169..), CARS/PARTS (TBP + 0x1BC2), TIRE (+ 0x1C44), WHEEL (models only)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020B348 loadPoseSet

**Description:** Maintained symbol-map entry: SYS entry 9 (POSE.GSL) at TBP + 0x1D8E; model blob to 0x3FA780 (likely)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x3FA780`

## 0x0020B5E8 loadShopSet

**Description:** Maintained symbol-map entry: SHOP/Tnn (table 0x29B29C) and SYS entry 11; rebases 0x1F2E, 0x1CC6, 0x285E, 0x34BE (shop interiors: T8 640x384)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29B29C`

## 0x0020BAC8 loadFieldTile

**Description:** Maintained symbol-map entry: state machine: reads a FLD/COURSE/ACTION file into 0x707B80 + slot * 0x3A8000

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x707B80 + slot`, `0x3A8000`

## 0x0020C188 loadCourseTile

**Description:** Maintained symbol-map entry: COURSE/ACTION file (table 0x29BE50), like loadFieldTile; each misc model's chain rebased to TBP 0x2B26 + i * 0x44

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29BE50`

## 0x0020C7B8 loadCarModel

**Description:** Maintained symbol-map entry: reads a CAR file into a slot; 6 model pointers to 0x177F310 + slot * 0x18

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x177F310 + slot`

## 0x0020C9D0 loadCarModelAt

**Description:** Maintained symbol-map entry: car file into 0x3FBB80 + n * 0x34000; textures rebased to TBP 0x243E + n * 0x44

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x3FBB80 + n`, `0x34000`

## 0x0020D120 loadStep

**Description:** Maintained symbol-map entry: the scene load state machine: request word kind = (w >> 16) & 0xF, index = w & 0xFF; state at 0x3356C0; picks the loader and the TBP rebase

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x3356C0`

## 0x0020DC50 loadRequestTask

**Description:** Maintained symbol-map entry: task: starts a load (busy 0x333370, request word 0x333374); sets drive flag 0x400000 (the loading blur) unless bit 0x100000

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x333370`, `0x333374`, `0x400000`, `0x100000`

## 0x0021D388 UNKNOWN_0021D388

**Description:** VU0 stub: loads 4 quads from matrix ptr and re-stores via vsqi at doubled VI0; semantics unclear, likely VU register sync [inferred]

**Callers:** `0x0022AEB0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021D3B8 UNKNOWN_0021D3B8

**Description:** VU0 stub: loads 3 matrix rows and stores via vsqi at address ctc2(4); returns 4; semantics unclear [inferred]

**Callers:** `0x00226298`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021D450 UNKNOWN_0021D450

**Description:** 12-byte stub: single lqc2 quad load to $0, result unused; likely prefetch/sync artifact [inferred]

**Callers:** `0x0022AEB0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00223238 DrawShowroomCarModel

**Description:** Uploads light matrix + object matrix + GIF regs for a car and vifCallModel into &carModels[idx*6]+0x10; per-model showroom drawer. [inferred]

**Callers:** `0x00216738`  
**Callees:** `0x00226868`, `0x00226b18`, `0x00226c28`, `0x00227048`
**Key globals / structures:** None identified in the inventory note.

## 0x002236A8 UploadCarAt16Env

**Description:** Loads At16 light env from 0x177f5e0 and starts a 4-cycle DMA (car env re-upload). [inferred]

**Callers:** `0x0022cc38`  
**Callees:** `0x002267a8`, `0x00226f70`
**Key globals / structures:** `0x177f5e0`

## 0x00226058 ScaleCarLightMatrix

**Description:** Per render: scales rows of the (param_2) light matrix by a fading param (clamped 0..1.25) and re-uploads the light matrix to VU0. [inferred]

**Callers:** `0x00221788`  
**Callees:** `0x00272970`, `0x00226868`
**Key globals / structures:** None identified in the inventory note.

## 0x0022EA78 UploadVifPacketA

**Description:** Stores param_1 into DAT_0029f540 and sends the 5-quad VIF direct packet at 0x29f510 via vifDirect. [inferred]

**Callers:** `0x0025eb70`  
**Callees:** `0x00226f70`
**Key globals / structures:** `DAT_0029f540`, `0x29f510`

## 0x0022EAA0 UploadVifPacketB

**Description:** Stores param_1 into DAT_0029f590 and sends the 5-quad VIF direct packet at 0x29f560 via vifDirect. [inferred]

**Callers:** `0x0025eb70`  
**Callees:** `0x00226f70`
**Key globals / structures:** `DAT_0029f590`, `0x29f560`

## 0x00232F08 SetWheelSprite2

**Description:** Initializes an alternate wheel sprite entry with mode-dependent size and asset pointers [inferred]

**Callers:** `0x00254430`, `0x002595f0`, `0x0025eb70`, `0x0025ff00`, `0x00260638`, `0x00260dc0`, `0x002638c0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00233338 SetShadowConfig

**Description:** Builds a shadow/mesh config from a frame index and stores asset and lifetime data [inferred]

**Callers:** `0x00251b60`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023F900 SubmitFiveTexturePacketsB

**Description:** Builds five packet entries, updates a float from the animation counter, and submits 21 packets. [inferred]

**Callers:** —  
**Callees:** `0x00207D80`, `0x00207B50`
**Key globals / structures:** None identified in the inventory note.

## 0x0026C460 RequestLocationAssetLoad

**Description:** Queues 22 gs_locations_table entries as load request (flag 0xb), polls status, spawns arrival handler 0x0026be50 on ready. [inferred]

**Callers:** `0x0026B848`  
**Callees:** `0x00205350`, `0x00205C20`, `0x00205DA0`, `0x00204DE8`, `0x00204C48`
**Key globals / structures:** `gs_locations_table`, `0x0026be50`

# Rendering, VIF/VU & GS Packet Helpers

VIF1/VU1 DMA chains, GS context emission, matrices, and low-level draw-packet submission.

## 0x002085B0 ReflectVectorVU0

**Description:** VU0 vector math: reflects input vector about normal (param3) with scale param2; uses FUN_00272* helpers [inferred].

**Callers:** —  
**Callees:** `002727c0`, `002727e8`, `002728d8`, `00272958`, `00272988`, `002732b8`, `00274b10`, `002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x002119A8 DrawMenuGridBorder

**Description:** Draws procedural grid borders, cell dividers, and frames for menu layouts using 2D primitive packets.

**Callers:** `0x00214AD0`, `0x00214EF8`, `0x00215220`, `0x002156D0`, `0x002161E0`, `0x00216738`  
**Callees:** `0x0024F640`, `0x0024F728`, `0x0024F7C8`
**Key globals / structures:** None identified in the inventory note.

## 0x0021D470 vu0TransformVerts

**Description:** Maintained symbol-map entry: VU0 macro code: project and clip vertices into GS packed form; guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021D718 gifBeginSpriteBatch

**Description:** Maintained symbol-map entry: 6 qw header: TEST_1 0x5001B, ALPHA_1 0x44, TEX1_1 0x60, TEX0_1 = arg, CLAMP_1 5

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x5001B`

## 0x0021D788 gifSetTexRegs

**Description:** Maintained symbol-map entry: 2 qw REGLIST TEX0_1 + CLAMP_1 (arena patches the second to TEX0_1)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021D7B0 gifSetTex0Tex2

**Description:** Maintained symbol-map entry: A+D pair TEX0_1 + TEX2_1

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021D7F8 BuildDmaPacketSlot3A

**Description:** Writes 3-quad DMA/GIF tag header from constants at DAT_0029cc00 and calls FUN_0021d470 (variant 3,0).

**Callers:** —  
**Callees:** `0021d470`
**Key globals / structures:** `DAT_0029cc00`

## 0x0021D850 BuildDmaPacketSlot4A

**Description:** Writes DMA tag header (type 4) from DAT_0029cc00 and calls FUN_0021d470 (variant 4,0).

**Callers:** —  
**Callees:** `0021d470`
**Key globals / structures:** `DAT_0029cc00`

## 0x0021D8A8 BuildDmaPacketSlot3B

**Description:** Writes DMA tag header (type 3) from DAT_0029cc10 and calls FUN_0021d470 (variant 3,0).

**Callers:** —  
**Callees:** `0021d470`
**Key globals / structures:** `DAT_0029cc10`

## 0x0021D900 BuildDmaPacketSlot4B

**Description:** Writes DMA tag header (type 4) from DAT_0029cc10 and calls FUN_0021d470 (variant 4,0).

**Callers:** —  
**Callees:** `0021d470`
**Key globals / structures:** `DAT_0029cc10`

## 0x0021D958 BuildDmaPacketSlot3C

**Description:** Writes DMA tag header (type 3) from DAT_0029cc20 and calls FUN_0021d470 (variant 3,1).

**Callers:** —  
**Callees:** `0021d470`
**Key globals / structures:** `DAT_0029cc20`

## 0x0021D9B0 eeQuadTransform

**Description:** Maintained symbol-map entry: transforms a quad with 21D470

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021DA08 BuildDmaPacketSlot3D

**Description:** Writes DMA tag header (type 3) from DAT_0029cc30 and calls FUN_0021d470 (variant 3,1).

**Callers:** —  
**Callees:** `0021d470`
**Key globals / structures:** `DAT_0029cc30`

## 0x0021DA60 BuildDmaPacketSlot4C

**Description:** Writes DMA tag header (type 4) from DAT_0029cc30 and calls FUN_0021d470 (variant 4,1).

**Callers:** —  
**Callees:** `0021d470`
**Key globals / structures:** `DAT_0029cc30`

## 0x0021DAB8 gifTexQuadTag

**Description:** Maintained symbol-map entry: PACKED ST/RGBAQ/XYZF2 strip tag, 4 loops (tag 0x29CC20)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29CC20`

## 0x0021DAE8 gifSetSpriteCount

**Description:** Maintained symbol-map entry: sprite data tag (0x29CBE0) with NLOOP = n * 5

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29CBE0`

## 0x0021DB20 gifQuadTag

**Description:** Maintained symbol-map entry: PACKED quad strip tag 0x29CBF0 (CLAMP_1, 4 x UV + XYZ2)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29CBF0`

## 0x0021DB48 gifAdPair

**Description:** Maintained symbol-map entry: one A+D write (tag 0x29CBC0): data, register

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29CBC0`

## 0x0021DBE0 gifFlatRectTag

**Description:** Maintained symbol-map entry: linestrip A+D tag 0x29CC60 (unused path)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29CC60`

## 0x0021F698 buildCamera

**Description:** Maintained symbol-map entry: fills a 0x190-byte camera block (matrices at +0, +0x40, +0x80, +0xC0, +0x100)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021FA38 cellVisible

**Description:** Maintained symbol-map entry: frustum test of a 200-unit cell centre (likely)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021FB38 drawFieldTiles

**Description:** Maintained symbol-map entry: 8x8 cells of 200 units per tile quadrant, culled by drawCellVisible

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00220030 drawFieldTilesExtra

**Description:** Maintained symbol-map entry: each tile's 65th chain (water/far layer, likely)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002207D0 DrawSceneArcRibbonFaded

**Description:** For drawSceneCloudHill/drawSceneField: maybe-draws an arc ribbon via drawArcRibbon then fades ribbon-color toward ~0 by distance bands (60<<band RGB). [inferred]

**Callers:** `0x00220938`, `0x00220b20`  
**Callees:** `0x002224f8`, `0x00226610`, `0x00226f70`
**Key globals / structures:** None identified in the inventory note.

## 0x00220938 drawSceneField

**Description:** Maintained symbol-map entry: towns and field areas: 4 FLD tile quadrants, cars, props, coins, sky

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00220B20 drawSceneCloudHill

**Description:** Maintained symbol-map entry: location 0x22 (Cloud Hill)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00220D40 drawSceneRace

**Description:** Maintained symbol-map entry: race courses (COURSE files, tile slot 2)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00220FE8 drawSceneAction

**Description:** Maintained symbol-map entry: mini-game maps: locations 0x12 maze, 0x1C tunnel race, 0x20 curling

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002212C8 SetVifFillDirect

**Description:** Stores replicated 8-bit pattern to _PTR_DAT_0029d130 and issues vifDirect with 6 quadwords.

**Callers:** —  
**Callees:** `00226f70`
**Key globals / structures:** None identified in the inventory note.

## 0x00221300 renderInit

**Description:** Maintained symbol-map entry: picks the scene draw function and the viewport's camera block

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002213E0 renderInitNoOp

**Description:** Empty stub called from renderInit; kept as a seam/no-op initializer. [inferred]

**Callers:** `0x00221300`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002213E8 drawCarWheelsMid

**Description:** Maintained symbol-map entry: guess: middle-distance wheels

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00221570 drawCarWheelsNear

**Description:** Maintained symbol-map entry: guess: near wheels with tyre detail

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00221788 drawCar

**Description:** Maintained symbol-map entry: one car: paint colours, body near/far LOD, parts, wheels

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00221F70 drawCarShowroom

**Description:** Maintained symbol-map entry: guess: a single car outside races (garage/shop)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00222360 drawCarExtraParts

**Description:** Maintained symbol-map entry: a car model's part A / part B chains (model header masks)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002224F8 drawArcRibbon

**Description:** Maintained symbol-map entry: rewrites vertices of the ELF chain 0x299430 every frame, then calls it

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x299430`

## 0x002228A8 drawCarsField

**Description:** Maintained symbol-map entry: 24 cars: shadow quads (ELF chain 0x290570), then drawCar with LOD

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x290570`

## 0x00222B70 drawCarsAction

**Description:** Maintained symbol-map entry: drawCarsField for the mini-game maps

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00222F08 drawRaceProps

**Description:** Maintained symbol-map entry: guess: race-course placed objects

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002232E8 drawFieldProps

**Description:** Maintained symbol-map entry: 32 placed objects from the tile's misc models (0x177F580)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x177F580`

## 0x00223410 InitCarRenderRegs

**Description:** Zeroes 0x800 bytes (DAT_0177eb10) then writes four GIF2 mscal paths (0x2b26/0x2b66) — car-render registers template. [inferred]

**Callers:** `0x00241490`  
**Callees:** `0x0027e840`
**Key globals / structures:** `DAT_0177eb10`

## 0x002234C8 drawModelParts

**Description:** Maintained symbol-map entry: draws part A and/or B of a model (mask bit 0 / 1)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00223588 drawCarShadow

**Description:** Maintained symbol-map entry: a car's shadow quad: colours (VU 0x18), object matrix, ELF chain 0x290570 (entry5); guess from its chain

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x290570`

## 0x002236D8 DrawCarVariant0

**Description:** Trivial wrapper: drawCar(0,0) — used as the "no animation" car draw path. [inferred]

**Callers:** `0x0022aeb0`, `0x0022bfd0`, `0x0022c280`, `0x0022cc38`  
**Callees:** `0x00221788`
**Key globals / structures:** None identified in the inventory note.

## 0x00223810 MakeVifCall15xGif

**Description:** Builds a 64-bit VIF call: reg 0x1be4 + 10*idx and reg 0x1bec + 10*idx inside an MSCA-like GIF packet. [inferred]

**Callers:** `0x00243c50`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002238A0 drawCarEffectA

**Description:** Maintained symbol-map entry: a car effect through ELF chain 0x290670 (called twice); sub-renderer of drawCarEffects, guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x290670`

## 0x00223B30 drawCarEffectB

**Description:** Maintained symbol-map entry: a car effect through ELF chain 0x290770 (called twice); sub-renderer of drawCarEffects, guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x290770`

## 0x00223D58 UpdateCarEffectSlots

**Description:** Iterates 128 car-effect slots, dispatches per-type effect update via FUN_00223b30, advances timers; called from drawCarEffects.

**Callers:** `00225120`  
**Callees:** `00223b30`, `00225390`
**Key globals / structures:** None identified in the inventory note.

## 0x002243E0 SpawnCarSmoke

**Description:** Lifecycle 0..0x10; emits smoke puff (effect 2) via FUN_00223b30, scales by speed-FUN_0021d358/300 (heat), with vu0MulMatrix-bias on rig. [inferred]

**Callers:** `0x00225120`  
**Callees:** `0x00223b30`, `0x002729d0`, `0x00272728`, `0x00272758`, `0x00274b10`
**Key globals / structures:** None identified in the inventory note.

## 0x00225120 drawCarEffects

**Description:** Maintained symbol-map entry: per-car effects by car flags (jets, wings, ...); guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00225608 drawSkyDome

**Description:** Maintained symbol-map entry: sky: ELF chain 0x290A40 (entry6), matrix through entry7

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x290A40`

## 0x002256A8 drawSkyLayers

**Description:** Maintained symbol-map entry: sky layers: ELF chains 0x2962E0 / 0x298A50 (entry6)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x2962E0`, `0x298A50`

## 0x00225988 updateTimeOfDayLight

**Description:** Maintained symbol-map entry: weights at 0x177F720 and fog from the clock (vblanks since midnight)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x177F720`

## 0x002261A0 updateLightDirection

**Description:** Maintained symbol-map entry: light direction relative to the camera position (block + 0x180)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226248 setFixedLight

**Description:** Maintained symbol-map entry: time-of-day weights/fog from the table at 0x29EA10

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x29EA10`

## 0x002262D8 SetDrawSkyColours

**Description:** If car type at param_2+0x22 not in {-1,0x11,0x22}, returns 1.0 RGBA; else writes scaled DAT_0177f754..56 floats → sky-dome colour choice. [inferred]

**Callers:** `0x00225608`  
**Callees:** —
**Key globals / structures:** `DAT_0177f754`

## 0x00226398 KickVifPacket

**Description:** Calls vifKick on a fixed packet address 0x28e730.

**Callers:** `002001b8`  
**Callees:** `002271f0`
**Key globals / structures:** `0x28e730`

## 0x002263B8 vifUploadCamera

**Description:** Maintained symbol-map entry: REF 15 qw from a camera block to VU1 4..0x12 (MSKPATH3 on)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226480 vifUploadSceneParams

**Description:** Maintained symbol-map entry: REF 3 qw to VU1 0x13 (time-of-day colour weights, fog)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226540 vifCameraSetup

**Description:** Maintained symbol-map entry: MSCALF entry0; BASE 0x8D, OFFSET 0x1AE (double-buffered input)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226610 vifSetBatchOffset

**Description:** Maintained symbol-map entry: 1 qw to VU1 0x1A: tile origin added to model positions

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002266E8 vifUploadAt4

**Description:** Maintained symbol-map entry: REF 8 qw to VU1 4 (no flush)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002267A8 vifUploadAt16

**Description:** Maintained symbol-map entry: REF 8 qw to VU1 0x10 (FLUSHE first)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226868 vifUploadLightMatrix

**Description:** Maintained symbol-map entry: 4 qw to VU1 0x14

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226940 vifUploadLightAt16

**Description:** Maintained symbol-map entry: 4 qw to VU1 0x10

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226A18 vifUploadColours

**Description:** Maintained symbol-map entry: 2 qw to VU1 0x18 (car paint / map colours)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226B18 vifObjectMatrix

**Description:** Maintained symbol-map entry: object matrix (4 qw) to VU1 0, then MSCALF entry1

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226C28 vifBatchRegs

**Description:** Maintained symbol-map entry: REGLIST GIF tag + 2 GS regs (e.g. TEX0_2) to TOPS+0x192

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226D58 vifFixedRegs

**Description:** Maintained symbol-map entry: as vifBatchRegs, to VU1 0x21F (no TOPS)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226E88 vifUploadPacket

**Description:** Maintained symbol-map entry: REF n qw to TOPS+0x192 (a prebuilt GS packet)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00226F70 vifDirect

**Description:** Maintained symbol-map entry: REF n qw sent by DIRECT (PATH2), FLUSH first

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00227048 vifCallModel

**Description:** Maintained symbol-map entry: DMA CALL into a model's prebuilt VIF chain

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00227108 vifBeginPacket

**Description:** Maintained symbol-map entry: 0: main buffer 0x1750BF0; 1: second buffer 0x1730BF0

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x1750BF0`, `0x1730BF0`

## 0x00227178 vifEndPacket

**Description:** Maintained symbol-map entry: END tag (MSKPATH3 off); returns the chain start

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002271F0 vifKick

**Description:** Maintained symbol-map entry: starts VIF1 DMA in chain mode at the given tag

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00227270 vifObjectMatrixSky

**Description:** Maintained symbol-map entry: object matrix to VU1 0, then MSCALF entry7

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002423B8 InitializeEffectPackets

**Description:** Initializes four fixed graphics packet words for a course effect. [inferred]

**Callers:** `0x00241490`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0026ABD0 ApplyAdjusterOffsets

**Description:** Reads 0xC table type, applies signed deltas to item fields 0xB0/0xB2, flag bits adjust floats 0x90/0x94/0x98, rebuilds VU0 matrix. [inferred]

**Callers:** —  
**Callees:** `0x00207ec8`, `0x00272ae8`, `0x00272b90`, `0x00272c38`, `0x00273268`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

# World Rendering, Props & Effects

Field/course props, animated world effects, coin pickup visuals, and map-specific draw dispatch.

## 0x00224068 SpawnCarDustPuffs

**Description:** Per frame advances a 0..0x20 cycle, scatters up to 2 dust puffs via FUN_00223b30 (effect 1) at world points, registers slot at DAT_0172ff70 if 2 units away. [inferred]

**Callers:** `0x00225120`  
**Callees:** `0x00223b30`, `0x002729d0`, `0x00272728`, `0x00274b10`
**Key globals / structures:** `DAT_0172ff70`

## 0x002246C0 SpawnCarSparks

**Description:** Cycle 0..0x10; emits spark (effect 3) if 1.25 units from registered slot; same rig-bias+heat scaling as sparks generator. [inferred]

**Callers:** `0x00225120`  
**Callees:** `0x00223b30`, `0x002729d0`, `0x00272728`, `0x00272758`, `0x00274b10`
**Key globals / structures:** None identified in the inventory note.

## 0x00225390 AdvanceCarEffectSlot

**Description:** Ticks stored effect slot (state byte controls two sub-animations with type-specific falloff); updates DAT_0172ff7c and rtt param. [inferred]

**Callers:** `0x00223d58`  
**Callees:** `0x00273268`, `0x00272728`
**Key globals / structures:** `DAT_0172ff7c`

## 0x00241490 DispatchCourseEffect

**Description:** Selects and initializes a course effect handler from the current course/event ID. [inferred]

**Callers:** `0x0020EBE8`  
**Callees:** `0x00223410`, `0x00241670`, `0x002423B8`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x00241838 NoOpEffect

**Description:** Performs no action. [inferred]

**Callers:** `0x00241490`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00241BA0 NoOpEffectB

**Description:** Performs no action. [inferred]

**Callers:** `0x00241490`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

# Maps & Minimap Rendering

Town/race minimaps, tile-slot queries, marker placement, and map-specific packet construction.

## 0x00207280 FindWorldMeshIntersection

**Description:** Adjusts a world position across map sectors and tests candidate meshes for intersection.

**Callers:** `0x002080d8`  
**Callees:** `0x00206f28`, `0x0023de30`, `0x002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x0020CC30 CheckInputMapping

**Description:** Checks a controller input mapping for the selected input mode.

**Callers:** `0x0020d120`  
**Callees:** `0x0020c7b8`
**Key globals / structures:** None identified in the inventory note.

## 0x0020CD10 UpdateInputMapping

**Description:** Updates input mapping state according to the active controller mode.

**Callers:** `0x0020d120`  
**Callees:** `0x0020c7b8`
**Key globals / structures:** None identified in the inventory note.

## 0x0020CDC0 DispatchInputMapping

**Description:** Dispatches input processing according to the active controller mapping mode.

**Callers:** `0x0020d120`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00214078 SetControllerButtonConfig

**Description:** Writes configured button assignments from menu selection indices back into player control mapping fields.

**Callers:** `0x00214220`, `0x00214470`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00214470 HandleControllerConfigMenuInput

**Description:** Handles navigation and button remapping for controller configuration menu, verifying duplicate assignments.

**Callers:** `0x00216738`  
**Callees:** `0x00206D28`, `0x00213C40`, `0x00214078`, `0x00214220`, `0x00258E50`, `0x00204C48`, `0x0027E5F8`, `0x0027E840`
**Key globals / structures:** None identified in the inventory note.

## 0x0021F920 mapViewMatrix

**Description:** Maintained symbol-map entry: map view block (VU1 4..): offset x, y (+0x20/0x24), scale s (x) and s*DAT_332CAC (y) from (x, y, s)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `DAT_332CAC`

## 0x00221258 tileSlotLoaded

**Description:** Maintained symbol-map entry: 1 if the map tile slot (0..3, id & 3) holds that tile id: quadrant byte 0x3335B8 >= 0

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x3335B8`

## 0x00221288 tileSlotOrigin

**Description:** Maintained symbol-map entry: matrix of the tile slot's quadrant origin (tileQuadrantOrigins), 0 if the slot is empty

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002277B0 InitSaveSlotDefaults

**Description:** Memsets a 0x3448-byte save slot and seeds its action-map/flags/limits/strings and 2 sub-tables (UnnamedRacer + gs_locations_table prefix). [inferred]

**Callers:** `0x00229fd8`  
**Callees:** `0x00250dd0`, `0x0027e840`
**Key globals / structures:** `gs_locations_table`

## 0x0023DEB8 IsMapEntryEnabled

**Description:** Returns whether the mapped flag for an in-range entry is set; treats invalid entries as enabled. [inferred]

**Callers:** `0x00259138`, `0x0026BE50`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00241E90 townMapCoins

**Description:** Maintained symbol-map entry: town map: a dot per coin of the table 0x2A3970 whose tile is loaded and not yet collected (FUN_0023DBF0 = collected)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x2A3970`

## 0x00242D20 raceMapMarkers

**Description:** Maintained symbol-map entry: race map: arrow chain 0x290870 for player 1 (red, 0x2A4370) and 2 (teal, 0x2A4380), dot chain 0x290940 per rival (green 0.5 alpha, 0x2A4390)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x290870`, `0x2A4370`, `0x2A4380`, `0x290940`, `0x2A4390`

## 0x00242FE8 townMapTask

**Description:** Maintained symbol-map entry: town/field HUD map: bezel (0x249208), gs_map_draw_functions_town, player arrow, town cars, coins, clock strip

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x249208`, `gs_map_draw_functions_town`

## 0x00243338 mapDot

**Description:** Maintained symbol-map entry: object matrix (scale s, x, z) + dot chain 0x290940: one coin/pickup mark

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x290940`

## 0x00243398 raceMapPickups

**Description:** Maintained symbol-map entry: race map: dots (yellow 0x2A43A0) at the pickup list 0x177EB40 (16 bytes each: x, ?, z, ?)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x2A43A0`, `0x177EB40`

## 0x00249208 townMapBezel

**Description:** Maintained symbol-map entry: UiBatch sprites of the town map's frame (3 sprites, TBP 7828); the second tab's marker picture is drawn here

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

# Day/Night, Lighting & Environment

Clock-driven lighting, fog, day-cycle tasks, and environment transition scripts.

## 0x002114E0 UpdateTimeOfDayCounter

**Description:** Increments and wraps the 24-hour time-of-day tick counter at offset +0x14, updating DAT_00335730 if flag 0x1000 is set.

**Callers:** `0x002156D0`  
**Callees:** —
**Key globals / structures:** `DAT_00335730`

## 0x0022D8C8 DrawRecordTableRows

**Description:** Emits up to three result rows via FUN_002340c0, highlighting entries equal to best record DAT_00335700. [inferred]

**Callers:** `0x0022ddd0`  
**Callees:** `0x002340c0`
**Key globals / structures:** `DAT_00335700`

## 0x00243588 GetLabelEntry_eff40

**Description:** Returns string pointer from Headlights label table at 002eff40 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0026CA48 ApplyTimeOfDayLighting

**Description:** Sets time-of-day value (+0x14) from DAT_002dcd28[arg], re-applies updateTimeOfDayLight every frame, spawns setup tasks on start. [inferred]

**Callers:** `0x0026CB60`, `0x0026CCD0`, `0x0026D1C0`, `0x0026D2C0`  
**Callees:** `0x00204F58`, `0x00210158`, `0x00210430`, `0x0021C6A0`, `0x00204C48`, `0x00225988`
**Key globals / structures:** `DAT_002dcd28`

## 0x0026CB60 StartAreaIntroDayCycle

**Description:** Area 0xa script: spawn lighting task, BGM 0x0a1c0201; tick 780 fires event, tick 844 restarts step 1 with BGM 0x0a1a0201. [inferred]

**Callers:** `0x0022AEB0`, `0x0026CCD0`  
**Callees:** `0x00258F30`, `0x00258FB0`, `0x00258EC8`, `0x00204DE8`, `0x00204C48`
**Key globals / structures:** `0x0a1c0201`, `0x0a1a0201`

## 0x0026CCD0 DispatchAreaEntryTask

**Description:** Queues 12 asset loads, polls area entry (FUN_00205c08), spawns day-cycle task chosen by area byte +0xd (0xa->cb60, 0xb->d2c0). [inferred]

**Callers:** `0x0026B848`  
**Callees:** `0x00204D60`, `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x00258E50`, `0x00258F70`, `0x00258FF0`, `0x00204DE8`, `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x0026CF20 BrowseItemsMenuTask

**Description:** Runs FUN_0026C588 then moves 2-at-a-time cursor over item count at task+0x26 with u/d buttons; exit btn X kills task. [inferred]

**Callers:** —  
**Callees:** `0x00238a48`, `0x0026c588`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0026D008 UNKNOWN_0026D008

**Description:** Task trampoline spawned by arrival handler: body is a single call to FUN_00265568(void). [inferred]

**Callers:** `0x002105B8`, `0x0026BE50`  
**Callees:** `0x00265568`
**Key globals / structures:** None identified in the inventory note.

## 0x0026D020 HandleMenuScrollSelect

**Description:** Flags at +0x30 scroll a selection index (0x1000/0x4000), 0x100 confirms item +0x20 via FUN_0020e5b8, 0x50 chains to 0x0026bc08. [inferred]

**Callers:** `0x0026BC08`  
**Callees:** `0x002080F8`, `0x0020E5B8`, `0x00250738`
**Key globals / structures:** `0x0026bc08`

## 0x0026D108 HouseEntryInitTask

**Description:** Computes work target via FUN_0023DB80 and pointer from gs_houses_residents table, sets 0x177EA5C, spawns tasks 0x20DC50/0x22C518. [inferred]

**Callers:** —  
**Callees:** `0x0023db80`, `0x00204c48`
**Key globals / structures:** `gs_houses_residents`, `0x177EA5C`, `0x20DC50`, `0x22C518`

## 0x0026D1C0 AdvanceDayCycleStep

**Description:** Waits for clock field (+8) to reach DAT_002dcd50[step], kills/respawns lighting task 0x0026ca48 per step, ends after step 10. [inferred]

**Callers:** `0x0026CB60`, `0x0026D2C0`  
**Callees:** `0x00204DE8`, `0x00204C48`
**Key globals / structures:** `DAT_002dcd50`, `0x0026ca48`

## 0x0026D2C0 StartDayCycleStepTask

**Description:** Area 0xb entry: BGM 0x0a1a0201, spawns lighting task 0x0026ca48 at step 1, swaps handler to stepper 0x0026d1c0 when player ready. [inferred]

**Callers:** `0x0022AEB0`, `0x0026CCD0`  
**Callees:** `0x00258F30`, `0x00258EC8`, `0x00204C48`
**Key globals / structures:** `0x0a1a0201`, `0x0026ca48`, `0x0026d1c0`

## 0x0026D370 InitDefaultSpawnPhase

**Description:** Boot phase: sets id 81000, inits progress globals, spawns entity tasks, writes player bytes, installs FUN_00211808. [inferred]

**Callers:** `0x00205088`  
**Callees:** `0x00204C48`
**Key globals / structures:** None identified in the inventory note.

## 0x0026D438 InitSelectedSpawnPhase

**Description:** Boot phase: maps FUN_0027afd8 select case 0-5 to spawn ids via FUN_00229f70, sets progress ptr, chains to 0x0026d550. [inferred]

**Callers:** `0x00205088`  
**Callees:** `0x00229F70`, `0x00229FD8`, `0x0027AFD8`, `0x00204C48`
**Key globals / structures:** `0x0026d550`

## 0x0026D550 LoadRaceSceneEntities

**Description:** Once entity ready (+0xf==0): spawns task 0x0025bcf0, spawns entities 0x370001/0x300001, chains handler to 0x0026d5e0. [inferred]

**Callers:** `0x0026D438`  
**Callees:** `0x00204C48`
**Key globals / structures:** `0x0025bcf0`, `0x370001`, `0x300001`, `0x0026d5e0`

## 0x0026D5E0 ContinueRaceEntityLoad

**Description:** If flag +0x24 set: respawns task 0x0025bcf0 with arg 1 and chains handler to 0x0026d628. [inferred]

**Callers:** `0x0026D550`  
**Callees:** `0x00204C48`
**Key globals / structures:** `0x0025bcf0`, `0x0026d628`

## 0x0026D628 ApplyRacePlayerMode

**Description:** If flag +0x24 set: writes player mode bytes (+0x21=9, +0x23=8), calls FUN_00211708, chains handler to 0x0026d678. [inferred]

**Callers:** `0x0026D5E0`  
**Callees:** `0x00211708`
**Key globals / structures:** `0x0026d678`

## 0x0026D678 FinishBootPhase

**Description:** If task field +0x10==0: resets progress globals, spawns entity 0x70001, sets player bytes 1/0, installs handler FUN_00211808. [inferred]

**Callers:** `0x0026D628`  
**Callees:** `0x00204C48`
**Key globals / structures:** `0x70001`

## 0x0026D700 DrawRaceHudNumbers

**Description:** Packs timer (+0xa0/+0xa8) and formats counters +0x90..0x98, +0x19c, +0x1f8 into numeric HUD slots via FUN_0027eee0/FUN_00203a50. [inferred]

**Callers:** `0x0021BA70`  
**Callees:** `0x00203A50`, `0x0021C8D0`, `0x0027B7B0`, `0x0027EEE0`
**Key globals / structures:** None identified in the inventory note.

# Progression, Collection Flags & World Queries

Persistent flags, stamps, resident slots, unlock conditions, collection masks, and queued progress events.

## 0x00205AD0 CenterRectangle

**Description:** Positions a rectangle within screen bounds based on alignment flags and its width/height.

**Callers:** `0x00209520`, `0x00209848`, `0x00209cb0`, `0x00228d10`, `0x00229538`, `0x0022bb88`, `0x0022c8a0`, `0x0022ca28`, `0x0022cd38`, `0x00239260`, `0x00239748`, `0x00239900`, `0x00239a80`, `0x0023bcd0`, `0x00248bf0`, `0x00268e40`, `0x00269568`, `0x00208f50`, `0x002090c0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00205EE8 DrawWidgetIfFlag16

**Description:** If flag bit 0x10 set, draws a 2-unit item at computed centered position via FUN_00203e70.

**Callers:** —  
**Callees:** `00203e70`
**Key globals / structures:** None identified in the inventory note.

## 0x00205F48 DrawWidgetIfFlag16Alt

**Description:** If flag bit 0x10 set, draws a 3-unit item at computed position via FUN_00203e70.

**Callers:** —  
**Callees:** `00203e70`
**Key globals / structures:** None identified in the inventory note.

## 0x00206D28 NavigateGridSelection

**Description:** Moves a selected grid index according to directional flags and wraps within grid dimensions.

**Callers:** `0x00205350`, `0x00212ac8`, `0x002135c8`, `0x002137b0`, `0x00214470`, `0x00261638`, `0x00265b30`, `0x00267248`, `0x0026b1b8`, `0x0026b5c8`  
**Callees:** `0x00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x002080D8 SelectMeshIntersectionRoutine

**Description:** Selects the mesh intersection function based on a mode flag.

**Callers:** `0x0020f950`, `0x00210158`, `0x002406f8`, `0x0025dbb8`, `0x0025e278`, `0x00263e68`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020E1B8 AbortCdOperation

**Description:** Clears flag at DAT_00333370 and calls sceCdBreak if busy flag bit 0x8000 is set in DAT_003356c0.

**Callers:** `0x0020DF98`  
**Callees:** `0x002791D8`
**Key globals / structures:** `DAT_00333370`, `DAT_003356c0`

## 0x0020E4E8 ToggleInputFlagWrapper

**Description:** Thin wrapper calling FUN_0020dcd8 with param XOR 1.

**Callers:** —  
**Callees:** `0020dcd8`
**Key globals / structures:** None identified in the inventory note.

## 0x0021C928 StoreIndexedPosition

**Description:** Writes scaled int3, float3 and a 16-bit tag into slot (stride 392B) of global race-object position table indexed by param_4 [inferred]

**Callers:** `0x00250F00`, `0x00254430`, `0x00254D20`, `0x002595F0`, `0x0025A120`, `0x0025EB70`, `0x0025FF00`, `0x00260638`, `0x00260B48`, `0x00260DC0`, `0x00262678`, `0x002631A0`, `0x00263710`, `0x002638C0`, `0x00263C38`, `0x00263E68`, `0x002642B8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021CE08 SelectScene3dHandler

**Description:** Installs one of two scene-3D handler entry points (0x21ce48 or 0x21cf80) into gs_game_scene3d_handler based on a flag

**Callers:** `0x0020F950`, `0x00210430`  
**Callees:** —
**Key globals / structures:** `0x21ce48`, `0x21cf80`, `gs_game_scene3d_handler`

## 0x0021FB20 GetCarSlotEntry

**Description:** Returns pointer into a 0x20-byte stride base (DAT_0029ce00) indexed by arg; used as a per-car slot accessor. [inferred]

**Callers:** `0x0021e690`  
**Callees:** —
**Key globals / structures:** `DAT_0029ce00`

## 0x002249B8 SpawnCarDebrisCycle

**Description:** Each call rolls 4 random 0..1 offsets to add a new "type-8" emitter (rotational debris) into slot table; count/speed scale with car state. [inferred]

**Callers:** `0x00225120`  
**Callees:** `0x0021d358`, `0x0027b540`, `0x002729d0`, `0x00272728`, `0x00272758`
**Key globals / structures:** None identified in the inventory note.

## 0x00227B28 InitPlayerSlotStrings

**Description:** Builds "PLAYER 1"/"PLAYER 2" 8-byte names and several control-flag bytes for the player slot block at DAT_0177fd8c. [inferred]

**Callers:** `0x00209848`  
**Callees:** —
**Key globals / structures:** `DAT_0177fd8c`

## 0x00229F70 SelectProgressSlot

**Description:** Points gs_progress_ram_address and DAT_017804d0[slot] at the slot's buffer from table PTR_DAT_002ed724.

**Callers:** `0x0026d438`  
**Callees:** `0x0023d2c0`
**Key globals / structures:** `gs_progress_ram_address`, `DAT_017804d0`, `PTR_DAT_002ed724`

## 0x0022E5F8 AccumulateDistanceTotals

**Description:** Accumulates accumulator DAT_0177ae54 (>>15) into a global total and per-slot counters for up to three flagged objects.

**Callers:** `0x0020f3b8`  
**Callees:** —
**Key globals / structures:** `DAT_0177ae54`

## 0x0022E718 AccumulateDistanceSingle

**Description:** Adds accumulator DAT_0177ae54 (>>15) to global total DAT_0177fdb8 and its per-slot counter, then zeroes it.

**Callers:** `0x002105b8`  
**Callees:** —
**Key globals / structures:** `DAT_0177ae54`, `DAT_0177fdb8`

## 0x0022E780 DrawSlotScoreNumber

**Description:** Draws the value at DAT_0177fdb4 for slot param_3 at (param_1, param_2*2-6) via FUN_00202d10. [inferred]

**Callers:** `0x002161e0`, `0x00268e40`  
**Callees:** `0x00202d10`
**Key globals / structures:** `DAT_0177fdb4`

## 0x0022EA08 GetPlayerScoreboardPtr

**Description:** Returns the score/status pointer for entry param_1: a per-slot block when the flag nibble is 0, else a house table entry.

**Callers:** `0x00236858`, `0x00236d28`, `0x00237080`, `0x00237878`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022ECB8 InitRankingDisplay

**Description:** Seeds flag byte +0x25 from the entry flags, XORs it with 0x3f, installs handler 0x22eb78 and runs it once. [inferred]

**Callers:** `0x0022ddd0`  
**Callees:** `0x0022eb78`, `0x00234528`, `0x00258f30`
**Key globals / structures:** `0x22eb78`

## 0x00234080 SetSlotConfig

**Description:** Writes slot-local config values for a specific item and its active state [inferred]

**Callers:** `0x0022ddd0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00235460 InitObjectType20

**Description:** Clears a 0x8cb0-byte object, sets type tag +0x8008=0x20 and initialises four +0x8460 slots to -1. [inferred]

**Callers:** `0x002631a0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002354F0 SetIndexedValueAndSum

**Description:** Writes arg3 at index arg2 of the +0x8460 array, records the index at +0x8458, returns the sum of the three positive slots. [inferred]

**Callers:** `0x002631a0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00236328 DrawLicenseList

**Description:** Loops six course slots and draws list rows with point values for each player below the active player count. [inferred]

**Callers:** `0x002364b0`  
**Callees:** `0x00202150`, `0x00204310`
**Key globals / structures:** None identified in the inventory note.

## 0x00237680 DrawShopRow

**Description:** Draws one shop row (name, resident, price, quantity) for town arg4/shop arg3 when the table entry at DAT_01780750 is valid. [inferred]

**Callers:** `0x00238a48`  
**Callees:** `0x00203b78`, `0x00204310`
**Key globals / structures:** `DAT_01780750`

## 0x00237AC0 LookupPlayerNameSlot

**Description:** Searches or creates a name slot (max 20) for the current player id, stores its index at +0x1c/+0x1e, else uses "ANYBODY". [inferred]

**Callers:** `0x00211320`  
**Callees:** `0x0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x0023BF28 FindFlaggedSlotIndex

**Description:** Scans 160-bit per-slot flag set (5 words, stride 0xd12) for Nth set bit with optional filters in param_4; returns index or -1

**Callers:** `0023d5b0`, `00246250`, `00267f48`, `002681a8`, `00268e40`  
**Callees:** `00243670`
**Key globals / structures:** None identified in the inventory note.

## 0x0023C150 CountFlaggedSlots

**Description:** Counts set bits in per-slot flag set (5 words) with same filters; with param_2=-1 returns total count, else rank of given bit

**Callers:** `0023d5c8`, `00246250`, `00268e40`  
**Callees:** `00207b20`, `00243670`
**Key globals / structures:** None identified in the inventory note.

## 0x0023C418 SetObjectAttribute

**Description:** Writes byte attribute at obj+id+0xc and updates obj+8 flag bits (ids 1,2,7-12) from table lookups; no callees

**Callers:** `00227c68`, `0022bb88`, `0023b168`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023C7B0 ReleaseStampFlag

**Description:** Clears stamp-bit for id at +0x21 in DAT_01780338[+0x23]; if slot mask empties calls FUN_0023cb68(0x19); sets house-stamp bit

**Callers:** `0022aeb0`, `0022cd38`, `0023ad00`, `0023b168`  
**Callees:** `0023cb68`
**Key globals / structures:** `DAT_01780338`

## 0x0023CB68 SetProgressFlagAndQueue

**Description:** Sets progress bit in DAT_0177fdd0, enqueues a code byte in 128-byte ring buffer, awards stamp 0x33 when all bits set

**Callers:** `002364b0`, `0023ad00`, `0023c7b0`  
**Callees:** `00207b20`, `0023c8a8`
**Key globals / structures:** `DAT_0177fdd0`

## 0x0023D058 ComputeSlotScore

**Description:** Computes a weighted score for slot param_1 by summing per-slot flags/counts and table weights [inferred]

**Callers:** `00243a78`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023D390 ClearFlaggedSlotBit

**Description:** Clears bit param_2 in the 5-word per-slot flag set, checking words in reverse order

**Callers:** `00239508`, `0023ad00`, `00268e40`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023D488 GetFlaggedSlotCount

**Description:** Returns how many of the 5 per-slot flag words have bit param_2 set; for slot 0/15 returns single-word bit test

**Callers:** `002161e0`, `0022c648`, `0022c8a0`, `0023ad00`, `00242fe8`, `00246250`, `0025be98`, `0025c1d8`, `0025c300`, `0025c528`, `0025c818`, `0025ca08`, `0025d3d8`, `0025d4e8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023D548 TestFlaggedSlotBit

**Description:** Returns the masked bit for param_2 from the per-slot flag set (word 4, or word 0 for slot 0/15)

**Callers:** `00268e40`, `00269568`, `00269900`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023D5E0 TestObjectAttribute

**Description:** Returns whether obj byte at param_2+0xc equals param_3

**Callers:** `0023ad00`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023D5F8 TestStampFlag

**Description:** Returns bit param_2 of stamp word DAT_01780338[param_1]; 0 if param_2 >= 32

**Callers:** `0022ca28`, `0023ad00`, `0026be50`  
**Callees:** —
**Key globals / structures:** `DAT_01780338`

## 0x0023D628 TestStateBitFc78

**Description:** Tests bit (id-1) in state bitmask DAT_0177fc78; id 0 checks all-set sentinel [inferred]

**Callers:** `002105b8`, `0022aeb0`, `0022d1e0`, `002364b0`, `00237878`, `00239a80`, `0023ad00`, `00240cd8`, `00249208`, `0025be98`, `0025c088`, `0025c1d8`, `0025c300`, `0025c528`, `0025c818`, `0025ca08`, `0025d3d8`, `0025d4e8`, `0025e630`, `0025f6a0`, `0025fbf8`, `002631a0`, `00263e68`, `002652b8`, `002678e0`, `002679c8`, `00267a80`  
**Callees:** —
**Key globals / structures:** `DAT_0177fc78`

## 0x0023D6A0 TestStateBitFc88

**Description:** Tests bit (id-1) in state bitmask DAT_0177fc88; returns 0/1 as 64-bit value [inferred]

**Callers:** `002652b8`  
**Callees:** —
**Key globals / structures:** `DAT_0177fc88`

## 0x0023D6E0 SetStateBitFc78AndFc88

**Description:** Sets bit id in both DAT_0177fc78 and DAT_0177fc88 state bitmasks [inferred]

**Callers:** `00265568`  
**Callees:** —
**Key globals / structures:** `DAT_0177fc78`, `DAT_0177fc88`

## 0x0023D730 SetStateBitFc88

**Description:** Sets bit (id-1) in state bitmask DAT_0177fc88 [inferred]

**Callers:** `0023ad00`  
**Callees:** —
**Key globals / structures:** `DAT_0177fc88`

## 0x0023D778 TestFlagBitFc98

**Description:** Returns whether bit id is set in flag bitmask DAT_0177fc98

**Callers:** `002105b8`, `0022c648`, `0023ad00`, `0023f0a0`, `0023fcb0`, `00240cd8`, `00259138`, `0025c088`, `0025d328`, `00267a80`  
**Callees:** —
**Key globals / structures:** `DAT_0177fc98`

## 0x0023D7A8 SetFlagBitFc98

**Description:** Sets bit id in flag bitmask DAT_0177fc98

**Callers:** `00227c68`, `0023ad00`  
**Callees:** —
**Key globals / structures:** `DAT_0177fc98`

## 0x0023D7E0 ClearFlagBitFc98

**Description:** Clears bit id in flag bitmask DAT_0177fc98

**Callers:** `002105b8`, `0023ad00`  
**Callees:** —
**Key globals / structures:** `DAT_0177fc98`

## 0x0023D878 AwardHouseStampIfUnset

**Description:** If house slot id<10 has no stamp in DAT_01780338, awards its stamp via gs_stamps_award_function

**Callers:** `002105b8`  
**Callees:** `0023c8a8`
**Key globals / structures:** `DAT_01780338`, `gs_stamps_award_function`

## 0x0023D8C8 GetOrSetPresidentRaceResult

**Description:** Stores param into gs_president_race_result_address when not -1; always returns the stored value

**Callers:** `0020f3b8`, `00235b58`, `00237c48`, `00238950`, `0023ad00`, `00252738`, `00254430`, `00254d20`, `002595f0`, `0025a120`, `0025dbb8`, `0025e278`, `0025e630`, `0025eb70`, `0025f6a0`, `0025fbf8`, `0025ff00`, `00260638`, `00262458`, `00262678`, `002631a0`, `002638c0`, `00263c38`, `00263e68`  
**Callees:** —
**Key globals / structures:** `gs_president_race_result_address`

## 0x0023D968 CountBitsFca8Fcb0

**Description:** Popcount of the two 64-bit words DAT_0177fca8/fcb0 for slot param_1 (stride 0x689)

**Callers:** `0023ad00`, `00241ba8`  
**Callees:** —
**Key globals / structures:** `DAT_0177fca8`

## 0x0023DA58 InitActorSlot

**Description:** Copies slot template fields from DAT_0177ea50[param_1] into slot param_2 and sets its class/state bytes; calls FUN_0020ad18, FUN_00250dd0 [inferred]

**Callers:** `00239378`  
**Callees:** `0020ad18`, `00250dd0`
**Key globals / structures:** `DAT_0177ea50`

## 0x0023DB00 GetSlotDataPtr

**Description:** Returns data pointer for slot param_2 (or resident table entry when param_1 nonzero); NULL when unset

**Callers:** `002359e8`, `00235f10`, `00239378`, `00239508`, `00239748`, `00239900`, `00247bf0`, `00247e38`, `00248580`, `00264498`, `00264cb0`, `00269f48`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023DBC8 CheckSlotPresence

**Description:** Returns 0 if both slot-A (fdd4) and slot-B (fdd6) entries are nonzero, else 1/2 [inferred]

**Callers:** `00239748`, `0023ad00`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023DBF0 TestFlagFca8Clear

**Description:** Returns true when bit id is clear in DAT_0177fca8 word set (or both words zero for id 0) [inferred]

**Callers:** `0023ad00`, `00241ba8`, `00241e90`, `00241d38`  
**Callees:** —
**Key globals / structures:** `DAT_0177fca8`

## 0x0023DCC0 TestFlagFcb8

**Description:** Tests bit param_2 in 64-bit word of DAT_0177fcb8 array (index 9 for param 0xd, else param-1)

**Callers:** `0023ad00`  
**Callees:** —
**Key globals / structures:** `DAT_0177fcb8`

## 0x0023DCF8 SetFlagFcb8

**Description:** Sets bit param_2 in DAT_0177fcb8 word (index 9 for param 0xd, else param-1)

**Callers:** `00237c48`  
**Callees:** —
**Key globals / structures:** `DAT_0177fcb8`

## 0x0023DE10 IsSlotActive

**Description:** Returns true when slot's +0x23 equals DAT_00335734 and its +0x21 flag is zero [inferred]

**Callers:** `002105b8`, `00259138`  
**Callees:** —
**Key globals / structures:** `DAT_00335734`

## 0x0023DE30 TestProgressFlag

**Description:** Returns whether bit param_1 is set in progress bitmask DAT_0177fdd0

**Callers:** `00207280`, `00219ed8`, `0023ad00`, `0021fb38`, `00220030`  
**Callees:** —
**Key globals / structures:** `DAT_0177fdd0`

## 0x0023DF00 GetQueuedByte

**Description:** Returns the low seven bits of a queued byte by index, or -1 when beyond the queue length. [inferred]

**Callers:** `0x00264498`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023DF38 AppendQueuedByte

**Description:** Appends a byte to a 128-byte queue, shifting older entries left when full. [inferred]

**Callers:** `0x00227C68`, `0x0022D1E0`, `0x002364B0`, `0x00241BA8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023DFE0 HasQueuedByteFlag

**Description:** Returns bit 7 of the indexed byte in the shared 128-byte queue. [inferred]

**Callers:** `0x00249208`, `0x00264498`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023DFF8 SetQueuedByteFlag

**Description:** Sets bit 7 on the indexed byte in the shared 128-byte queue. [inferred]

**Callers:** `0x00264498`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243A78 ComputeScaledPartValue

**Description:** Scales the value from DispatchByIndex15 by two FUN_0023d058 readings (/2000 or /4000 by flag); returns int [inferred]

**Callers:** `00246250`, `00268428`, `00268890`, `00268e40`  
**Callees:** `0023d058`, `00243670`
**Key globals / structures:** None identified in the inventory note.

## 0x002678E0 KnockCollectMaskGates

**Description:** Queries 7 knock slots (0x5C,0x27,0x61,0x13,0x43,0x59,0x3B) and ORs result bits into caller mask[0]. [inferred]

**Callers:** `0x00267f08`  
**Callees:** `0x0023d628`
**Key globals / structures:** None identified in the inventory note.

## 0x002679C8 MailCollectMaskGates

**Description:** Queries 5 mail slots (0x5A,0x2C,0x11,0x2F,0xE) and ORs check bits (4/8/0x20/0x80/0x100) into caller mask. [inferred]

**Callers:** `0x00267f20`  
**Callees:** `0x0023d628`
**Key globals / structures:** None identified in the inventory note.

## 0x00267A80 HintCollectMaskGates

**Description:** Queries 14 hint/GPS slots (d628/d778) setting bits across mask[0..4]; maps slot IDs to collection/hint flags. [inferred]

**Callers:** `0x00267db0`  
**Callees:** `0x0023d628`, `0x0023d778`
**Key globals / structures:** None identified in the inventory note.

## 0x00267C30 UNKNOWN_0x00267C30

**Description:** Remaps a bitmask to a packed slot table and dispatches the matching action for the selected index.

**Callers:** `00267f48`, `002681a8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00267DB0 UNKNOWN_0x00267DB0

**Description:** Flushes the high-level result deck by invoking the current finalizer callback.

**Callers:** —  
**Callees:** `00267a80`
**Key globals / structures:** None identified in the inventory note.

## 0x00267F08 UNKNOWN_0x00267F08

**Description:** Flushes the status callback for the next event by invoking its initializer helper.

**Callers:** —  
**Callees:** `002678e0`
**Key globals / structures:** None identified in the inventory note.

## 0x00267F20 UNKNOWN_0x00267F20

**Description:** Invokes the score-entry teardown routine for the result-list state.

**Callers:** —  
**Callees:** `002679c8`
**Key globals / structures:** None identified in the inventory note.

## 0x00267F38 UNKNOWN_0x00267F38

**Description:** No-op stub used as a placeholder while the state machine advances.

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00267F48 UNKNOWN_0x00267F48

**Description:** Finds the next active bit in a mode-specific bitmask and returns its slot index or -1.

**Callers:** `00246250`, `00269568`, `00269900`  
**Callees:** `0023bf28`, `0024f3b8`, `00267c30`
**Key globals / structures:** None identified in the inventory note.

## 0x002681A8 UNKNOWN_0x002681A8

**Description:** Counts active bits before the requested index in a packed result mask and returns the slot offset.

**Callers:** `00246250`, `00269568`, `00269900`  
**Callees:** `0023bf28`, `0024f3b8`, `00267c30`
**Key globals / structures:** None identified in the inventory note.

## 0x0026AE88 ApplyAdjusterScales

**Description:** Applies scale ops (+1 / x399 when flags 2/8 set) to float fields 0x3C/0x15C/0x158/0x38 and prints result at 0x168. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027ba50`, `0x0027baa8`, `0x0027bdc0`, `0x0027c110`, `0x0027eee0`
**Key globals / structures:** None identified in the inventory note.

# Hub, Town, House & Event Scripts

Location arrival, door/house interactions, town scripts, and narrative event setup.

## 0x002182B0 SpawnCarAtDoor

**Description:** Calculates initial spawn position, ground elevation, and heading angle for a car at a building entrance door.

**Callers:** `0x00218458`, `0x0021BA70`, `0x0021CA90`  
**Callees:** `0x00274AF8`
**Key globals / structures:** None identified in the inventory note.

## 0x0023AD00 InterpretScriptCommands

**Description:** Interprets control bytes 1-0x1d (conditions/setup) of a town-shop event script and returns the next script pointer. [inferred]

**Callers:** `0x0023b168`  
**Callees:** `0x0023c7b0`, `0x0023cb68`, `0x0023d390`, `0x0023d488`, `0x0023d5e0`, `0x0023d5f8`, `0x0023d628`, `0x0023d730`, `0x0023d778`, `0x0023d7a8`, `0x0023d7e0`, `0x0023d8c8`, `0x0023d8e8`, `0x0023d968`, `0x0023db80`, `0x0023dbc8`, `0x0023dbf0`, `0x0023dcc0`, `0x0023dd30`, `0x0023ddb8`, `0x0023de30`, `0x0023de50`, `0x0027ea30`
**Key globals / structures:** None identified in the inventory note.

# Shop, Garage, Tuning & Paint Editor

Garage/shop flows, inventory transactions, car tuning, and the color/paint editor.

## 0x00212A00 InitMenuElementLayout

**Description:** Populates element layout positions and selection state for a vertical list of menu items.

**Callers:** `0x00214AD0`, `0x00214EF8`, `0x00215220`, `0x002156D0`, `0x002161E0`, `0x00216738`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00229F20 InitTuningConstants

**Description:** Writes ten fixed u16 tuning constants into offsets 0x12e8-0x12fa of the passed object. [inferred]

**Callers:** `0x00214220`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00255070 BuildPaintEditorMenuSprites

**Description:** Fills a 0x175b570 sprite packet buffer with a grid of menu sprites, RGB color-coded swatches (red/green/blue/magenta/cyan/yellow), and 4 text labels for the paint editor. [inferred]

**Callers:** `0x00256958`  
**Callees:** `0x00206040`, `0x002063c8`, `0x002079a8`, `0x00207a28`, `0x0021d718`, `0x0021d788`, `0x0021dae8`, `0x0021db20`, `0x0021db48`
**Key globals / structures:** `0x175b570`

## 0x002557A0 DrawPaintCursorSprites

**Description:** Draws two 0x20x0x20 cursor sprites selected from a table (0x2ac600), or static sprites when blink timer is zero; second sprite driven by item index. [inferred]

**Callers:** `0x00256640`  
**Callees:** `0x002063c8`
**Key globals / structures:** `0x2ac600`

## 0x002558B8 DrawPaintEditorDigits

**Description:** Builds digit sprites for 6 paint-value nibbles with dimming by player cursor, draws hex digit characters, item-count banner text, and turn arrows, into the paint editor packet buffer. [inferred]

**Callers:** `0x002564e8`, `0x00256640`  
**Callees:** `0x00202150`, `0x00203cd0`, `0x002063c8`, `0x002075f0`, `0x00207998`
**Key globals / structures:** None identified in the inventory note.

## 0x002560F0 HandlePaintEditorInput

**Description:** Controller input for paint editor: moves cursor among 6 nibbles/palette, increments nibble of RGB value in progress record, wraps at bounds, plays beep 0x1d, swaps RGB fields. [inferred]

**Callers:** `0x00256640`  
**Callees:** `0x00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x002564E8 PaintEditorConfirmTask

**Description:** Paint-editor confirm step: redraws digits, on commit stores edited 32-bit paint value into gs_progress_ram entry, compares to previous value and charges payment via FUN_0023dc58; toggles between confirm and edit tasks. [inferred]

**Callers:** `0x00256640`  
**Callees:** `0x0022e7f0`, `0x0023dc58`, `0x002558b8`, `0x00258e50`, `0x00226f70`
**Key globals / structures:** `gs_progress_ram`

## 0x00256640 PaintEditorEditTask

**Description:** Paint editor main task: runs input handler, redraws menu and cursors, honors blink timer, on X/cancel transitions to confirm task FUN_002564e8, cancel dialog, or "Need more money" dialog. [inferred]

**Callers:** `0x002564e8`, `0x00256a78`, `0x00256958`  
**Callees:** `0x00207a28`, `0x0022e7f0`, `0x002557a0`, `0x002558b8`, `0x002560f0`, `0x00258e50`, `0x00204c48`, `0x00226f70`
**Key globals / structures:** None identified in the inventory note.

## 0x002568D8 ClosePaintEditorWindow

**Description:** Window slide-out animation: decrements 16-bit timer at task+2 by 4, moves the panel sprite up with FUN_002063c8, kills task when offset hits -0x38. [inferred]

**Callers:** `0x002564e8`, `0x00256640`  
**Callees:** `0x002063c8`, `0x00204de8`, `0x00226f70`
**Key globals / structures:** None identified in the inventory note.

## 0x00256958 OpenPaintEditorScene

**Description:** Opens paint editor scene: copies paint value from gs_progress_ram entry, detects mirrored RGB nibbles to set grayscale flag, builds menu sprites, then starts slide-in anim or hands off. [inferred]

**Callers:** `0x0022aeb0`  
**Callees:** `0x00255070`, `0x00256a78`
**Key globals / structures:** `gs_progress_ram`

## 0x00256A78 SlideInPaintEditorWindow

**Description:** Slide-in animation opposite of close: increments timer at task+2 by 4 until it reaches 0 while moving the panel sprite, then transitions to FUN_00256640. [inferred]

**Callers:** `0x00256958`  
**Callees:** `0x002063c8`, `0x00226f70`
**Key globals / structures:** None identified in the inventory note.

## 0x00268428 UNKNOWN_0x00268428

**Description:** Builds the shop purchase dialog and resolves the item price/stock message based on the selected buy action.

**Callers:** `00268e40`, `00269568`, `00269900`  
**Callees:** `00201fd0`, `00203f40`, `00203fc8`, `002058a8`, `00205c08`, `00205c20`, `00243468`, `00243670`, `00243a78`, `0024f4e0`, `00258e50`, `0027eee0`, `00204de8`, `0027e840`, `0027ef70`, `0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x00268890 UNKNOWN_0x00268890

**Description:** Builds the sell/confirm dialog and resolves the item price and result text for the sale action.

**Callers:** `00268e40`, `00269568`, `00269900`  
**Callees:** `00201fd0`, `00203f40`, `00203fc8`, `002058a8`, `00205c08`, `00205c20`, `00243468`, `00243670`, `00243a78`, `0024f4e0`, `00258e50`, `0027eee0`, `00204de8`, `0027e840`, `0027ef70`, `0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x00268E40 UNKNOWN_0x00268E40

**Description:** Runs the item-shop state machine, handles buy/sell transitions, and spawns the next dialog when a choice is made.

**Callers:** `00269e50`, `00269e88`  
**Callees:** `00202d10`, `00203e70`, `00204d60`, `00205350`, `00205ad0`, `00205c20`, `00205da0`, `0022e780`, `0022e7f0`, `0023bf28`, `0023c150`, `0023d2c0`, `0023d390`, `0023d548`, `0023dc58`, `00243670`, `00243a78`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00269568 UNKNOWN_0x00269568

**Description:** Handles the select-and-confirm menu flow for the shop state, updating the item index and result flags.

**Callers:** `00269e88`  
**Callees:** `00204d60`, `00205350`, `00205ad0`, `00205c20`, `00205da0`, `0022e7f0`, `0023d2c0`, `0023d548`, `0023dc58`, `00243670`, `00267f48`, `002681a8`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00269900 UNKNOWN_0x00269900

**Description:** Runs the cradle/shop branch for the single-item menu and clears the pending action when it completes.

**Callers:** `00269e88`  
**Callees:** `00204d60`, `00205c20`, `0022e7f0`, `0023d2c0`, `0023d548`, `0023dc58`, `00243670`, `00267f48`, `002681a8`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00269BD0 FlagIndexGetNth

**Description:** Scans flag words (param_2<<5 bits); returns bit index of the Nth set flag, or -1; -1 count asks FUN_0024F3B8. [inferred]

**Callers:** —  
**Callees:** `0x0024f3b8`
**Key globals / structures:** None identified in the inventory note.

## 0x00269C80 FlagIndexBitsUpTo

**Description:** Rewrite of 0x00269BD0: counts set bits before global index param_3 and reports whether that bit is set. [inferred]

**Callers:** —  
**Callees:** `0x0024f3b8`
**Key globals / structures:** `0x00269BD0`

## 0x00269D50 UNKNOWN_0x00269D50

**Description:** Creates the generic result window and exits when the parent task confirms the action.

**Callers:** `00269900`  
**Callees:** `002058a8`, `00205c08`, `00205c20`, `00258e50`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00269E50 UNKNOWN_0x00269E50

**Description:** Initializes the shop state handler and marks the mode as buy-enabled when the task starts.

**Callers:** `00248398`  
**Callees:** `00268e40`
**Key globals / structures:** None identified in the inventory note.

## 0x00269E88 UNKNOWN_0x00269E88

**Description:** Switches the current shop task between buy, sell, and neutral states based on the current menu state.

**Callers:** `0023b168`  
**Callees:** `0022ad70`, `00268e40`, `00269568`, `00269900`
**Key globals / structures:** None identified in the inventory note.

## 0x00269F48 UNKNOWN_0x00269F48

**Description:** Draws the course progress list and score card, allowing selection and update of the active race entry.

**Callers:** `0020a580`  
**Callees:** `00202350`, `00203b78`, `00203f40`, `00203fc8`, `00205350`, `00205c20`, `00205da0`, `0023db00`, `00258e50`, `0027eee0`, `00204de8`, `0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x0026A928 UNKNOWN_0x0026A928

**Description:** Scrolls and renders the course text ticker while it fades out as time advances.

**Callers:** `0026cb60`, `0026d2c0`  
**Callees:** `00203fc8`, `0027e840`, `0027ea30`
**Key globals / structures:** None identified in the inventory note.

# Carousel, Quick Race & Two-Player Results

Carousel item presentation/selection, split-player result views, and course-selection presentation.

## 0x00238A48 DrawRaceSummaryRow

**Description:** Draws a one-line race summary (course name, rank) plus three FUN_00237680 rows in the column selected by arg2 bit 0. [inferred]

**Callers:** `0x00267248`, `0x0026cf20`  
**Callees:** `0x00204310`, `0x00237680`
**Key globals / structures:** None identified in the inventory note.

## 0x00243B38 SetupCarouselSlotTransforms

**Description:** Loops 16 slots: sets draw state via FUN_0021d788, copies VU0 vectors and writes 0xff0 scale words

**Callers:** `002455c0`  
**Callees:** `0021d788`, `0021dab8`, `vu0CopyVector@002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x00243C50 BuildCarouselSlotPackets

**Description:** Builds per-slot GS draw packets for 16 carousel entries from item tables; uses GetPartValueU16 and FUN_00223810 [inferred]

**Callers:** `00246250`  
**Callees:** `0021d788`, `0021dab8`, `00223810`, `00243a00`, `vu0CopyVector@002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x00244078 LayoutRaceRowQuads

**Description:** Fills 16 rows of 4-vertex quad data (positions, colors) from race tables, table chosen by flag [inferred]

**Callers:** `002455c0`  
**Callees:** `0021d788`, `0021dab8`
**Key globals / structures:** None identified in the inventory note.

## 0x002443D0 BuildCarouselQuadA

**Description:** Writes quad vertex data for a carousel card using cos-based scale and per-slot offsets [inferred]

**Callers:** `00246250`, `002455c0`  
**Callees:** `cos@00274810`
**Key globals / structures:** None identified in the inventory note.

## 0x00244538 BuildCarouselQuadB

**Description:** Variant of BuildCarouselQuadA with different offsets; writes 4 vertices for a carousel card [inferred]

**Callers:** `002455c0`  
**Callees:** `cos@00274810`
**Key globals / structures:** None identified in the inventory note.

## 0x002446A8 BuildCarouselQuadC

**Description:** Variant of BuildCarouselQuadA with different offsets; writes 4 vertices for a carousel card [inferred]

**Callers:** `00246250`  
**Callees:** `cos@00274810`
**Key globals / structures:** None identified in the inventory note.

## 0x00244800 BuildCarouselQuadD

**Description:** Variant of BuildCarouselQuadA with different offsets; writes 4 vertices for a carousel card [inferred]

**Callers:** `002455c0`  
**Callees:** `cos@00274810`
**Key globals / structures:** None identified in the inventory note.

## 0x00244988 BuildCarouselDrawPacket

**Description:** Builds packed transform/GS register words for a carousel item, then sets up camera and light direction [inferred]

**Callers:** `002455c0`  
**Callees:** `FUN_0021db48`, `FUN_0021f910`, `buildCamera@0021f698`, `updateLightDirection@002261a0`, `vu0CopyMatrix@002729e0`
**Key globals / structures:** None identified in the inventory note.

## 0x00244D60 drawCarouselCells

**Description:** Maintained symbol-map entry: vifCallModel of one cell chain (0x177AB24 table); the carousel's course tiles, guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x177AB24`

## 0x00244F60 drawCarouselCar

**Description:** Maintained symbol-map entry: car models of the carousel / select screens (0x177F478 table, carModels); guess

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x177F478`

## 0x002455C0 drawCarouselScene

**Description:** Maintained symbol-map entry: likely: Quick Race course carousel (tile 1 cells)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00246250 shopMenuScene

**Description:** Maintained symbol-map entry: shop buy/sell scene (strings Selling, Buying)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002477A8 DrawCarouselStatText

**Description:** Draws stat labels and numeric values for a carousel item via text-draw helpers [inferred]

**Callers:** `00248180`  
**Callees:** `FUN_00201e48`, `FUN_00203fc8`, `FUN_00204310`, `FUN_00204568`, `FUN_002046c0`
**Key globals / structures:** None identified in the inventory note.

## 0x00247AC8 DrawCarouselStatBars

**Description:** Draws stat values for one item: sums 20-byte table slices and draws two text readouts [inferred]

**Callers:** `00247bf0`, `00247e38`  
**Callees:** `FUN_00201e48`, `FUN_00204310`
**Key globals / structures:** None identified in the inventory note.

## 0x00247BF0 DrawCarouselSlotPanel

**Description:** Draws a carousel slot panel (header, icons, stat rows) by calling DrawCarouselStatBars [inferred]

**Callers:** `00248180`  
**Callees:** `FUN_00203b78`, `FUN_00204310`, `FUN_00204568`, `FUN_0023db00`, `DrawCarouselStatBars@00247ac8`
**Key globals / structures:** None identified in the inventory note.

## 0x00247E38 DrawTwoPlayerResults

**Description:** Draws 2-player race results table UI, printing scores and calculating total sums for both players [inferred]

**Callers:** `00248180`  
**Callees:** `00201e48`, `00203b78`, `00204310`, `00204568`, `0023db00`, `00247ac8`
**Key globals / structures:** None identified in the inventory note.

## 0x00248180 UpdateRaceResultsMenu

**Description:** Task update function for quick race and 2-player race results screen, handling inputs and screen transitions [inferred]

**Callers:** `002455c0`  
**Callees:** `00200dc0`, `00203990`, `002477a8`, `00247bf0`, `00247e38`, `00258e50`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00248398 UpdateCarouselConfirmDialog

**Description:** Task update function displaying a two-option confirmation dialog in the carousel menu and handling confirmation [inferred]

**Callers:** `002455c0`  
**Callees:** `00203990`, `002058a8`, `00205c08`, `00205c20`, `0022edb8`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00248580 UpdateCarouselPlayerSelect

**Description:** Task update function for carousel player/entry selection menu, managing 1P/2P choices and spawning sub-tasks [inferred]

**Callers:** `002455c0`  
**Callees:** `00203990`, `002058a8`, `00205c08`, `00205c20`, `0020ad18`, `0022c280`, `0023db00`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00248A70 UpdateCarouselPromptDialog

**Description:** Task update function that spawns a prompt dialog task (0x239150) and handles return result or cancellation [inferred]

**Callers:** `002455c0`  
**Callees:** `00203990`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** `0x239150`

## 0x00248BF0 UpdateCarouselSubMenu

**Description:** Task update function for a 3-item carousel sub-menu, handling item navigation, sound effects, and task spawning [inferred]

**Callers:** `002455c0`  
**Callees:** `00203990`, `002058a8`, `00205ad0`, `00205c08`, `00205c20`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

# Race Results, Rankings & Licenses

Race result persistence, rankings, scorecards, awards, licenses, and post-race messaging.

## 0x002084D0 TestMeshIntersection

**Description:** Tests candidate triangles and updates the closest intersection result.

**Callers:** `0x002080d8`  
**Callees:** `0x00206f28`
**Key globals / structures:** None identified in the inventory note.

## 0x0021D310 VecSubtractAndLength

**Description:** Subtracts two f15 fixed-point vectors and stores 3D Euclidean length of the difference in result vector's w component [inferred]

**Callers:** `0x00217830`, `0x002179D8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022DDD0 raceHudTask

**Description:** Maintained symbol-map entry: race HUD task: allocates 0x8CB0 B from the extra buffer, spawns minimap and music; inferred

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022EE70 raceHudElementA

**Description:** Maintained symbol-map entry: HUD element builder (arena records); inferred

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022F338 raceHudItemPanel

**Description:** Maintained symbol-map entry: HUD item panel: 8 slots

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022F750 raceHudElementC

**Description:** Maintained symbol-map entry: HUD element builder

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0022FE50 raceHudElementD

**Description:** Maintained symbol-map entry: HUD element builder

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00230520 raceHudElementE

**Description:** Maintained symbol-map entry: HUD element builder

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00230C20 raceHudElementF

**Description:** Maintained symbol-map entry: HUD element builder

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00231208 raceHudElementG

**Description:** Maintained symbol-map entry: HUD element builder

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002318B0 raceHudElementH

**Description:** Maintained symbol-map entry: HUD element builder

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00231C90 raceHudElementI

**Description:** Maintained symbol-map entry: HUD element builder

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00231F28 raceHudElementJ

**Description:** Maintained symbol-map entry: HUD element builder

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002321B8 raceHudInit

**Description:** Maintained symbol-map entry: initialises the HUD element states (one or two players)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00232C18 raceHudUpdate

**Description:** Maintained symbol-map entry: per-player HUD update

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00233860 raceHudDraw

**Description:** Maintained symbol-map entry: rebuilds the HUD arena from its element builders and submits it

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00235418 InitObjectType1F

**Description:** Clears a 0x8cb0-byte game object and sets its type tag at +0x8008 to 0x1f, returning memset's result. [inferred]

**Callers:** `0x0025e630`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002359E8 DrawStandingsEntry

**Description:** Draws one two-column result entry (rank icon plus values) for row (arg2,arg3), picking the string table from the +0x22 type. [inferred]

**Callers:** `0x00235b58`  
**Callees:** `0x00201e48`, `0x00203b78`, `0x00203fc8`, `0x0023db00`
**Key globals / structures:** None identified in the inventory note.

## 0x00235F10 DrawPlayerResults

**Description:** Draws one player's result block: rank/course labels plus time and score values read from the per-course tables. [inferred]

**Callers:** `0x002364b0`  
**Callees:** `0x00203b78`, `0x00203fc8`, `0x00204310`, `0x0023db00`
**Key globals / structures:** None identified in the inventory note.

## 0x00237C48 UpdateRaceRecords

**Description:** Updates the per-course/per-player best-time and best-score tables from the current result and prints "Player 1/2 WIN!!". [inferred]

**Callers:** `0x0020f3b8`, `0x00235b58`, `0x002364b0`  
**Callees:** `0x0023d8c8`, `0x0023dcf8`, `0x0027ea30`
**Key globals / structures:** None identified in the inventory note.

## 0x00243468 DispatchByIndex16

**Description:** Jump-table dispatch: for index<16 calls function pointer from PTR_FUN_002f1240 and returns its result, else 0 [inferred]

**Callers:** `0022c8a0`, `00239f68`, `00244f60`, `00246250`, `00268428`, `00268890`  
**Callees:** —
**Key globals / structures:** `PTR_FUN_002f1240`

## 0x00243670 DispatchByIndex15

**Description:** Jump-table dispatch: for index<15 calls function pointer from PTR_FUN_002f1280 and returns result, else 0 [inferred]

**Callers:** `0023bf28`, `0023c150`, `00243a78`, `00246250`, `00268428`, `00268890`, `00268e40`, `00269568`, `00269900`  
**Callees:** —
**Key globals / structures:** `PTR_FUN_002f1280`

## 0x002495F0 FormatRaceTimeBCD

**Description:** Converts tick timer at offset 0x14 into packed MM:SS digit format with colon separator [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0026BC08 UNKNOWN_0x0026BC08

**Description:** Looks up the pending menu result and resolves the current action when a button press confirms the choice.

**Callers:** `0020f3b8`, `0026d020`  
**Callees:** `002058a8`, `00205c08`, `00205c20`, `002080f8`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

# Mini-games: Golf, Stunts, Sports & Slots

Golf physics and scoring plus stunt, sport, slot-machine, chase, and goal mini-game state machines.

## 0x00207F38 WrapCoordinate

**Description:** Wraps a floating-point coordinate across configured lower and upper bounds.

**Callers:** `0x0022a408`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0025A120 UNKNOWN_0x0025A120

**Description:** Stunt-card task: state machine driving a stunt list (per-stamp awards, line passes, fade to results), drives menu/scissor/music/draw helpers

**Callers:** —  
**Callees:** `00204d60`, `002080f8`, `0020df50`, `0021c928`, `00233158`, `00233860`, `00233fc8`, `00234128`, `00234c80`, `00235050`, `0023d8c8`, `00242d20`, `00243398`, `0024fb38`, `0024fb80`, `00258e50`, `00258fb0`, `00258ff0`, `0025aa58`, `0025b500`, `00242a88`, `00258ec8`, `0023c8a8`, `00204de8`, `00204c48`, `0027e840`, `002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x0025AB28 UNKNOWN_0x0025AB28

**Description:** Stunt-select menu renderer: builds a centered 5-row text list with left/right accept/cancel, draws header/icons and drives input -> selection state

**Callers:** `0025a120`  
**Callees:** `00202350`, `00203b78`, `002058a8`, `00205c08`, `00205c20`, `0024f530`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `0024f928`, `00258e50`, `0027eee0`, `00204de8`, `0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x0025B0C0 UNKNOWN_0x0025B0C0

**Description:** Stunt results/HUD screen: renders title, per-stamp stats (time/score, bests, new-record markers) with button-input confirmation handling

**Callers:** `0025a120`  
**Callees:** `00202350`, `00203b78`, `0024f530`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `0024f928`, `00258e50`, `0027eee0`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0025B5A0 UNKNOWN_0x0025B5A0

**Description:** Draws name-entry grid: header strip + small alphabet/character grid (with wrap rules) at layout y depending on selection mode

**Callers:** `0025bcf0`, `0025be30`  
**Callees:** `00202350`, `00203098`, `00203f40`, `00203fc8`
**Key globals / structures:** None identified in the inventory note.

## 0x0025B798 UNKNOWN_0x0025B798

**Description:** Name-entry input: handles accept/cancel + per-letter commit; advances slot when full and transitions to confirm state, plays SFX

**Callers:** `0025bcf0`  
**Callees:** `00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x0025B908 UNKNOWN_0x0025B908

**Description:** Name-entry letter D-pad navigation: translates D-pad axes to neighbor letter (with edge wrap) using directional adjacency, plays cursor SFX

**Callers:** `0025bcf0`  
**Callees:** `00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x0025BB08 UNKNOWN_0x0025BB08

**Description:** Renders the committed-name preview list under the character grid: title row + per-slot name/dash, centered within a frame

**Callers:** `0025bcf0`, `0025be30`  
**Callees:** `00202350`, `00203f40`, `00203fc8`, `0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x0025BCF0 UNKNOWN_0x0025BCF0

**Description:** Name-entry controller task: ticks per-frame letter navigation/typing, eventually concatenates name into a buffer and stores it on the parent

**Callers:** `0023be18`, `0026b848`, `0026d550`, `0026d5e0`  
**Callees:** `00203240`, `00203990`, `0025b5a0`, `0025b798`, `0025b908`, `0025bb08`, `0027ef70`
**Key globals / structures:** None identified in the inventory note.

## 0x0025BE30 UNKNOWN_0x0025BE30

**Description:** Name-entry exit/confirm task: fades highlight, ends input when slot=0x10, kills self and notifies parent on completion

**Callers:** `0025bcf0`  
**Callees:** `00203990`, `0025b5a0`, `0025bb08`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0025BE98 UNKNOWN_0x0025BE98

**Description:** Renders an animated map prop (misc-model) with a VIF chain and triggers a pickup SFX + counter increment when player is within range

**Callers:** `0025d028`  
**Callees:** `00221288`, `0023d2c0`, `0023d488`, `0023d628`, `00258e50`, `002727c0`, `00272958`, `00274b10`, `00227048`, `00226d58`, `00226b18`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025C300 UNKNOWN_0x0025C088

**Description:** Draws two props gated by slot IDs (DAT_002c4190/4) and FUN_0023d628(9); thresholded (1.5 vs 2.0) proximity awards different SFX/counters

**Callers:** —  
**Callees:** `00221288`, `0023d2c0`, `0023d488`, `0023d628`, `00258e50`, `002727c0`, `00272958`, `00274b10`, `00227048`, `00226d58`, `00226b18`, `00272a48`
**Key globals / structures:** `DAT_002c4190`

## 0x0025C528 UNKNOWN_0x0025C528

**Description:** Renders three prop slots (id 0x2c/0x24/0x2a) gated by FUN_0023d488/FUN_0023d628 with thresholds 2.5/2.5/1.5 and matching counter/SFX on proximity

**Callers:** —  
**Callees:** `00221288`, `0023d2c0`, `0023d488`, `0023d628`, `00258e50`, `002727c0`, `00272958`, `00274b10`, `00227048`, `00226d58`, `00226b18`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025C818 UNKNOWN_0x0025C818

**Description:** Draws a model via MISC slot and conditionally a second prop gated by world-frame timer; the second prop's rotation tracks game frame counter

**Callers:** —  
**Callees:** `00221288`, `0023d2c0`, `0023d488`, `0023d628`, `00258e50`, `002727c0`, `00272958`, `00272c38`, `00274b10`, `00227048`, `00226d58`, `00226b18`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025CA08 UNKNOWN_0x0025CA08

**Description:** Renders two prop slots (0x28/0x20) gated by FUN_0023d488 + FUN_0023d628(0x27/9) with 1.5-unit proximity thresholds and shared SFX/counter

**Callers:** —  
**Callees:** `00221288`, `0023d2c0`, `0023d488`, `0023d628`, `00258e50`, `002727c0`, `00272958`, `00274b10`, `00227048`, `00226d58`, `00226b18`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025CC18 UNKNOWN_0x0025CC18

**Description:** Batch-renders up to 0x6b sets of three animated props each: per-prop angle tick +/- DAT_003330d4/30 with directional wrap, fetches from fieldMiscModels table

**Callers:** —  
**Callees:** `00221288`, `00272b90`, `00227048`, `00226d58`, `00226b18`, `00272a48`
**Key globals / structures:** `DAT_003330d4`

## 0x0025CE20 UNKNOWN_0x0025CE20

**Description:** Batch-renders up to 6 sets of three props from DAT_0177f5b0 area, per-prop angle tick +/- DAT_003330d8/30 with directional wrap, model index varies per slot

**Callers:** —  
**Callees:** `00221288`, `00272b90`, `00227048`, `00226d58`, `00226b18`, `00272a48`
**Key globals / structures:** `DAT_0177f5b0`, `DAT_003330d8`

## 0x0025D028 drawAnimatedProps

**Description:** Maintained symbol-map entry: dispatch over 0x2C43B0 to the prop renderers 0x25BE98..0x25D5F8

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x2C43B0`, `0x25BE98`, `0x25D5F8`

## 0x0025D2C8 UNKNOWN_0x0025D2C8

**Description:** One-shot VU0 init: sets vifFixedRegs default and writes a unit matrix plus an object pose from constant 0x2a2bd0 into globals 0x177ebd0/0x177ec00

**Callers:** —  
**Callees:** `00221288`, `00272a48`
**Key globals / structures:** `0x2a2bd0`, `0x177ebd0`, `0x177ec00`

## 0x0025D328 UNKNOWN_0x0025D328

**Description:** Uploads a colour table and conditionally renders one model (DAT_0177f5b0) via drawModelParts when DAT_00335738 is in (6,7) and FUN_0023d778(0x50) is clear

**Callers:** —  
**Callees:** `00221288`, `0023d778`, `002234c8`, `00226d58`, `00226b18`, `00226a18`, `00272a48`
**Key globals / structures:** `DAT_0177f5b0`, `DAT_00335738`

## 0x0025D3D8 UNKNOWN_0x0025D3D8

**Description:** Slot-0x23 prop renderer: gated by FUN_0023d488 + FUN_0023d628; on 1.5-unit proximity awards SFX and tickles counter; writes VU0 unit matrix + pose to globals

**Callers:** —  
**Callees:** `00221288`, `0023d2c0`, `0023d488`, `0023d628`, `00258e50`, `002727c0`, `00272958`, `00274b10`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025D4E8 UNKNOWN_0x0025D4E8

**Description:** Slot-0x21 prop renderer: gated by FUN_0023d488 + FUN_0023d628; on 1.5-unit proximity awards SFX and tickles counter; writes VU0 unit matrix + pose to globals

**Callers:** —  
**Callees:** `00221288`, `0023d2c0`, `0023d488`, `0023d628`, `00258e50`, `002727c0`, `00272958`, `00274b10`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025D5F8 UNKNOWN_0x0025D5F8

**Description:** Renders 0x16 copies of a prop (DAT_0177f5c8) with periodic Y rotations from three modulos (180/720/360 deg) using DAT_003330dc, advancing per frame

**Callers:** —  
**Callees:** `00221288`, `00272ae8`, `00227048`, `00226d58`, `00226b18`, `00272a48`
**Key globals / structures:** `DAT_0177f5c8`, `DAT_003330dc`

## 0x0025D7C8 UNKNOWN_0x0025D7C8

**Description:** Per-stamp NPC update: drives short-angle target seek on input, copies scratch pose to per-slot globals; sets 'done' flag + parent counter on proximity

**Callers:** `0025dbb8`  
**Callees:** `00207ec8`, `00258e50`, `002727c0`, `002727e8`, `00272940`, `00272958`, `00272988`, `00272c38`, `00273268`, `00274938`, `00274a10`, `00274b10`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025DBB8 UNKNOWN_0x0025DBB8

**Description:** Stamp-collection minigame task: spawns 0x1e NPC updaters, advances a frame timer with a 0x14-tick threshold, fades and finishes on completion

**Callers:** —  
**Callees:** `002080d8`, `002080f8`, `0020df50`, `00233860`, `00234128`, `002356d0`, `00235718`, `0023d8c8`, `00258ff0`, `0027af60`, `0027afd8`, `0027b540`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x0025DF80 UNKNOWN_0x0025DF80

**Description:** Single balloon/NPC AI: 3-state vertical/horizontal motion over 0..880 range with resets, awards goal/MISS via parent + FUN_0027afd8, and yaw-rotates VU0 pose

**Callers:** `0025e278`  
**Callees:** `002727c0`, `00272958`, `00272b90`, `00274b10`, `0027afd8`, `002729d0`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025E278 UNKNOWN_0x0025E278

**Description:** Balloon-pop minigame task: spawns 0x20 updaters, watches gs_race_cars_address vs DAT_0033311c (Y) to score GOAL/MISS, fades to finish, prints "GOAL"/"MISS"

**Callers:** —  
**Callees:** `002080d8`, `002080f8`, `0020df50`, `00233860`, `00234708`, `002349c8`, `002355a8`, `0023d8c8`, `00258e50`, `00258ff0`, `0027af60`, `0027afd8`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`, `0027ea30`
**Key globals / structures:** `gs_race_cars_address`, `DAT_0033311c`

## 0x0025E630 UNKNOWN_0x0025E630

**Description:** Goal-plane minigame task: uses 4-side polygon overlap test (plane cross-product) vs player pos, fails if Y<=100 or gs_race_cars_address<=110, then fades out

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `00233860`, `00233fc8`, `00234708`, `002349c8`, `00235418`, `0023d628`, `0023d8c8`, `00258e50`, `00258ff0`, `002729d0`, `00272b90`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`, `00272a48`
**Key globals / structures:** `gs_race_cars_address`

## 0x0025EB70 UNKNOWN_0x0025EB70

**Description:** Tug-of-war / chase minigame: dual-perspective state machine (player+other) with checkpoint polygons (0x2c4f00..), gain-flag wins/losses, fade-out and opponent penalty

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `0021c928`, `0022ea78`, `0022eaa0`, `00232d68`, `00232f08`, `00233860`, `00233fc8`, `00234128`, `00235378`, `0023d8c8`, `0024fb80`, `00258fb0`, `00258ff0`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`
**Key globals / structures:** `0x2c4f00`

## 0x0025F6A0 UNKNOWN_0x0025F6A0

**Description:** Finish-zone detection task: 4-edge polygon overlap test, fail-out on y<=100 and speed<=110, sets fail flag and decays scale/model pose globals per-frame

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `00233860`, `00233fc8`, `00234708`, `00234d10`, `0023d628`, `0023d8c8`, `00258e50`, `00258ff0`, `00272ae8`, `00272c38`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`, `002729d0`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0025FBF8 UNKNOWN_0x0025FBF8

**Description:** Finish-zone (variant) task: 2-triangle polygon tests trigger result states (set kind=6), and fade-to-finish with counter increment on cross

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `00233860`, `00233fc8`, `002348d8`, `00234e08`, `0023d628`, `0023d8c8`, `00258ff0`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x0025FF00 UNKNOWN_0x0025FF00

**Description:** Dual-camera split minigame task: both views transition on DAT_0177ae9a/0xb10a vs thresholds, fades with per-view fade counters, writes kind flags for outcome

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `0021c928`, `00232d68`, `00232f08`, `00233158`, `00233860`, `00233fc8`, `00234db0`, `0023d8c8`, `00258ff0`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`
**Key globals / structures:** `DAT_0177ae9a`

## 0x00260638 UNKNOWN_0x00260638

**Description:** Goal-area test task: bounds vs (X,Y) inside (410..430,370) or >DAT_00333170 Y -> result states (kind 0/2/3) with side-set, fade-out + counters

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `0021c928`, `00232d68`, `00232f08`, `00233860`, `00233fc8`, `00234ab0`, `00234e50`, `0023d8c8`, `00258ff0`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`
**Key globals / structures:** `DAT_00333170`

## 0x002609D0 UNKNOWN_0x002609D0

**Description:** Per-frame camera/world-pose update: integrates a yaw using accumulated short counter scaled by DAT_00333178, computes two VU0 basis frames and stores them to globals

**Callers:** `00260dc0`  
**Callees:** `00207f88`, `002727e8`, `00272988`, `00272ce0`, `00273268`, `0027e840`, `00272728`, `002729e0`, `00272a48`
**Key globals / structures:** `DAT_00333178`

## 0x00260B48 UNKNOWN_0x00260B48

**Description:** Slot angular control: smooths an angle toward a target distance (~15), chooses +/-10 step from angular error vs player (gs_race_cars_address,DAT_0177ace8) and sets CAM at world+400

**Callers:** `00260dc0`  
**Callees:** `0021c928`, `002727c0`, `002727e8`, `00272958`, `00272988`, `00272ce0`, `00274af8`, `00274b10`, `00272728`, `00272a48`
**Key globals / structures:** `gs_race_cars_address`, `DAT_0177ace8`

## 0x00260DC0 UNKNOWN_0x00260DC0

**Description:** Slot-machine minigame task: states 0..9 driving slot reel, camera/world updates, lever-pull+result, opponent settle and finish fade [inferred]

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `0021c928`, `00232d68`, `00232f08`, `00233860`, `00233ef0`, `00235008`, `0024fb38`, `0024fb80`, `00258f30`, `002609d0`, `00260b48`, `00262328`, `00262358`, `00262458`, `002727c0`, `00272958`, `00274af8`, `00274b10`, `0027afd8`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00261378 UNKNOWN_0x00261378

**Description:** Menu-list cursor resolver: maps D-pad axes to next menu index using per-row neighbor table at DAT_002d9984..0x2d9994; plays SFX on change [inferred]

**Callers:** `00261ac8`  
**Callees:** `00258e50`
**Key globals / structures:** `DAT_002d9984`, `0x2d9994`

## 0x00261508 UNKNOWN_0x00261508

**Description:** Yes/No confirmation task: lays out a small text panel with two options, awaits FUN_00205c08; returns chosen index to parent and kills self [inferred]

**Callers:** `00261638`  
**Callees:** `002058a8`, `00205c08`, `00205c20`, `00258e50`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00261638 UNKNOWN_0x00261638

**Description:** Bet-amount selection task: arrow keys step a clamped credit value by a power-of-10 step; X confirms -> spawns FUN_00261508; draws amount + range indicator [inferred]

**Callers:** `00261ac8`  
**Callees:** `00202350`, `00203e70`, `00203fc8`, `002046c0`, `00206d28`, `0022e7f0`, `0023dc58`, `0024f4e0`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00261AC8 UNKNOWN_0x00261AC8

**Description:** Shop/menu flow task: setup viewport/graphics, then state machine (intro -> bet select -> confirm -> result -> fade) calling helpers 0x261378,0x261638,0x2624b8 [inferred]

**Callers:** `00260dc0`  
**Callees:** `002080f8`, `0022e7c0`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `00258e50`, `00258fb0`, `00261378`, `00204de8`, `00204c48`, `0027e840`
**Key globals / structures:** `0x261378`, `0x261638`, `0x2624b8`

## 0x00261DE0 UNKNOWN_0x00261DE0

**Description:** Results/payout screen: draws history rows + final amount; on cross button calls FUN_0023dc58 to credit funds and notifies parent with code 2 [inferred]

**Callers:** `00260dc0`  
**Callees:** `002080f8`, `0022e7f0`, `0023dc58`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `00258e50`, `00204de8`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00262358 UNKNOWN_0x00262358

**Description:** Updates the slot-state result and stores the current/next reel index and win flag for the machine state.

**Callers:** `00260dc0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00262458 UNKNOWN_0x00262458

**Description:** Compares current credits to target and triggers the win/lose result signal for the slot outcome.

**Callers:** `00260dc0`  
**Callees:** `0023d8c8`
**Key globals / structures:** None identified in the inventory note.

## 0x002624B8 UNKNOWN_0x002624B8

**Description:** Confirms a bet choice and resolves the selection callback when the confirm button is accepted.

**Callers:** `00261ac8`  
**Callees:** `002058a8`, `00205c08`, `00205c20`, `00258e50`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x002625A0 UNKNOWN_0x002625A0

**Description:** Handles the no/deny decision path for the bet dialog and closes it on confirmation.

**Callers:** `00261638`  
**Callees:** `002058a8`, `00205c08`, `00205c20`, `00258e50`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x00262678 texAnimCopyB

**Description:** Maintained symbol-map entry: same with the template 0x2D9F50 (3 copies: NLOOP 0xC)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x2D9F50`

## 0x002631A0 UNKNOWN_0x002631A0

**Description:** Runs the race-course minigame flow: intro, timer/watch, stage transitions, and finish cleanup.

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `0021c928`, `0021c998`, `0021d358`, `00233860`, `00234618`, `002347f0`, `00235460`, `002354d8`, `002354f0`, `00235558`, `00235570`, `0023d628`, `0023d8c8`, `00258f30`, `00258fb0`, `00258ff0`, `002727c0`, `00272958`, `00274b10`, `00258ec8`, `00204de8`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00263710 SeqIntroFadeTask

**Description:** Task initing 0x8CBC work buffer, plays BGM flag 0xA100201, state machine ramps a short via FUN_002080F8 then kills task on done. [inferred]

**Callers:** —  
**Callees:** `0x002080f8`, `0x0020df50`, `0x0021c928`, `0x00233860`, `0x002355f0`, `0x00258ff0`, `0x00258ec8`, `0x00204de8`, `0x00204c48`, `0x0027e840`
**Key globals / structures:** `0xA100201`

## 0x002638C0 UNKNOWN_0x002638C0

**Description:** Runs a chase/goal minigame with a short setup sequence and fade-out when the target is reached.

**Callers:** —  
**Callees:** `002080f8`, `0020df50`, `0021c928`, `00232d68`, `00232f08`, `00233860`, `00235730`, `0023d8c8`, `00258e50`, `00258ff0`, `002727c0`, `00272958`, `00274b10`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`, `002729d0`, `00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x00263C38 UNKNOWN_0x00263C38

**Description:** Handles a short stage loop that checks distance thresholds and exits on completion with a kill signal.

**Callers:** `0022ddd0`  
**Callees:** `002080f8`, `0020df50`, `0021c928`, `0023d8c8`, `00204de8`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00263E68 UNKNOWN_0x00263E68

**Description:** Drives a target/goal loop that advances the state, updates the motion target, and exits on finish.

**Callers:** —  
**Callees:** `002080d8`, `002080f8`, `0020df50`, `0021c928`, `0021d358`, `00233860`, `00234128`, `002347f0`, `00235638`, `00235680`, `00235698`, `0023d628`, `0023d8c8`, `00258ff0`, `00258ec8`, `00204de8`, `00204c48`, `0027e840`, `002729d0`
**Key globals / structures:** None identified in the inventory note.

## 0x002642B8 SeqLongFadeTask

**Description:** Task inits 0x8CC0 buffer + BGM, state machine ramps 2 values (states 0-4), sets screen fade via FUN_002080F8, kills itself. [inferred]

**Callers:** —  
**Callees:** `0x002080f8`, `0x0020df50`, `0x0021c928`, `0x00233860`, `0x00235320`, `0x00258ec8`, `0x00204de8`, `0x00204c48`, `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00264498 UNKNOWN_0x00264498

**Description:** Draws the course-selection menu and list of entry names with keyboard-style selection and exit on confirm.

**Callers:** `0022cd38`  
**Callees:** `00202350`, `00203f40`, `00203fc8`, `00205350`, `00205c20`, `00205da0`, `0023db00`, `0023df00`, `0023dfe0`, `0023dff8`, `00258e50`, `0027eee0`, `00204de8`, `0027e840`, `0027ef70`, `0027f1e0`, `0027f2f4`
**Key globals / structures:** None identified in the inventory note.

## 0x00264CB0 SubstituteNameTokens

**Description:** '$N' substitution: grabs name from FUN_0023DB00(0,0) and replaces the $N token in caller's text buffer. [inferred]

**Callers:** —  
**Callees:** `0x0023db00`, `0x0027ef70`, `0x0027f1e0`
**Key globals / structures:** None identified in the inventory note.

## 0x00264D50 UNKNOWN_0x00264D50

**Description:** Applies a perspective warp to the map marker overlay for the current wheel/slider value.

**Callers:** `00265568`, `00265b30`, `002665b8`, `00267248`  
**Callees:** `0024eb10`, `00274938`, `00274a10`
**Key globals / structures:** None identified in the inventory note.

## 0x00264FF0 UNKNOWN_0x00264FF0

**Description:** Renders a small row of value tiles for the current selection state without changing the underlying state.

**Callers:** `00265568`, `00265b30`, `002665b8`, `00267248`  
**Callees:** `0024f728`, `0024f7c8`, `0024f928`
**Key globals / structures:** None identified in the inventory note.

## 0x00265128 UNKNOWN_0x00265128

**Description:** Builds a stretched icon/beam sprite for the current ride counter and pushes it into the render queue.

**Callers:** `002652b8`, `00265568`  
**Callees:** `0024eb10`, `0024f640`, `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002652B8 UNKNOWN_0x002652B8

**Description:** Renders the per-vehicle result banner and writes any visited-house text for the current side.

**Callers:** `00265568`, `00265b30`  
**Callees:** `0023d628`, `0023d6a0`, `0024ea00`, `0024f530`, `0024f640`, `0024f728`, `0024f7c8`, `0024f928`, `00265128`, `0027eee0`, `0027f2f4`
**Key globals / structures:** None identified in the inventory note.

## 0x00265568 HouseVisitedMenuTask

**Description:** Task building 0x1762400 menu inits house/set data, draws visited-house list from PTR table, edits current house/set via dpad, exits on btn. [inferred]

**Callers:** `0x0026d008`  
**Callees:** `0x002080f8`, `0x0023d6e0`, `0x0023d818`, `0x0024ea00`, `0x0024f530`, `0x0024f558`, `0x0024f578`, `0x0024f5c8`, `0x0024f640`, `0x0024f728`, `0x0024f928`, `0x00258e50`, `0x00264d50`, `0x00264ff0`, `0x00265128`, `0x002652b8`, `0x00204de8`, `0x00204c48`
**Key globals / structures:** `0x1762400`

## 0x00265B30 UNKNOWN_0x00265B30

**Description:** Drives a medal/slider wheel with directional motion and spawns the next event when the player accepts.

**Callers:** `0026b848`, `00217148`  
**Callees:** `00206d28`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `0024f928`, `00258e50`, `00264d50`, `00264ff0`, `002652b8`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00265FE8 UNKNOWN_0x00265FE8

**Description:** Adjusts a digit by a directional button increment while enforcing wrap-around and sound-on-change logic.

**Callers:** `002665b8`  
**Callees:** `00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x002660F8 UNKNOWN_0x002660F8

**Description:** Draws the per-digit score column and secondary value tiles for the current rank/score state.

**Callers:** `002665b8`  
**Callees:** `0023d8e8`, `0024eb10`, `0024f4a0`, `0024f4e0`, `0024f640`, `0024f728`, `0024f7c8`
**Key globals / structures:** None identified in the inventory note.

## 0x002665B8 UNKNOWN_0x002665B8

**Description:** Runs the final score/wheel state machine, updating the displayed total and spawning the next view.

**Callers:** `0026b848`, `00217148`  
**Callees:** `0023d8e8`, `0024f148`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `0024f928`, `00258e50`, `00264d50`, `00264ff0`, `00265fe8`, `002660f8`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x00266B78 UNKNOWN_0x00266B78

**Description:** Prints the timer/elapsed value for the score banner when the record flag is enabled.

**Callers:** `00267248`  
**Callees:** `00203fc8`, `0027eee0`, `0027f2f4`
**Key globals / structures:** None identified in the inventory note.

## 0x00266CE8 UNKNOWN_0x00266CE8

**Description:** Draws the ski-jump name/value label and its score line for the active course result.

**Callers:** `00267248`  
**Callees:** `00203fc8`, `0027eee0`, `0027f2f4`
**Key globals / structures:** None identified in the inventory note.

## 0x00266DF8 UNKNOWN_0x00266DF8

**Description:** Draws the curling score label and current value text for the active game state.

**Callers:** `00267248`  
**Callees:** `00203fc8`, `0027eee0`, `0027f2f4`
**Key globals / structures:** None identified in the inventory note.

## 0x00266F10 UNKNOWN_0x00266F10

**Description:** Draws the figure-eight scoring block and the helper labels associated with it.

**Callers:** `00267248`  
**Callees:** `00203b78`, `00203fc8`, `0027eee0`
**Key globals / structures:** None identified in the inventory note.

## 0x00267060 UNKNOWN_0x00267060

**Description:** Prints the per-row total values for the discipline leaderboard and aligns them in the HUD.

**Callers:** `00267248`  
**Callees:** `00203fc8`, `0027eee0`, `0027f2f4`
**Key globals / structures:** None identified in the inventory note.

## 0x00267248 UNKNOWN_0x00267248

**Description:** Drives the sport-score display and spawns the right result panel for the final ranking state.

**Callers:** `0026b848`, `00217148`  
**Callees:** `00206d28`, `00238a48`, `0024f558`, `0024f578`, `0024f5c8`, `0024f640`, `0024f728`, `0024f7c8`, `0024f928`, `00258e50`, `00264d50`, `00264ff0`, `00266b78`, `00266ce8`, `00266df8`, `00266f10`, `00267060`, `00204de8`
**Key globals / structures:** None identified in the inventory note.

# Debug Text & Diagnostics

Developer-facing formatted text dumps, debug grids, state inspection, and diagnostic overlays.

## 0x0020DCD8 debugScreenRead

**Description:** Maintained symbol-map entry: sceGsExecStoreImage of a whole frame buffer (debug 'Screen Save'; no live callers)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020DDE0 loadFontTextures

**Description:** Maintained symbol-map entry: reads SYS table entry 0 (FONT/FONTE.GSL) and sends its chain with TBP + 0x1A40

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020DF50 getExtraPacketArea

**Description:** Maintained symbol-map entry: returns 0x15A7B80 (fatal above 0x180000 bytes)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x15A7B80`, `0x180000`

## 0x0020E1F0 cdReadEntry

**Description:** Maintained symbol-map entry: starts sceCdRead of a {mode, lsn, sectors} table entry to dest (async; 0x20E440 polls)

**Callers:** —  
**Callees:** —
**Key globals / structures:** `0x20E440`

## 0x0020E250 cdReadEntryPart

**Description:** Maintained symbol-map entry: as cdReadEntry for a byte range of the entry (offset, size)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020E318 cdReadEntryWait

**Description:** Maintained symbol-map entry: cdReadEntry that waits (sceGsSyncV loop) until the read is done

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020E440 cdReadPoll

**Description:** Maintained symbol-map entry: polls the pending sceCdRead; 0 when done

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020E500 debugScreenSave

**Description:** Maintained symbol-map entry: writes the capture to a file (debug; no callers)

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0026D880 PrintDoorParams

**Description:** Decodes door struct: pos(0-2),scale with 32FB48 label, ypr shorts *0xB4/0x8000 deg, prints via FUN_0027EEE0/FUN_00203A50. [inferred]

**Callers:** `0x0021f198`  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** None identified in the inventory note.

## 0x0026DA90 PrintDoorBase

**Description:** Dumps base struct to text: d 0x335630 = arg2, plus floats off param_1+0x30/0x34/0x38. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** `0x335630`

## 0x0026DB80 PrintCamPair

**Description:** Converts two floats via FUN_0027B7B0/FUN_0027EEE0 into text api calls 0x32FB78/0x32FB88 (camera near/far?). [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** `0x32FB78`, `0x32FB88`

## 0x0026DC10 PrintCamProj

**Description:** Dumps arg2 via 32FB98 plus param_1 via 0x335668 with size 0x20 - camera aspect/fov debug print. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** `0x335668`

## 0x0026DC98 PrintUiSlotN

**Description:** Sets UI text api slot: id param_1 via a0 335670 plus current via 0x33xx label, prints with size param_1<<3. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** None identified in the inventory note.

## 0x0026DCF8 PrintXyzRow

**Description:** Converts 4 args (id + x/y/z or w) via FUN_0027B7B0 into one 0x32FBA8 text api record of size 0x50. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** `0x32FBA8`

## 0x0026DDB0 PrintXyzTable

**Description:** Rows table (stride 0x30) at param_1: for each of 4 rows prints 4 floats as one 0x32FBA8 record at base param_2<<3. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** `0x32FBA8`

## 0x0026DE80 DrawMapCoordinateValues

**Description:** Formats 4 vector words via FUN_0027b7b0 into template 0x32fbb8 and draws at slot 0x10 row param_2*8, used by map draw fns. [inferred]

**Callers:** `0x002427D0`, `0x00242A88`  
**Callees:** `0x00203A50`, `0x0027B7B0`, `0x0027EEE0`
**Key globals / structures:** `0x32fbb8`

## 0x0026DF30 PrintVec2Short

**Description:** Prints 2 shorts from param_1 as one 0x32FBD0 record of size 0x10 at param_2<<3. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** `0x32FBD0`

## 0x0026DFB0 PrintVec3Norm

**Description:** Writes 0x32FC08 (x,y,z) or normalized 0x32FBE0 (x,y,z,x/z,y/z) record for vec at param_1, size 0x10 at param_2<<3. [inferred]

**Callers:** —  
**Callees:** `0x00203a50`, `0x0027b7b0`, `0x0027eee0`
**Key globals / structures:** `0x32FC08`, `0x32FBE0`

# Math, Tables, Strings & Utility Helpers

General vector/math, table dispatch, bounded strings, token parsing, BCD, bit counts, and small utility routines.

## 0x00207B20 CountSetBits

**Description:** Returns the population count of a 32-bit value.

**Callers:** `0x0023c150`, `0x0023cb68`, `0x00246250`, `0x002455c0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00208080 ApplyMatrixRotation

**Description:** Builds a rotation matrix and multiplies it into the supplied matrix.

**Callers:** `0x0021b3d0`, `0x0021dc18`  
**Callees:** `0x00272c38`, `0x00272758`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x00219CB0 TestQuadLateralMask

**Description:** Per quad offsets verts by plane origin and rotates to car frame; returns bitmask 1/2/4/8 of lateral band when height/z tests pass [inferred]

**Callers:** `0x00219ED8`  
**Callees:** `0x00272728`
**Key globals / structures:** None identified in the inventory note.

## 0x00219ED8 QueryCarSurfaces

**Description:** Rotates by car yaw, bitmask-tests the car's surface quad list via 0x219cb0; special type loops 26 extra parts [inferred]

**Callers:** `0x0021BA70`  
**Callees:** `0x00219CB0`, `0x0023DE30`, `0x00272C38`, `0x00272A48`
**Key globals / structures:** `0x219cb0`

## 0x0021AF28 UpdateWheelSuspension

**Description:** Per wheel compares contact height vs travel (+0x114), updates spring/damping, starts rumble; returns contact bits [inferred]

**Callers:** `0x0021B3D0`  
**Callees:** `0x0020ACB8`
**Key globals / structures:** None identified in the inventory note.

## 0x0021C6A0 DecomposeWorldPosition

**Description:** Normalizes packed fixed-point world pos into in-cell range, writes carry/base to out ptrs, returns grid sector index [inferred]

**Callers:** `0x002105B8`, `0x0026CA48`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021C728 ToRelativeVec4

**Description:** Builds float vec4(x,y,z,1) from fixed-point position minus reference origin, wraps at 27 bits, scales by world-unit constant DAT_00332c1c

**Callers:** `0x00222B70`, `0x002228A8`  
**Callees:** —
**Key globals / structures:** `DAT_00332c1c`

## 0x0021C7F0 FixedPosToFloatVec4

**Description:** Converts packed fixed-point position struct (x, y, z+carry bit) to float vec4 with w=1.0 using scale DAT_00332c20

**Callers:** `0x0021E690`, `0x0021EC58`  
**Callees:** —
**Key globals / structures:** `DAT_00332c20`

## 0x0021C880 GetGridCellIndex

**Description:** Returns 2-bit grid cell selector (0-3) from sign bits of fixed-point x and z fields of a position struct

**Callers:** `0x0021E690`, `0x00220938`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021D358 GetSpeedFromF15Vector

**Description:** Converts f15 fixed-point 3D vector to float and returns its length scaled by 216.0 (car speed in display units) [inferred]

**Callers:** `0x0021BA70`, `0x002249B8`, `0x00224CD0`, `0x002406F8`, `0x00241F68`, `0x00249458`, `0x002494E8`, `0x00253230`, `0x00253B80`, `0x00259A08`, `0x002631A0`, `0x00263E68`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002283E0 MemcardReadFlushHandler

**Description:** Async sceMcSync handler: branches on size error vs ok; on success dispatches "Close"/"Done" diagnostic strings and notifies caller. [inferred]

**Callers:** `0x00228510`  
**Callees:** `0x00279c78`, `0x0027a1a8`, `0x0027ea30`, `0x00204de8`
**Key globals / structures:** None identified in the inventory note.

## 0x0022A6B8 UpdateAnimationPhaseMachine

**Description:** Drives clip playback via FUN_0022a408, tweens spin counters, plays SE 0x35/0x8035 and advances the phase byte at +0x27. [inferred]

**Callers:** `0x0022aeb0`  
**Callees:** `0x0022a408`, `0x00258e50`, `0x00272c38`, `0x00273268`, `0x002729d0`, `0x00272a48`
**Key globals / structures:** None identified in the inventory note.

## 0x0022D390 RecordLapSplitTimes

**Description:** Stores a lap/split time into per-category record tables, plays SE 0x2e/0x2f for new bests and advances the lap counter.

**Callers:** `0x0022d620`  
**Callees:** `0x00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x0022E7C0 DrawScoreCounter

**Description:** Draws the score counter DAT_0177fdb4 at fixed screen position (0x1b8,0x1a) via FUN_00202d10.

**Callers:** `0x0022aeb0`, `0x00261ac8`  
**Callees:** `0x00202d10`
**Key globals / structures:** `DAT_0177fdb4`

## 0x002357C0 DispatchTypeSetup

**Description:** Switches on object type byte +0x22 and calls FUN_00233a80 with a category/mode; unknown types fall back to FUN_0022dae8. [inferred]

**Callers:** `0x00254d20`, `0x002595f0`, `0x0025a120`, `0x0025dbb8`, `0x0025e278`, `0x0025e630`, `0x0025eb70`, `0x0025f6a0`, `0x0025fbf8`, `0x0025ff00`, `0x00260638`, `0x00262678`, `0x00263710`, `0x002638c0`, `0x00263e68`, `0x002642b8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00236238 SumPlayerPoints

**Description:** Returns the sum of table points (DAT_0029fcb8) for the course byte at +3 across the active players. [inferred]

**Callers:** `0x00237508`  
**Callees:** —
**Key globals / structures:** `DAT_0029fcb8`

## 0x00237080 DrawEntryRowsForState

**Description:** Draws 8-entry rows for a state object, using lookup tables and FUN_00204568/FUN_00204310 draw calls [inferred].

**Callers:** `00237388`  
**Callees:** `00203fc8`, `00204310`, `00204568`, `0022ea08`, `00258e50`
**Key globals / structures:** None identified in the inventory note.

## 0x00237388 DispatchRecordsScreen

**Description:** Inits a 0x1f0-byte record-screen state, fills the DAT_01782a74 table, then dispatches to one of three sub-screens by byte +0x25. [inferred]

**Callers:** `0x0020f3b8`  
**Callees:** `0x00203fc8`, `0x0020df50`, `0x00235880`, `0x00236858`, `0x00236d28`, `0x00237080`, `0x00226f70`
**Key globals / structures:** `DAT_01782a74`

## 0x0023C6B0 FindIndexByMaskedByte

**Description:** Searches a byte table for an entry matching value (param3 mod param2 + base) with mask 0x7f; returns found index [inferred].

**Callers:** `0023dfa0`, `0023dfc0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0023E018 AdvanceOscillatingValue

**Description:** Adjusts a signed 16-bit value through table-defined ranges and toggles direction near its upper bound. [inferred]

**Callers:** `0x0023E9C0`, `0x0023ECA0`, `0x0023F398`, `0x002412E0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002406E8 EmptyStub

**Description:** Returns immediately; no-op.

**Callers:** `00240228`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00241670 EmptyStub2

**Description:** Returns immediately; no-op.

**Callers:** `00241490`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243490 GetTableEntry_ed720

**Description:** Returns table pointer at PTR_002ed720 indexed by param_2 with stride 0x44; param_1 ignored [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** `PTR_002ed720`

## 0x002434B0 GetLabelEntry_eca70

**Description:** Returns string pointer from label table at 002eca70 indexed by param_2 (stride 0x1c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002434D0 GetLabelEntry_eced0

**Description:** Returns string pointer from label table at 002eced0 indexed by param_2 (stride 0x18) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002434F0 GetLabelEntry_ed1f8

**Description:** Returns string pointer from label table at 002ed1f8 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243508 GetLabelEntry_ed328

**Description:** Returns string pointer from label table at 002ed328 indexed by param_2 (stride 0x1c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243528 GetLabelEntry_ed4e8

**Description:** Returns string pointer from label table at 002ed4e8 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243568 GetLabelEntry_effd8

**Description:** Returns string pointer from label table at 002effd8 indexed by param_2 (stride 0x0c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002435A0 GetLabelEntry_f0280

**Description:** Returns pointer from table at 002f0280 indexed by param_2 (stride 0x0c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002435C0 GetLabelEntry_f02f8

**Description:** Returns pointer from table at 002f02f8 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002435D8 GetLabelEntry_f03e0

**Description:** Returns pointer from table at 002f03e0 indexed by param_2 (stride 0x14) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002435F8 GetLabelEntry_f07f8

**Description:** Returns pointer from table at 002f07f8 indexed by param_2 (stride 0x0c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243618 GetLabelEntry_f00a0

**Description:** Returns string pointer from Normal Horn label table at 002f00a0 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243630 GetLabelEntry_f06c0

**Description:** Returns string pointer from Normal Meter label table at 002f06c0 indexed by param_2 (stride 0x0c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243650 GetLabelEntry_f0810

**Description:** Returns string pointer from Stylish Pattern label table at 002f0810 indexed by param_2 (stride 0x08) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243698 GetConst500

**Description:** Returns constant 500

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002436A0 GetIntField_eca78

**Description:** Reads int from table at 002eca78 indexed by param_2 (stride 0x1c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002436C0 GetIntField_eced8

**Description:** Reads int from table at 002eced8 indexed by param_2 (stride 0x18) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002436E0 GetIntField_ed200

**Description:** Reads int from table at 002ed200 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002436F8 GetIntField_ed330

**Description:** Reads int from table at 002ed330 indexed by param_2 (stride 0x1c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243718 GetIntField_ed4f0

**Description:** Reads int from table at 002ed4f0 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243758 GetIntField_effe0

**Description:** Reads int from table at 002effe0 indexed by param_2 (stride 0x0c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243778 GetIntField_eff48

**Description:** Reads int from table at 002eff48 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243790 GetIntField_f0288

**Description:** Reads int from table at 002f0288 indexed by param_2 (stride 0x0c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002437B0 GetIntField_f0300

**Description:** Reads int from table at 002f0300 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002437C8 GetIntField_f03e8

**Description:** Reads int from table at 002f03e8 indexed by param_2 (stride 0x14) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002437E8 GetIntField_f0800

**Description:** Reads int from table at 002f0800 indexed by param_2 (stride 0x0c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243808 GetIntField_f00a8

**Description:** Reads int from table at 002f00a8 indexed by param_2 (stride 0x10) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243820 GetIntField_f06c8

**Description:** Reads int from table at 002f06c8 indexed by param_2 (stride 0x0c) [inferred]

**Callers:** —  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243848 GetNthStringByCategory

**Description:** Switch on category id 1-15 selects a string table; returns the param_3-th NUL-delimited string from it [inferred]

**Callers:** `0022e960`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00243A00 GetPartValueU16

**Description:** Returns u16 from category tables for id 8/11/12 (else 0) indexed by param_2 [inferred]

**Callers:** `00243c50`, `00244f60`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024EA00 ExtractDelimitedToken

**Description:** Extracts the n-th delimited token from string param_2 using delimiter set param_3 into buffer param_1 [inferred]

**Callers:** `002161e0`, `002652b8`, `00265568`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F3B8 CountSetBitsInByteArray

**Description:** Computes population count (total set bits) across an array of param_2 bytes [inferred]

**Callers:** `002161e0`, `00267f48`, `002681a8`, `00269bd0`, `00269c80`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F4A0 IntToPackedBCD

**Description:** Converts an integer to packed BCD format with each decimal digit placed into a 4-bit nibble [inferred]

**Callers:** `00230520`, `00231208`, `002318b0`, `00231c90`, `00231f28`, `002494e8`, `002660f8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0024F4E0 CountDecimalDigits

**Description:** Returns the number of decimal digits required to represent positive integer param_1 [inferred]

**Callers:** `00231208`, `002318b0`, `00231c90`, `00231f28`, `00261638`, `002660f8`, `00268428`, `00268890`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

# Unclassified Engine & Gameplay Helpers

Miscellaneous identified helpers whose inventory description does not yet establish a tighter subsystem.

## 0x00205D00 AdjustWidgetCentering

**Description:** Increments frame byte at +0x11, recenters/resizes widget rect around a width from FUN_00203f40 [inferred].

**Callers:** —  
**Callees:** `00203f40`
**Key globals / structures:** None identified in the inventory note.

## 0x002081D0 FindSetBitRank

**Description:** Returns the zero-based rank of a set bit, or -1 if the requested bit is clear.

**Callers:** `0x002455c0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00208228 FindNthSetBit

**Description:** Returns the bit index of the requested set bit, or -1 when absent.

**Callers:** `0x0022ca28`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020A660 UpdateMenuInputState

**Description:** Updates menu input state and associated option records.

**Callers:** `0x00205088`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0020AB10 UpdateControllerInput

**Description:** Processes controller input and updates shared input state.

**Callers:** `0x00205088`  
**Callees:** `0x0020a8a8`
**Key globals / structures:** None identified in the inventory note.

## 0x0020D008 HandleInputEvent

**Description:** Processes an input event and updates associated state.

**Callers:** —  
**Callees:** `0x002006f0`, `0x00270180`, `0x00270a20`, `0x00207b50`
**Key globals / structures:** None identified in the inventory note.

## 0x0020DF98 UpdateInputDeviceState

**Description:** Updates input device state using shared controller/device data.

**Callers:** `0x002052d0`, `0x0020d120`  
**Callees:** `0x0020d120`
**Key globals / structures:** None identified in the inventory note.

## 0x0020E018 ClearInputTransientState

**Description:** Clears transient input state when the shared input mode is inactive.

**Callers:** `0x00205088`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00212190 DrawMenuGridBackground

**Description:** Draws tiled menu background panels according to grid style and dimensions specified in param_3[4].

**Callers:** `0x00214AD0`, `0x00214EF8`, `0x00215220`, `0x002156D0`, `0x002161E0`, `0x00216738`  
**Callees:** `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0027E840`
**Key globals / structures:** None identified in the inventory note.

## 0x00213C40 GetControllerButtonConfig

**Description:** Converts controller button bindings from player state offset +0x12e8 into configuration index values.

**Callers:** `0x00214220`, `0x00214470`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x0021C8D0 ComputeGridCellIndex

**Description:** Same grid cell index as 0x21c6a0 computed directly from raw fixed-point x and z values [inferred]

**Callers:** `0x002201C0`, `0x0026D700`  
**Callees:** —
**Key globals / structures:** `0x21c6a0`

## 0x002262C8 GetSceneFogParamsPtr

**Description:** Returns &DAT_0177f730 — fog distance param pointer for scene rending. [inferred]

**Callers:** `0x0022aeb0`  
**Callees:** —
**Key globals / structures:** `DAT_0177f730`

## 0x0022EDB8 DrawDualScoreNumbers

**Description:** Draws DAT_0177fdb4 at (0x140,0x2a) and DAT_017831fc at (0x1b8,0x4a) via FUN_00202d10.

**Callers:** `0x00248398`  
**Callees:** `0x00202d10`
**Key globals / structures:** `DAT_0177fdb4`, `DAT_017831fc`

## 0x00233EF0 SetBaseValue

**Description:** Stores a base value in both object fields so later state updates inherit it [inferred]

**Callers:** `0x00251b60`, `0x00254430`, `0x00254d20`, `0x00260dc0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002346F0 SetTrackData

**Description:** Stores two track-related values into the object metadata fields [inferred]

**Callers:** `0x00262678`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00234D58 InitMode13

**Description:** Clears a mode-13 struct and initializes it with a type ID and start state [inferred]

**Callers:** `0x002595f0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234DB0 InitMode14

**Description:** Clears a mode-14 struct and initializes it with a type ID and start state [inferred]

**Callers:** `0x0025ff00`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234E98 InitMode17

**Description:** Clears a mode-17 object and stores a track pointer and mode metadata [inferred]

**Callers:** `0x00251b60`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00234F20 SetTrackMeta

**Description:** Stores a track metadata value in the object at offset 0x8414 [inferred]

**Callers:** `0x00251b60`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00234F38 SetTrackMeta2

**Description:** Stores a second track metadata value into the object fields [inferred]

**Callers:** `0x00251b60`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00234F50 SetTrackMeta3

**Description:** Stores a third track metadata value into the object fields [inferred]

**Callers:** `0x00251b60`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00235320 InitMode1C

**Description:** Clears a mode-1C object block and seeds its type/state values [inferred]

**Callers:** `0x002642b8`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235378 InitMode1D

**Description:** Clears a mode-1D object block and seeds its type/state values [inferred]

**Callers:** `0x0025eb70`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002354D8 SetObjectField844C

**Description:** Stores arg2 into the object field at offset +0x844c. [inferred]

**Callers:** `0x002631a0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00235558 SetObjectField845C

**Description:** Stores arg2 into the object field at offset +0x845c. [inferred]

**Callers:** `0x002631a0`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002355A8 InitObjectType21

**Description:** Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x21. [inferred]

**Callers:** `0x0025e278`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x002355F0 InitObjectType22

**Description:** Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x22. [inferred]

**Callers:** `0x00263710`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235638 InitObjectType23

**Description:** Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x23. [inferred]

**Callers:** `0x00263e68`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235680 SetObjectFields847C8470

**Description:** Stores arg2 at +0x847c and arg3 at +0x8470 of the object. [inferred]

**Callers:** `0x00263e68`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x002356D0 InitObjectType24

**Description:** Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x24. [inferred]

**Callers:** `0x0025dbb8`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235718 SetObjectFields848C8480

**Description:** Stores arg2 at +0x848c and arg3 at +0x8480 of the object. [inferred]

**Callers:** `0x0025dbb8`  
**Callees:** —
**Key globals / structures:** None identified in the inventory note.

## 0x00235730 InitObjectType25

**Description:** Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x25. [inferred]

**Callers:** `0x002638c0`  
**Callees:** `0x0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x00235778 InitGameStateBlock

**Description:** Zeroes 0x8cb0-byte state block, sets fields at +0x8004 and +0x8008=0x26.

**Callers:** —  
**Callees:** `0027e840`
**Key globals / structures:** None identified in the inventory note.

## 0x0026B1B8 UNKNOWN_0x0026B1B8

**Description:** Builds the course list menu and handles accept/decline actions with a left-right selector.

**Callers:** `0026b848`  
**Callees:** `00203d90`, `00204310`, `00204568`, `00206d28`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x0026B470 UNKNOWN_0x0026B470

**Description:** Spawns the racecourse selector list and advances to the chosen track when the player confirms.

**Callers:** `0026b848`  
**Callees:** `002058a8`, `00205c08`, `00205c20`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.

## 0x0026B5C8 UNKNOWN_0x0026B5C8

**Description:** Builds the track-select menu and advances the current choice based on controller input.

**Callers:** `0026b848`  
**Callees:** `00203d90`, `00204310`, `00204568`, `00206d28`, `00258e50`, `00204de8`, `00204c48`
**Key globals / structures:** None identified in the inventory note.
