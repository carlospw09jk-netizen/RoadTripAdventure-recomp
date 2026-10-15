# Town Exploration & Locations

Town mode: block and entity spawning from car/resident tables, resident dialogue tasks, hub arrival with location asset loading, house entry, and the visited-house menu.

Entries marked *(unresolved)* carry a placeholder `UNKNOWN_` symbol; their behaviour below comes from decompilation notes only.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0020E708` | `UpdateTownBlockTasks` | Updates town/world block tasks based on active block index, spawning block entities or destroying inactive blocks. | `0x00210430` | `0x00204D60`, `0x00204ED0`, `0x0021CE28`, `0x00221258`, `0x0023CD58`, `0x0023DB80`, `0x00204C48`, `0x0027EA30` |
| `0x00210158` | `SpawnTownEntities` | Spawns resident and landmark tasks for a town based on resident table pointers and progress state [inferred]. | `0x002105B8`, `0x0026CA48` | `0x002080D8`, `0x0023DB80`, `0x00204C48` |
| `0x00210430` | `StartTownModeTask` | Sets town mode state, spawns viewport HUD, town block manager, player car, and sound tasks [inferred]. | `0x002105B8`, `0x0026CA48` | `0x00204ED0`, `0x0020EBE8`, `0x0021CE08`, `0x00225790`, `0x002590B0`, `0x00204C48` |
| `0x002105B8` | `UpdateTownExplorationTask` | Main loop task callback for town exploration; updates day/night time, checks mail, dialogue, shop, and race entry states. | `0x00211808` | `0x00204A00`, `0x00204D60`, `0x00204EE0`, `0x00204F18`, `0x00204F58`, `0x00210158`, `0x00210430`, `0x0021C6A0`, `0x00225790`, `0x0022E718`, `0x0023D628`, `0x0023D778`, `0x0023D7E0`, `0x0023D878`, `0x0023DB80`, `0x0023DE10`, `0x0023DFC0`, `0x00258B30`, `0x00258E50`, `0x00258FF0`, `0x00259050`, `0x002590B0`, `0x00211154`, `0x00204DE8`, `0x00204C48`, `0x0027EA30`, `0x00225988` |
| `0x00211530` | `SpawnTasksFromCarTable` | Fills 0x177ea60 table with per-entry addresses from DAT tables and spawns task 0x20dc50 via gs_tasks_spawn [inferred]. | — | `0x0023DB80`, `0x00204C48` |
| `0x00211648` | `SpawnTasksFromResidentTable` | Builds address table from gs_houses_residents entries and spawns task 0x20dc50 via gs_tasks_spawn [inferred]. | — | `0x00204C48` |
| `0x00211708` | `SpawnTownDialogueTasks` | Spawns resident dialogue and camera tasks for current town resident or town center [inferred]. | `0x0026BE50`, `0x0026D628` | `0x0023DB80`, `0x00204C48` |
| `0x00211808` | `StartTownExplorationTask` | Initializes town resident tasks and sets task callback to UpdateTownExplorationTask (FUN_002105b8). | `0x0026BE50`, `0x0026D370`, `0x0026D678` | `0x0023DB80`, `0x00204C48` |
| `0x00265568` | `HouseVisitedMenuTask` | Task building 0x1762400 menu inits house/set data, draws visited-house list from PTR table, edits current house/set via dpad, exits on btn. [inferred] | `0x0026D008` | `0x002080F8`, `0x0023D6E0`, `0x0023D818`, `0x0024EA00`, `0x0024F530`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F928`, `0x00258E50`, `0x00264D50`, `0x00264FF0`, `0x00265128`, `0x002652B8`, `0x00204DE8`, `0x00204C48` |
| `0x0026BE50` | `ProcessLocationArrival` | Hub arrival: builds location asset list, writes per-resident status bytes, picks interaction target, spawns follow-up tasks. [inferred] | `0x0026C460` | `0x00204D60`, `0x00205350`, `0x00205C20`, `0x00205DA0`, `0x00211708`, `0x00211808`, `0x0023CD58`, `0x0023D5F8`, `0x0023DB80`, `0x0023DEB8`, `0x00204DE8`, `0x00204C48` |
| `0x0026C460` | `RequestLocationAssetLoad` | Queues 22 gs_locations_table entries as load request (flag 0xb), polls status, spawns arrival handler 0x0026be50 on ready. [inferred] | `0x0026B848` | `0x00205350`, `0x00205C20`, `0x00205DA0`, `0x00204DE8`, `0x00204C48` |
| `0x0026CCD0` | `DispatchAreaEntryTask` | Queues 12 asset loads, polls area entry (FUN_00205c08), spawns day-cycle task chosen by area byte +0xd (0xa->cb60, 0xb->d2c0). [inferred] | `0x0026B848` | `0x00204D60`, `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x00258E50`, `0x00258F70`, `0x00258FF0`, `0x00204DE8`, `0x00204C48` |
| `0x0026D008` | `UNKNOWN_0026D008` *(unresolved)* | Task trampoline spawned by arrival handler: body is a single call to FUN_00265568(void). [inferred] | `0x002105B8`, `0x0026BE50` | `0x00265568` |
| `0x0026D108` | `HouseEntryInitTask` | Computes work target via FUN_0023DB80 and pointer from gs_houses_residents table, sets 0x177EA5C, spawns tasks 0x20DC50/0x22C518. [inferred] | — | `0x0023DB80`, `0x00204C48` |

