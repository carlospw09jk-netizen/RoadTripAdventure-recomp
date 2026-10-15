# Task Scheduler and Task State Machines

## How it works

The game's gameplay logic is implemented as **co-routines called "tasks"**. A task is a fixed-size record (`0x30`/48 bytes) living in a static pool of 48 slots at `gs_tasks_pool` (`0x177a0d0`). At every frame the main loop walks the slot list once and invokes each live task's handler — a `(task, frameCtx)` function pointer stored at offset `+0xc`. The handler updates the task's state byte (`+0x25`), reads inputs from the frame context, and either reschedules itself or signals its parent and exits.

Tasks form a **parent / sibling tree** per pool entry: each slot carries a pointer to its "first child" (`+0x10`) and "next sibling" (`+0x14`), with the parent slot's address at `+0x18`. The walker recurses into children after firing each parent's handler, so an entire subtree is ticked in one pass. `gs_tasks_spawn` (`0x00204c48`) is the only way to create a task — it scans the 48-entry pool for the first `*task = 0` slot, marks it live (`*task = 1`), stores the `(handler, arg)` pair, clears the rest of the slot, and links the new task into the parent's child chain. `gs_tasks_kill` (`0x00204de8`) is the only way to destroy one — it runs the optional cleanup callback at `+0x2c`, recursively tears down the child and sibling subtrees via `DestroyTaskTree` (`0x00205018`), unlinks the slot from its parent's `+0x10`/`+0x14` chain, and marks the slot free.

"State machines" in the game are tasks that **self-restart on a tick boundary**: when one phase finishes, the handler updates the per-slot short timer at `+0x2` and rewrites its own `+0xc` pointer to the next phase's function. That is why `0x0026cb60`, `0x0026ccd0`, `0x0026d1c0`, and friends all look like handshake pairs that swap function pointers in their own bodies. Long-running flows like `ProcessLocationArrival` (`0x0026be50`), `BootPhase` chains (`0x0026d370` → `0x0026d438` → `0x0026d550` → `0x0026d5e0` → `0x0026d628` → `0x0026d678`), and the time-of-day cycle (`0x0026ca48` + `0x0026cb60`/`0x0026d2c0` → `0x0026d1c0`) are built by repeatedly assigning new `+0xc` handler values and re-arming the same slot.

The high-level "talk to a guy / open a shop / trigger a race" decision tree in a hub is encoded as a **bytecode interpreter inside `dialogueTask`** (`0x0023b168`). `dialogueTask` is itself a task; on its first tick it caches the script pointer (`+0x1c`), resets the per-task short `+0x2` opcode counter, and on later ticks reads the next opcode byte (`+0x28`), `switch`ing on it to one of `0x23bc90` / `0x0023b168`'s case arms. The cases are themselves more state machines — `0x0023bb88` (string-table load into a text object → poll the result → kill), `0x0023bcd0` (tab-terminated text-table reader), `0x0023be18` (spawn watcher → flag copy back), `0x0023be88` (load `0x2083c0` with a payload from the per-token table at `0x2f00ac`) — each of which spawns a sub-task, waits for its `+0x24` flag, copies a byte back into the parent, and terminates itself.

By convention the "command" chain is `dialogueTask → (sub-state-machine per opcode) → gs_tasks_spawn(0x2083c0, arg)` to actually carry out the gameplay command (a timed effect runner like fade, wait-for-input, dialog box advance). Spawning a task returns a pointer the caller usually discards — `gs_tasks_kill` is invoked from inside the spawned task's handler when its exit condition is met (`+0x24 = 1` plus a result written to `parent +4`).

