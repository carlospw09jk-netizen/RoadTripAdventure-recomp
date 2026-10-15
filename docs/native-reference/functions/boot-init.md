# Boot & System Init

Low-level PlayStation 2 bring-up: syscall stubs, GS double-buffer setup, and the `Ps2InitSystem` IOP reboot / module-load sequence. Also covers the game-side boot chain that seeds progress state, loads race scene entities, and hands off to town or race entry.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x002000B8` | `Ps2ExitThreadId` | Tiny stub: invokes PS2 syscall 4 (ExitThreadId) and returns; called from 0x0027DB20. | `0x0027DB20` | — |
| `0x002000C0` | `Ps2InitGsDoubleBuffer` | Executes syscall 0x23 then resets GS and sets double buffers: 640x448 (arg 0) or 512x256, storing DMODE2/6 words. [inferred] | `0x00200008` | — |
| `0x002001B8` | `Ps2InitSystem` | Prints E-game banner, reboots IOP via image 0x2E9A58, resets SIF/CD/DMA, loads 7 file modules 0x2E9A70-0x2E9B00, inits MC+PAD, calls 0x00226398. | `0x002003B8` | `0x00226398`, `0x0028C950`, `0x0027EA30`, `0x002789C0`, `0x00278E98`, `0x002701A8`, `0x00288E88`, `0x002797A8`, `0x00273598`, `0x002737F8`, `0x0028BC50`, `0x00287668`, `0x0028C0E0`, `0x0028CE50`, `0x0028CE18` |
| `0x0020E1B8` | `AbortCdOperation` | Clears flag at DAT_00333370 and calls sceCdBreak if busy flag bit 0x8000 is set in DAT_003356c0. | `0x0020DF98` | `0x002791D8` |
| `0x0026D370` | `InitDefaultSpawnPhase` | Boot phase: sets id 81000, inits progress globals, spawns entity tasks, writes player bytes, installs FUN_00211808. [inferred] | `0x00205088` | `0x00204C48` |
| `0x0026D438` | `InitSelectedSpawnPhase` | Boot phase: maps FUN_0027afd8 select case 0-5 to spawn ids via FUN_00229f70, sets progress ptr, chains to 0x0026d550. [inferred] | `0x00205088` | `0x00229F70`, `0x00229FD8`, `0x0027AFD8`, `0x00204C48` |
| `0x0026D550` | `LoadRaceSceneEntities` | Once entity ready (+0xf==0): spawns task 0x0025bcf0, spawns entities 0x370001/0x300001, chains handler to 0x0026d5e0. [inferred] | `0x0026D438` | `0x00204C48` |
| `0x0026D5E0` | `ContinueRaceEntityLoad` | If flag +0x24 set: respawns task 0x0025bcf0 with arg 1 and chains handler to 0x0026d628. [inferred] | `0x0026D550` | `0x00204C48` |
| `0x0026D628` | `ApplyRacePlayerMode` | If flag +0x24 set: writes player mode bytes (+0x21=9, +0x23=8), calls FUN_00211708, chains handler to 0x0026d678. [inferred] | `0x0026D5E0` | `0x00211708` |
| `0x0026D678` | `FinishBootPhase` | If task field +0x10==0: resets progress globals, spawns entity 0x70001, sets player bytes 1/0, installs handler FUN_00211808. [inferred] | `0x0026D628` | `0x00204C48` |

