# Race Mode

Race setup and lifecycle: HUD/viewport and rival spawning, rank and lap/split record tables, countdown timers, course events, zone classification, and race result recording.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0020EBE8` | `SpawnGameViewTasks` | Spawns HUD and camera viewport tasks for 1-player (mode 0/2) or 2-player split screen (mode 1) [inferred]. | `0x0020F950`, `0x00210430` | `0x00204ED0`, `0x00204C48` |
| `0x0020ED30` | `SpawnRaceRivalTasks` | Spawns rival car tasks for race modes and populates race participant slot table DAT_01787008 [inferred]. | `0x0020F950` | `0x00204ED0`, `0x0023DB80`, `0x00204C48` |
| `0x0020F330` | `ResetRaceRanks` | Resets race participant slot status and adjusts car finish positions against eliminated ranks into DAT_01787016 [inferred]. | `0x0020F3B8` | — |
| `0x0020F3B8` | `UpdateRaceGameTask` | Main per-frame task handler for race mode; updates day/night lighting, checks pause/finish state, and spawns exit transitions. | `0x0020F950` | `0x00204D60`, `0x002080F8`, `0x0020F330`, `0x0021CBC8`, `0x0021CCB8`, `0x0022E5F8`, `0x00237C48`, `0x00238950`, `0x0023D8C8`, `0x00258E50`, `0x00258F70`, `0x00258F90`, `0x00258FF0`, `0x00259050`, `0x00258EC8`, `0x00204DE8`, `0x00204C48`, `0x0026E4F0`, `0x00226248`, `0x00225988` |
| `0x0020F950` | `StartRaceModeTask` | Initializes race mode: sets race course table, spawns HUD, rivals, and environment tasks, and assigns race update callback. | `0x0020A150`, `0x002105B8`, `0x00211170`, `0x00211320`, `0x0026B1B8`, `0x0026B470`, `0x0026B5C8` | `0x00204ED0`, `0x002080D8`, `0x0020EBE8`, `0x0020ED30`, `0x0021CE08`, `0x0021CE28`, `0x00225790`, `0x00204C48` |
| `0x00211170` | `UpdateQuickRaceMenuTask` | Handles menu flow and race selection for Quick Race mode, spawning selection UI and launching the race [inferred]. | `0x00205088` | `0x00204D60`, `0x00229FD8`, `0x00204C48` |
| `0x00211320` | `UpdateTwoPlayerMenuTask` | Handles menu flow for 2-player race mode, setting up player selections and launching 2-player race task [inferred]. | `0x00205088` | `0x00204D60`, `0x002080F8`, `0x00237AC0`, `0x00204C48` |
| `0x0022C518` | `StartCourseEventTask` | Probes FUN_0023cd58 for an unlock flag, installs handlers 0x22aeb0/0x22c630, resets the tick and spawns task 0x208270. [inferred] | `0x002105B8`, `0x00211708`, `0x00211808`, `0x0026BE50`, `0x0026D108` | `0x002080F8`, `0x0022AEB0`, `0x0023CD58`, `0x00204C48` |
| `0x0022D288` | `ResetRecordTables` | Clears record tables DAT_01787068 and DAT_01786ff0 to INT_MAX and seeds best record DAT_00335700 from progress by slot/type. | `0x0022DDD0` | — |
| `0x0022D390` | `RecordLapSplitTimes` | Stores a lap/split time into per-category record tables, plays SE 0x2e/0x2f for new bests and advances the lap counter. | `0x0022D620` | `0x00258E50` |
| `0x0022D620` | `UpdateZoneFromRects` | Classifies an object's X/Y against three rectangles from param_4, storing zone id 1-4 at +0x19a. [inferred] | `0x0022DDD0`, `0x0022EB78` | `0x0022D390` |
| `0x0022D7B8` | `RankCarsByProgress` | Insertion-sorts the 24 car objects by lap progress/zone/position and assigns incrementing rank numbers at +0x247. | `0x0022DDD0` | — |
| `0x0022D8C8` | `DrawRecordTableRows` | Emits up to three result rows via FUN_002340c0, highlighting entries equal to best record DAT_00335700. [inferred] | `0x0022DDD0` | `0x002340C0` |
| `0x0022DAE8` | `RunCountdownTimerTask` | Fades in over 64 ticks, triggers SE 0x2d at tick 60, a flag at 240, then after 300 ticks ends the event and kills itself. [inferred] | `0x0022DDD0`, `0x00235868` | `0x002080F8`, `0x002329A0`, `0x00258E50`, `0x00258F30`, `0x00204DE8` |
| `0x0022DC68` | `FinishOnCarApproach` | Scans cars; the first within 110 units sets rank bits, switches handler to 0x22eac8 and kills this task. [inferred] | `0x0022DDD0` | `0x00258FB0`, `0x00258FF0`, `0x00204DE8` |
| `0x0022E5F8` | `AccumulateDistanceTotals` | Accumulates accumulator DAT_0177ae54 (>>15) into a global total and per-slot counters for up to three flagged objects. | `0x0020F3B8` | — |
| `0x0022E718` | `AccumulateDistanceSingle` | Adds accumulator DAT_0177ae54 (>>15) to global total DAT_0177fdb8 and its per-slot counter, then zeroes it. | `0x002105B8` | — |
| `0x0022EB78` | `UpdateZoneCheckFrame` | Runs zone test 0x22d620 on all 24 cars, tallies results into param_1+0x25, and on finish switches handler to 0x22ee00. [inferred] | `0x0022ECB8` | `0x0022D620`, `0x00233860`, `0x00258FB0` |
| `0x0022ECB8` | `InitRankingDisplay` | Seeds flag byte +0x25 from the entry flags, XORs it with 0x3f, installs handler 0x22eb78 and runs it once. [inferred] | `0x0022DDD0` | `0x0022EB78`, `0x00234528`, `0x00258F30` |
| `0x00259498` | `CheckCarArrivedAtZone` | Checks whether race car index param_2 has arrived (position thresholds DAT_003330ac-0xb4, speed 117/83, parked flags at 0x177ad40); sets route bit 0x800 and per-player flag bytes on param_1. [inferred] | `0x002595F0` | — |