## Key functions

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x002003b8` | `gameMainLoop` | Endless per-frame loop. Calls `ClearTaskPool` once at boot, then forever loops `vifBeginPacket → gs_tasks_walker → render → ...`. | `0x00200008` (`entry`) | `0x00204c10`, `0x00204f98` (`gs_tasks_walker`), `0x00204c48` (`gs_tasks_spawn`) |
| `0x00204C10` | `ClearTaskPool` | Boot-time helper: zeros `*task` byte and `task[+0x28]` for all 48 slots in `gs_tasks_pool`. | `0x002003b8` | — |
| `0x00204C48` | `gs_tasks_spawn` | Allocates a free pool slot, marks it live, stores `(handler=arg2, arg=arg3)`, links into parent's child list at `+0x10`. **Returns** the task pointer. | 105 callers — every subsystem: menu, town, race, dialogue, memory card, save, boot | `0x002006f0` (`fatal` on pool exhaustion) |
| `0x00204D20` | `InitializeTask` | Low-level 60-byte record init — sets `*task=1`, `task[+0xc]=handler`, `task[+0x1c]=arg`, zeroes `task[+0x4/+0x8/+0x20/+0x28/+0x2c]`, `task[+0x24..+0x27]` and `task[+0x2]` short. Used by a few legacy camera/state paths instead of `gs_tasks_spawn`. | `0x0021e328`, `0x0021e4d8`, `0x0021e690`, `0x0021ed90`, `0x0021ee98`, `0x0021ef98` | — |
| `0x00204D60` | `DestroyTaskRecord` | Runs task's cleanup callback `+0x2c`, recursively frees children (`+0x10`) and siblings (`+0x14`) via `DestroyTaskTree` (`0x00205018`), writes zero into the parent's `+0x10` pointer slot. | menu + arrival flows: `0x00209cb0`, `0x0020a2b0`, `0x0020e708`, `0x0020f3b8`, `0x002105b8`, `0x00211170`, `0x00211320`, `0x0025a120`, `0x00268e40`, `0x00269568`, `0x00269900`, `0x0026be50`, `0x0026ccd0` | `0x00205018` |
| `0x00204DE8` | `gs_tasks_kill` | Runs cleanup callback at `+0x2c`, recursively destroys children + siblings via `0x00205018`, zeros the slot's `*task`, `task[+0x28]`, and walks the previous-link chain `pcVar3[+0x18]+0x10`/`+0x14` to unlink the slot from its sibling list. **146 callers.** | Fade/timer tasks, every menu/race/town flow that wants to retire a sub-task | `0x00205018` |
| `0x00204ED0` | `SetTaskFlag` | Sets bit 1 on the slot's status byte. Marker flag (typically "tick me past my normal lifecycle"). | `0x002052d0`, `0x0020a2b0`, `0x0020e708`, `0x0020ebe8`, `0x0020ed30`, `0x0020f950`, `0x00210430` | — |
| `0x00204EE0` | `SetTaskListFlag` | Sets bit 1 on every task reachable through the parent `+0x10` chain. | `0x002105b8` | — |
| `0x00204F18` | `ClearTaskListFlag` | Clears bit 1 on every task reachable through the parent `+0x10` chain. | `0x002105b8`, `0x00246250` | — |
| `0x00204F58` | `ClearTaskPoolFlag` | Clears bit 1 on each of the 48 pooled slots (sweeping reset). | `0x0020d120`, `0x002105b8`, `0x0026ca48` | — |
| `0x00204F98` | `gs_tasks_walker` | The per-frame tick driver: walks `gs_tasks_pool` siblings. For each live slot (`*task == 1`) calls `task[+0xc]`, clears `task[+0x24]`, increments `task[+0x8]`, then recurses into the child list `task[+0x10]`. | `0x002003b8` (every frame) | recurses into self |
| `0x00205018` | `DestroyTaskTree` | Recursive destructor: invokes `task[+0x2c]`, clears the slot, recursively destroys `task[+0x10]` (child) and `task[+0x14]` (sibling). | `0x00204a00`, `0x00204d60`, `0x00205018`, `0x00204de8` | self (recursive) |
| `0x002082D0` | `fadeInTask` (mono) | Fade task: per-tick calls `FUN_002080f8` with intensity `<< 0x19`, kills itself at tick 0x40. | — | `0x002080f8` |
| `0x002083C0` | `AdvanceTimedTask` | Common timed-execution body: ticks once, when its `task[+0x8]` reaches a terminal value the task kills itself via `gs_tasks_kill`. Used as the worker for menu `+0x2c` cleanup hooks. | `0x0022bb88`, `0x0023be88` | `0x00258e50`, `0x00204de8` |
| `0x0020DC50` | `loadRequestTask` | Specialized task that kicks off a load (busy flag at `0x333370`, request word at `0x333374`) and ORs `0x400000` into `gs_game_drive_flags` (the "loading blur") unless bit `0x100000` is already set. Spawned with an asset id passed through `task[+0x1c]` (`0x370001` = race entity, `0x300001` = car, `0x70001` = scene root, `0x70003` = progress chunk, etc.) | every spawner (`0x002052d0`, `0x00211530`, `0x0026be50`, `0x0026d370`, `0x0026d438`, `0x0026d550`, `0x0026d678`) | engine loaders |
| `0x002080f8` | `ApplyDisplayFadeAlpha` | Tiny helper: writes the supplied alpha value to `DAT_0029abb0` and calls `vifDirect(0x29ab70, 6)`. Used by *every* fade task and a handful of per-frame UI/visual bits. | 51 callers | `0x0026f70` (`vifDirect`) |
| `0x00222EE00` | `(timed kill stub)` | Helper reused by long-running effects: bumps a halfword at `task[+0x2]` each tick, calls `ApplyDisplayFadeAlpha` with the shifted value, and once the counter saturates calls `gs_tasks_kill` on the current slot then sets parent `+0x24 = 1`, parent `+4 = -1`. | `0x0022eb78` | `0x002080f8`, `0x00204de8` |
| `0x0023B168` | `dialogueTask` | "Next msg #%d" hub script interpreter. On first tick caches the script pointer at `task[+0x20]` + the opcode-buffer ptr at `task[+0x28]`. On later ticks reads the next opcode byte and dispatches it (case arms 0..0x18) into sub-state-machines `0x23bc90/0x23bb88/0x23bcd0/0x23be18/0x23be88`, or to a generic follower `0x2083c0` whose `task[+0x1c]` is a payload from `DAT_002f00ac[…]`. State variable at `+0x25` advances through phases 0→6; tick counter at `+0x8` and sub-counter `short +0x2` coordinate them. | every "interact with NPC" entry point — `0x002052d0`, `0x0022bfd0`, `0x00229538`, `0x0026d020`, `0x00239508`, `0x00248398`… | `0x00204c48`, `0x00204de8`, dispatcher cases call `0x0023bc90`/`0x0023bb88`/`0x0023bcd0`/`0x0023be18`/`0x0023be88` |
| `0x0023BB88` | (dialogueTask opcode 9 / string-load) | State machine: first run loads two string tables into a `uiMenuWindow` (`0x002058a8` x3, with language index `DAT_00333284*2` choosing tables from `0x2a2af0`/`0x2a2af4`); later runs poll `FUN_00205c08` (UI wait). On `result == -1` or on receive writes parent `+4 = task[+0xd]`, parent `+0x24 = 1`, kills self. | spawn opcode of `dialogueTask` (`0x0023b168`) | `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x00204de8` |
| `0x0023BC90` | (dialogueTask opcode 2 / handler install) | Tiny init: runs handler `FUN_00239260` once, sets state `+0xd = 1`, rewrites `task[+0xc]` to `FUN_00239260` so subsequent ticks run there. | `0x0023b168` | `0x00239260` |
| `0x0023BCD0` | (dialogueTask opcode: tab-text reader) | First run advances a `(char *)task[+0x1c]` cursor past `\t`-terminated records, fills a `uiMenuWindow` (`0x00205c20`) string-array, sets cursor at `task[+0x20]`. Later ticks call `FUN_00205c08`; on `result == -2` kills self, on `result != 0` posts a sound effect (`0x258e50(0x1a)`), writes parent `+4 = record[idx].byte`, marks parent done. | `0x0023b168` | `0x002058a8`, `0x00205ad0`, `0x00205c08`, `0x00205c20`, `0x00258e50`, `0x00204de8` |
| `0x0023BE18` | (dialogueTask opcode 0x12 / chain-with-flag) | First run spawns task `0x0025bcf0` with arg 2. When `task[+0x24]` is set, copies `*task[+0x1c]` into parent `+4`, sets parent `+0x24 = 1`, kills self. | `0x0023b168` | `0x00204c48`, `0x00204de8` |
| `0x0023BE88` | (dialogueTask opcode 0xb / chain-with-arg) | First run indexes the per-token payload table at `DAT_002f00ac` with `*task[+0x1c]` (or, when the token equals `0x10`, the indirect byte at `DAT_0177f778 + 1`) and spawns `0x2083c0` with the table value as the arg. When `task[+0x8] == 0x40`, writes parent `+4 = task[+0x1c][1]`, kills self. | `0x0023b168` | `0x00204c48`, `0x00204de8` |
| `0x0026BE50` | `ProcessLocationArrival` | Hub arrival big state machine. Decision-byte (`parent +0x24`) is one of `4, 5, 6, 0xb, 0xc`; on `6`/default posts parent done with `-1` and kills self; on `5` spawns body-handler `0x0026d008`; on `4`/`0xb` calls `DestroyTaskRecord` (`0x00204d60`) and respawns via `SpawnTownDialogueTasks` (`0x00211708`) or `StartTownExplorationTask` (`0x00211808`). Otherwise iterates `gs_houses_residents[town]` (`+0x14`), classifies each target per `FUN_0023cd58` / `FUN_0023d5f8` / `FUN_0023deb8` (status byte into `+0x50`), polls the asset-load system (`FUN_00205350`), and once a target is selected sets `DAT_0177ea5c`, spawns `loadRequestTask` (`0x0020dc50`) + `0x0022c518`. | `0x0026c460` | `0x00204c48`, `0x00204d60`, `0x00205350`, `0x00205c20`, `0x00205da0`, `0x00211708`, `0x00211808`, `0x0023cd58`, `0x0023d5f8`, `0x0023db80`, `0x0023deb8` |
| `0x0026C460` | `RequestLocationAssetLoad` | Loads 22 `gs_locations_table` entries as a `uiMenuWindow` load request (load-flag `0xb`), polls it; when load completes writes `param_2[+0x23] = townIdx`, then spawns `ProcessLocationArrival` as the next phase. | `0x0026b848` | `0x00204c48`, `0x00204de8`, `0x00205350`, `0x00205c20`, `0x00205da0` |
| `0x0026CA48` | `ApplyTimeOfDayLighting` | Per-town lighting task. First run spawns `SpawnTownEntities` (`0x00210158`) and `StartTownModeTask` (`0x00210430`). Later runs call `ClearTaskPoolFlag`, spawn `fadeOutTask` (`0x00208270`), set `+0x14` from `DAT_002dcd28[arg]`, and re-apply `updateTimeOfDayLight` (`0x00225988`). When `param_2[+0x22] == -1` it recomputes the player grid sector via `DecomposeWorldPosition` (`0x0021c6a0`). | `0x0026cb60`, `0x0026ccd0`, `0x0026d1c0`, `0x0026d2c0` | `0x00204c48`, `0x00204f58`, `0x00208270`, `0x00210158`, `0x00210430`, `0x0021c6a0`, `0x00225988` |
| `0x0026CB60` | `StartAreaIntroDayCycle` (area 0xa) | Area-0xa day-cycle intro. First run spawns the lighting task `0x0026ca48` and starts BGM `0x0a1c0201`. Each tick the short counter `task[+0x2]` advances; at `0x30c` (780) it spawns `fadeInTask` and triggers an event; at `0x34c` (844) it kills the current lighting task, sets `+0x27 = 1`, swaps in `StartDayCycleStepTask` (`0x0026d1c0`) as its new handler, and switches BGM to `0x0a1a0201`. | `0x0022aeb0`, `0x0026ccd0` | `0x00204c48`, `0x00204de8`, `0x002082d8`, `0x00258f30`, `0x00258fb0`, `0x00258ec8` |
| `0x0026CCD0` | `DispatchAreaEntryTask` | Area-entry dispatcher: first run preloads 12 string tables into a `uiMenuWindow` (`0x00205c20` → `0x002058a8` x12). On later runs polls via `0x00205c08`; when its ready flag (param_2 +0x30 bit 0x800) is set it `DestroyTaskRecord`s the previous flow, calls `0x00258ff0` + `0x00258f70`, then picks a day-cycle task by `param_2[+0xd]` (0xa → `0x0026cb60`, 0xb → `0x0026d2c0`) and spawns it. | `0x0026b848` | `0x00204c48`, `0x00204d60`, `0x00204de8`, `0x002058a8`, `0x00205c08`, `0x00205c20`, `0x00258e50`, `0x00258f70`, `0x00258ff0` |
| `0x0026CF20` | `BrowseItemsMenuTask` | UI menu task: lays out two items per page and supports cursor up/down with `0x10` button. Body calls `FUN_0026c588` once per tick, then moves `task[+0x26]` by `0x2` on each `0x2000`/`0x8000` input. On button `0x10` (skip / X), posts parent done with `-1` and kills self. | — | `0x0026c588`, `0x00238a48`, `0x00204de8` |
| `0x0026D008` | (trampoline task) | Trivial wrapper task whose body is a single call to `FUN_00265568`. Spawned from `UpdateTownExplorationTask` and `ProcessLocationArrival` to run that handler as a detached child. | `0x002105b8`, `0x0026be50` | `0x00265568` |
| `0x0026D020` | `HandleMenuScrollSelect` | Menu selection-via-buttons: reads input bits `0x1000`/`0x4000` to scroll the chosen index at `DAT_0177aea8[DAT_0177ae9a]`; bits `0x50` swap the handler to the resolver `0x0026bc08`; bit `0x100` calls commit `0x0020e5b8(parent[+0x20], DAT_0177aea8)`. | `0x0026bc08` | `0x002080f8`, `0x0020e5b8`, `0x00250738` |
| `0x0026D108` | `HouseEntryInitTask` | Spawned when player walks up to a door: computes work target via `FUN_0023db80`, sets `DAT_0177ea5c` to either `gs_houses_residents[town] + i*0x10` or `gs_progress_ram_address + i*0x1c`, and spawns `loadRequestTask` (`0x0020dc50, 0x70301`) plus `0x0022c518`. | — | `0x00204c48`, `0x0023db80` |
| `0x0026D1C0` | `AdvanceDayCycleStep` | Day-cycle state stepper. Each tick compares the runtime clock to `DAT_002dcd50[step]` (`task[+0x27]`); at `target - 0x40` spawns a fade-in; at target it kills the current lighting task, advances `+0x27`, respawns lighting task `0x0026ca48` with the new step, and after step 10 marks parent done (`+4 = 0`) and kills itself. | `0x0026cb60`, `0x0026d2c0` | `0x00204c48`, `0x00204de8`, `0x002082d8` |
| `0x0026D2C0` | `StartDayCycleStepTask` (area 0xb) | Area-0b entry: starts BGM `0x0a1a0201`, spawns lighting task `0x0026ca48` at step `+0x27 = 1`. When the player-ready flag (`param_2[+0xf] == 0`) fires it rewrites its own `+0xc` to `AdvanceDayCycleStep` (`0x0026d1c0`) and spawns setup task `0x0026a928`. | `0x0022aeb0`, `0x0026ccd0` | `0x00204c48`, `0x00258ec8`, `0x00258f30` |
| `0x0026D370` | `InitDefaultSpawnPhase` | "Boot phase" task #1 (bootid `81000`): inits the progress globals (`DAT_0177ea50 = &gs_progress_ram_address`, `DAT_0177ea58 = &0x0177f798`, `DAT_0177ea54 = 0x177f77c`), spawns the three `loadRequestTask` passes (entity 1, current-slot chunk, `0x70003` progress chunk), writes `param_2[+0x21] = 0` and `param_2[+0x23] = DAT_01780a3c`, then sets its `+0xc` to `StartTownExplorationTask` (`0x00211808`). | `0x00205088` | `0x00204c48`, `0x00211808` |
| `0x0026D438` | `InitSelectedSpawnPhase` | "Boot phase" task #2 — switch-on-`FUN_0027afd8(param_2[0], 6)` with cases `0..5` writing per-case progress slot ids via `FUN_00229f70` (case-`0 → 0x2b`, case-`1 → 0`, …, case-`5 → 0x5d`), runs `InitProgressRamDefaults` (`0x00229fd8`), spawns a `loadRequestTask` for the selected chunk, points `DAT_0177ea50 = &PRR_PTR_TABLE + 0x80`, sets `+0xc = LoadRaceSceneEntities` (`0x0026d550`). | `0x00205088` | `0x00204c48`, `0x00229f70`, `0x00229fd8`, `0x0027afd8` |
| `0x0026D550` | `LoadRaceSceneEntities` | First `param_2[+0xf] == 0`: spawns task `0x0025bcf0` with arg 0, sets `+0xc = ContinueRaceEntityLoad` (`0x0026d5e0`), and spawns entities `0x370001` and `0x300001` via `loadRequestTask`. | `0x0026d438` | `0x00204c48` |
| `0x0026D5E0` | `ContinueRaceEntityLoad` | When the spawned sub-task's `+0x24` flag is set: respawns `0x0025bcf0` with arg 1, sets `+0xc = ApplyRacePlayerMode` (`0x0026d628`). | `0x0026d550` | `0x00204c48` |
| `0x0026D628` | `ApplyRacePlayerMode` | On its sub-task's `+0x24` flag: writes `param_2[+0x21] = 9`, `param_2[+0x23] = 8`, runs `SpawnTownDialogueTasks` (`0x00211708`), chains handler to `FinishBootPhase` (`0x0026d678`). | `0x0026d5e0` | `0x00211708` |
| `0x0026D678` | `FinishBootPhase` | Only on `task[+0x10] == 0`: re-points `DAT_0177ea50/54/58` at the progress tables, spawns `loadRequestTask(0x70001)` (scene root entity), writes `param_2[+0x21] = 0` and `param_2[+0x23] = 1`, swaps its `+0xc` to `StartTownExplorationTask` (`0x00211808`). | `0x0026d628` | `0x00204c48`, `0x00211808` |

## Data structures / globals

| Address | Name / role |
|---|---|
| `0x177a0d0` | `gs_tasks_pool` — root of the task tree. Walker enters here. The pool is **48 task records of 0x30 bytes**; `*task` (`+0x0`) is the live flag (`0` = free, `1` = live). |
| task slot layout (`task*` + offset) | `+0x0` live byte; `+0x2` short opcode/cursor counter; `+0x4` result or index field (used to pass back to parent); `+0x8` tick/frame counter; `+0xc` `code *handler(self, frameCtx)`; `+0x10` first child slot ptr; `+0x14` next sibling slot ptr; `+0x18` parent slot ptr; `+0x1c` initial argument (opaque, sized to opcode); `+0x20..0x23` state bytes (sub-state, language index, `[inferred]` values: `+0x20` step counter in `AdvanceDayCycleStep`, `+0x22` player ready, `+0x23` town/character index); `+0x24` "done" flag (consumed by parent); `+0x25` inner state phase (`0..0x6`); `+0x26` sub-state (per-opcode scratch); `+0x27` day-cycle step index `0..10`; `+0x28` second arg / opcode pointer for `dialogueTask`; `+0x2c` cleanup callback fn-ptr (invoked by `gs_tasks_kill` and `DestroyTaskTree` before freeing). |
| `0x177a0d0..0x177a0ff` | `*task` (live flag) for slots 0..47. |
| `0x177a100` | first iteration pointer into the pool (Walker iterates via `+0x14` chains). |
| `0x177a108..0x177a130` | `task[+0x10]`/`+0x14` linked-list fields for the first few slots; detailed pointer chain varies. |
| `0x335900` | `+0x160`-byte scratch frame context passed as `param_2` to every task handler. The walker is `gs_tasks_walker(gs_tasks_pool, frameCtx)`. |
| `0x335908` | `DAT_00335908` — current back-buffer index; flipped by the main loop each frame. |
| `0x332288` | Display mode (set by `gsInitDisplay`). |
| `0x332284` | `DAT_00333284` — language index (used by `0x0023bb88` to pick strings from the per-language tables at `0x2a2af0`/`0x2a2af4`). |
| `0x29abb0` | Most-recently-written value for `ApplyDisplayFadeAlpha` (`0x002080f8`); the actual alpha register lives at `0x29ab70`. |
| `0x177f798` | progress chunk base (target of `DAT_0177ea58`). |
| `0x177f77c` | progress slot pointer table (target of `DAT_0177ea54`). |
| `0x177f778` | a 4-byte current-slot tag; byte `+1` (`DAT_0177f778._1_1_` = `0x177f779`) is read by `0x0023be88` when the opcode token is `0x10`. |
| `0x177ea50` | `DAT_0177ea50` — current progress address pointer (set by `InitDefaultSpawnPhase`, `InitSelectedSpawnPhase`, `FinishBootPhase`). |
| `0x177ea54` | `DAT_0177ea54` — current progress slot pointer table. |
| `0x177ea58` | `DAT_0177ea58` — current progress chunk base. |
| `0x177ea5c` | `DAT_0177ea5c` — last-set work-target pointer (house-entry or arrival): either `gs_houses_residents[town] + i*0x10` or `gs_progress_ram_address + i*0x1c`. |
| `0x177f760` | scratch area written by `0x0023c418` (called from `dialogueTask` opcode `0x15`). |
| `0x1780a3c` | persistent string byte (`DAT_01780a3c`, set by `dialogueTask` opcode `0xe`, read by `InitDefaultSpawnPhase`). |
| `0x177ace4` | `DAT_0177ace4` — light-state pointer passed to `updateTimeOfDayLight` each frame by `ApplyTimeOfDayLighting`. |
| `0x177acf0` | `DAT_0177acf0` — player-area sector table (target of `DecomposeWorldPosition`). |
| `0x177ac50` | hi/lo alpha strip word target for `FUN_00250738` (fade/alpha update). |
| `0x177ae9a` | `DAT_0177ae9a` — index into the menu cursor array at `0x177aea8`. |
| `0x177aea8` | `DAT_0177aea8` — base of the menu cursor array (modified by `HandleMenuScrollSelect`). |
| `0x1787008..0x178701a` | race-participant slot table (written by `SpawnRaceRivalTasks` `0x0020ed30`). |
| `0x1787016` | `DAT_01787016` — finish-position ranks table (written by `ResetRaceRanks`). |
| `0x172ff70..0x172ff7c` | per-car effect-slot table (target of `SpawnCarEmitterEntry`/`AdvanceCarEffectSlot`). |
| `0x2be438..0x2be448` | entries in `gs_locations_table` — 22 town-table per-index pointers (read by `RequestLocationAssetLoad`). |
| `0x2a7ed0` | base of `gs_houses_residents[town]` — per-town rows of `{resident-ptr…}`. |
| `0x2c22c8` | `gs_progress_ram_address` — current per-resident progress record base (sized `0x1c` per resident). |
| `0x2f00ac` | `DAT_002f00ac` — per-token payload table indexed by `dialogueTask` opcode `0xb`'s `*task[+0x1c]` (or the alternate `DAT_0177f778[1]` byte). Entries are 16-byte stride; `gs_tasks_spawn` is called with the second 32-bit field as the `arg` value (forwarded into `AdvanceTimedTask`/`0x002083c0`). |
| `0x2a2af0/0x2a2af4` | per-language string-table pointer arrays for the dialogueTask `0x9` (string-load) opcode. |
| `0x2ec7c8/0x2ec7e0` | `PTR_DAT_002ec7e0` — dialogue byte-code tables referenced from `dialogueTask`'s opcode `0xf`/`0x9`. |
| `0x2f00ac[0x10]` | alternate index `0x10` (resolved through `DAT_0177f778[1]` byte). |
| `0x333284..0x33360a` | various per-tick flags and the "loading blur" `0x400000` bit in `gs_game_drive_flags`. |
| `0x335900..0x3359c0` | scratch globals used by the main loop (frame-counter at `+0x0`, back-buffer index at `+0x8`, sync-vs at `+0x9`, frame-counter hi at `+0x50`). |

## Control flow

**Per-frame boot path** (one-time, `0x002003b8`):

1. `Ps2InitSystem` (`0x002001b8`) — IOP reboot, SIF/CD/DMA init, pad init.
2. `memset(&DAT_00335900, 0, 0x160)` — clear scratch globals.
3. `FUN_0020dde0` / `FUN_002038f8` — kernel / display hooks.
4. **`ClearTaskPool` (`0x00204c10`)** — zero all 48 task slots.
5. **`gs_tasks_spawn(0, StartMenuFlow, 0)`** — create the root menu task as an orphan (parent `0`).

**Per-frame loop** (every ~16 ms):

1. `vifBeginPacket(0)` — open the GS packet.
2. **`gs_tasks_walker(gs_tasks_pool, frameCtx)`** — `0x00204f98` walks the live-task tree:
   - For each slot whose `*task == 1`: invoke `task[+0xc](self, frameCtx)`; clear `task[+0x24]` ("done-this-tick" flag); increment `task[+0x8]`; recurse into `task[+0x10]`.
   - Follow `+0x14` to walk the sibling chain.
3. VIF/GS submit + swap buffers; back-buffer index `DAT_00335908 ^= 1`.
4. Loop forever — never returns.

**Spawning a task** (called from ~105 sites):
- `gs_tasks_spawn(parent_task*, handler_fn, arg)` at `0x00204c48`.
- Linear scans 48-slot pool for `*slot == 0`, sets `*slot = 1`, writes `slot[+0xc] = handler`, `slot[+0x1c] = arg`, links `slot` at the end of `parent[+0x10]` (or starts the chain if empty).
- If no slot is free it `fatal(0x2ea048)`s — the game runs out of tasks on overflow.
- Returns the new slot pointer (callers usually discard it; the parent polls the spawned task's `+0x24` flag and reads `task[+4]` for the result).

**Killing a task** (called from 146 sites):
- `gs_tasks_kill(self)` at `0x00204de8`.
- Runs the optional `task[+0x2c]` cleanup callback (used by tasks that allocate heap/buffers).
- Recursively destroys the child subtree (`task[+0x10]`) and next sibling (`task[+0x14]`) via `DestroyTaskTree` (`0x00205018`).
- Walks the previous-sibling chain through `parent[+0x18]+0x10` to splice itself out.
- Marks the slot free.

**State-machine stepping** — tasks like the boot phase chain, day cycle, and `dialogueTask` opcodes all follow the same pattern:

1. First tick (`task[+0x8] == 0`): spawn child tasks via `gs_tasks_spawn(parent, sub_handler, sub_arg)`.
2. Subsequent ticks: poll the child's `+0x24` flag (or a global ready flag like `parent[+0xf]`); when set, write the result to `parent[+4]` and `parent[+0x24] = 1`, then `gs_tasks_kill(self)`.
3. The next phase rewrites the parent's `+0xc` handler pointer before exiting, so the **parent task body advances**: e.g. `InitDefaultSpawnPhase` (`0x0026d370`) hands control to `StartTownExplorationTask` (`0x00211808`) by writing its address into `task[+0xc]`; that handler in turn hands control to `UpdateTownExplorationTask` (`0x002105b8`) the same way.

**Boot phase chain** (`0x0026d370 → 0x0026d438 → 0x0026d550 → 0x0026d5e0 → 0x0026d628 → 0x0026d678`):

```
ClearTaskPool
  -> gs_tasks_spawn(StartMenuFlow, 0)        // 0x002052D0
     -> UpdateMenuFlowTask  (0x00205088)
        -> gs_tasks_spawn(InitDefaultSpawnPhase, 0)        // 0x0026d370 (case 1 = boot flow id 81000)
           -> spawns 0x20dc50 (loadRequestTask) x3           // entity 1, slot chunk, 0x70003 progress
           -> sets task[+0xc] = StartTownExplorationTask     // 0x00211808
              -> spawns InitialTownResidentTasks              // 0x0023db80
              -> sets task[+0xc] = UpdateTownExplorationTask  // 0x002105b8  (the per-frame town loop)
        -> gs_tasks_spawn(InitSelectedSpawnPhase, 0)         // 0x0026d438 (case 0 = race flow)
           -> InitProgressRamDefaults + per-case spawn id via switchD_0..5
           -> spawns 0x20dc50 (selected-chunk load)
           -> sets task[+0xc] = LoadRaceSceneEntities          // 0x0026d550
              -> spawns 0x25bcf0 + 0x20dc50(0x370001) + 0x20dc50(0x300001)
              -> sets task[+0xc] = ContinueRaceEntityLoad      // 0x0026d5e0
                 -> respawns 0x25bcf0 arg=1 on sub-flag
                 -> sets task[+0xc] = ApplyRacePlayerMode      // 0x0026d628
                    -> writes param_2[+0x21]=9, [+0x23]=8
                    -> SpawnTownDialogueTasks                  // 0x00211708
                    -> sets task[+0xc] = FinishBootPhase       // 0x0026d678
                       -> spawns 0x20dc50(0x70001) (scene root)
                       -> sets task[+0xc] = StartTownExplorationTask // 0x00211808
