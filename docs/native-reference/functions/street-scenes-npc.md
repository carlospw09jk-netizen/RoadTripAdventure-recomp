# Street Scenes & NPC Cars

Street scene tasks: door menu entity spawning from the doors table, the street scene dialogue/arrival state machine, and NPC tail-car transform updates near the player.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00259138` | `SpawnDoorMenuEntity` | Iterates the doors table (0x2be43c) matching route-event id at param_2+0x54, picking entries from gs_doors_table_address with special cases for doors 9 and 0xc (route/gear dialogs); spawns associated entity. [inferred] | `0x00210430` | `0x00203FC8`, `0x0023D778`, `0x0023DE10`, `0x0023DEB8` |
| `0x002595F0` | `StreetSceneTask` | Task allocating 0x8cc0 state that plays bgm 0xa0d0201, spawns task 0x2357c0, places cars at fixed coordinates (730,98) and (730,102), and drives a dialog/arrive state machine. [inferred] | — | `0x002080F8`, `0x0020DF50`, `0x0021C928`, `0x00232D68`, `0x00232F08`, `0x00233860`, `0x002349C8`, `0x00234D58`, `0x0023D8C8`, `0x00259498`, `0x00258EC8`, `0x00204DE8`, `0x00204C48`, `0x0027E840` |
| `0x00259A08` | `UpdateNpcTailCarTransform` | If not paused, computes distance between an NPC object (param_1+0x1c ptr) and car position 0x177ace0; under 1.5 it snaps flags and rebuilds the NPC's transform/matrix from car velocity and position sectors. [inferred] | `0x0025A120` | `0x00207EC8`, `0x0021D358`, `0x002727C0`, `0x00272958`, `0x00272AE8`, `0x00272B90`, `0x00272C38`, `0x00274B10`, `0x002729D0`, `0x00272A48` |

