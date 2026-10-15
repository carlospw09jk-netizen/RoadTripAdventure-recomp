# Task System

The core of the game's cooperative task scheduler: 48 fixed task slots, task records with cleanup callbacks, and recursively-linked task trees. The small helpers here tick timed tasks, kill tasks, and record per-task results.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00204C10` | `ClearTaskPool` | Clears active and linked-task fields for 48 fixed task slots. | `0x002003B8` | — |
| `0x00204D20` | `InitializeTask` | Initializes a task record with supplied fields and clears remaining state. | `0x0021E328`, `0x0021E4D8`, `0x0021E690`, `0x0021ED90`, `0x0021EE98`, `0x0021EF98` | — |
| `0x00204D60` | `DestroyTaskRecord` | Runs task cleanup callback, recursively releases linked tasks, and clears the task reference. | `0x00209CB0`, `0x0020A2B0`, `0x0020E708`, `0x0020F3B8`, `0x002105B8`, `0x00211170`, `0x00211320`, `0x0025A120`, `0x00268E40`, `0x00269568`, `0x00269900`, `0x0026BE50`, `0x0026CCD0` | `0x00205018` |
| `0x00204ED0` | `SetTaskFlag` | Sets bit 1 in a task's status byte. | `0x002052D0`, `0x0020A2B0`, `0x0020E708`, `0x0020EBE8`, `0x0020ED30`, `0x0020F950`, `0x00210430` | — |
| `0x00204EE0` | `SetTaskListFlag` | Sets bit 1 on each task in a linked list. | `0x002105B8` | — |
| `0x00204F18` | `ClearTaskListFlag` | Clears bit 1 on each task in a linked list. | `0x002105B8`, `0x00246250` | — |
| `0x00204F58` | `ClearTaskPoolFlag` | Clears bit 1 on each of 48 pooled task entries. | `0x0020D120`, `0x002105B8`, `0x0026CA48` | — |
| `0x00205018` | `DestroyTaskTree` | Invokes cleanup callback, recursively destroys child/sibling tasks, and clears task state. | `0x00204A00`, `0x00204D60`, `0x00205018`, `0x00204DE8` | `0x00205018` |
| `0x002083C0` | `AdvanceTimedTask` | Advances a timed task callback and kills the task when its timer reaches its terminal value. | `0x0022BB88`, `0x0023BE88` | `0x00258E50`, `0x00204DE8` |
| `0x00208428` | `PlayTaskSound` | Plays a task-associated sound using its stored sound identifier. | `0x002083C0` | `0x00258E50` |
| `0x0020E140` | `InitFlagsIfNotLocked` | If flag 0x100000 clear: sets 0x400000 when count<0x40, calls FUN_002080f8 and FUN_00204310, sets 0x80000000 [inferred]. | — | `0x00204310`, `0x002080F8` |
| `0x0022A2F8` | `CompleteTaskSuccess` | Trivial stub: sets the parent's done flag and result 1, then kills the current task. | `0x00229AF8` | `0x00204DE8` |
| `0x0022C5F0` | `FinishTaskOnInputIdle` | When controller state +0xf is zero, sets the parent's done flag/result to 1 and kills the current task. | `0x0022BFD0` | `0x00204DE8` |
| `0x0022EE00` | `KillTask` | Sets a task slot's timer/kill state and tears it down when it overflows [inferred] | `0x0022EB78` | `0x002080F8`, `0x00204DE8` |
| `0x00233A80` | `UpdateTimedPhaseTask` | Per-frame task state machine: counts 60-frame phases, sets up draw state per phase, kills task on timeout. | `0x002357F0`, `0x00235838`, `0x00235808`, `0x00235850`, `0x00235820` | `0x002080F8`, `0x00258E50`, `0x00258F30`, `0x00204DE8`, `0x0027E840` |
| `0x0023DD30` | `RecordTaskResult` | When param nonzero records DAT_00335730 into table DAT_017870c4 and, for index 6, updates min DAT_01780a10; resets DAT_00335730 [inferred] | `0x0023AD00` | — |
| `0x0023DDB8` | `FindCurrentTaskIndex` | Returns index 0-5 of DAT_00335734 in the DAT_002a2b30 table; 0 when first entry matches or not found [inferred] | `0x0023AD00` | — |
| `0x0023DE10` | `IsSlotActive` | Returns true when slot's +0x23 equals DAT_00335734 and its +0x21 flag is zero [inferred] | `0x002105B8`, `0x00259138` | — |
| `0x002406E8` | `EmptyStub` | Returns immediately; no-op. | `0x00240228` | — |
| `0x00241670` | `EmptyStub2` | Returns immediately; no-op. | `0x00241490` | — |
| `0x00248FA0` | `UpdateTaskTimerLimit` | Advances task timer by 8 per step up to 16, kills task on overflow; flag-dependent path [inferred]. | — | `0x00203990`, `0x00258E50`, `0x00204DE8` |