```

**DialogueTask bytecode** (each NPC interaction in a town is one of these):

```
gs_tasks_spawn(parent_task, dialogueTask=0x0023B168, script_ptr)
  dialogueTask:
    ++tick (first run)
      cache script_ptr at task[+0x20]
      cache opcode-table ptr at task[+0x28]
      play sfx 0x25
    subsequent ticks
      read next byte from script_ptr[+0x28]
      switch(byte):
        0/0xf: "advance text" — rewind tick counter and listen for buttons 0x50
        1   : chain to FUN_00239260 (run that handler) — write task[+0xc] = FUN_00239260
        2   : chain via 0x0023bc90 — install FUN_00239260 as handler, set state +0xd=1
        3   : "wait for confirm" — play sfx 0x26, set state 3
        4/6 : spawn generic timed-task 0x239900 with id; transition to state 5
        5   : chain via 0x0023a320 (an "advance + replay" sub-state)
        7   : chain via 0x0023bcd0 (tabbed table reader) — runs string-load then waits on FUN_00205c08
        8   : chain via 0x0023a320
        9   : chain via 0x0023bcd0 (state 9 variant)
        10  : default — count ticks until another 0x50 input
        11  : chain via 0x0023be88 (chain-with-arg — see below)
        12  : load record 0x0e from table; chain
        13  : award stamps via gs_stamps_award_function
        14  : write DAT_01780a3c, spawn save spinner, load record
        15  : FUN_0023c418(0x177f760, b1, b2); load record 0x15
        16/17/18:  chain to 0x23a140/0x23a460/ad-hoc counters
        19  : "open paint editor" via gs_photos_take_function / 0x22bfd0 / etc.
      On result: post parent_done = record_byte, parent flag = 1, gs_tasks_kill(self)
```

**State-machine helpers used inside `dialogueTask`** (5 hand-written state-machines the bytecode can dispatch to):
- `0x0023bb88` (opcode 9/string-load): first-tick loads 2 string tables; later waits on `FUN_00205c08`; on `−1`/`receive` posts back result.
- `0x0023bc90` (opcode 2): installs `FUN_00239260` and runs it once.
- `0x0023bcd0` (opcode 9/tab-reader): scans a tab-terminated record buffer; treats `"result == −2"` as "kill self"; otherwise returns the indexed byte.
- `0x0023be18` (opcode 0x12/chain-with-flag): first run spawns `0x0025bcf0 arg=2`; when its `+0x24` flag is set it copies `*task[+0x1c]` back into `parent[+4]`.
- `0x0023be88` (opcode 0xb/chain-with-arg): first run indexes `DAT_002f00ac` with `*task[+0x1c]` (or `DAT_0177f778[1]` for token `0x10`) and spawns `0x002083c0` with that value as the arg; when `task[+0x8] == 0x40` it copies `task[+0x1c][1]` back into `parent[+4]`.

## Open questions

- `[inferred]` Slot count "48" comes from `ClearTaskPool`'s `0x2f` down-counter; not double-checked against actual walked capacity — `gs_tasks_spawn` has `0x2f < iVar1` overflow check and `fatal(0x2ea048)`s above it.
- `[inferred]` The exact meaning of `task[+0x25]`'s values 0..6 (used as an inner phase) and `task[+0x27]`'s 0..10 day-cycle step range; the bounds and transitions are partly visible in `AdvanceDayCycleStep`'s `'\t'` step-10 kill branch and `dialogueTask`'s case `0`/`0xf`/`0x14` (step-0xb).
- `[inferred]` The full opcode table at `DAT_002f00ac` and the size/scope of the per-token payloads used by `0x0023be88` — only the dispatch code is documented; the table content is data-of-data.
- `[inferred]` Function-pointer at `task[+0x2c]` is described in `functions.tsv` as a "cleanup callback" — confirmed by the `*task[+0x2c]` check inside `gs_tasks_kill` and `DestroyTaskTree`, but no concrete example of a callback that uses heap allocation has been mapped to a specific symbol beyond the implicit ones used by menu/camera spawn paths.
- Unknown: how the per-task result `task[+4]` is initialized (always zero by `gs_tasks_spawn`, but `dialogueTask`'s opcode `0x14` writes a byte and opcode `0x15` writes a byte — the type is `int` but the high 24 bits are likely unused except for `0xffffffff` aborts from `ProcessLocationArrival` and the like).
- Unknown: how the frame context (`0x335900`) gets padded/extended between the main loop and the per-task handler — every handler takes `(slot, frameCtx)` so the layout of `frameCtx` (drive flags at `+0x14/+0x20/+0x21/+0x23/+0x28/+0x30/+0x34/+0x50`, world pos at `+0x58/+0x5c`, sector at `+0x54`) is what every task reads from.
