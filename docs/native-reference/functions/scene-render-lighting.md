# Scene Rendering & Lighting

Scene-level rendering setup: terrain tile streaming, the faded arc ribbon, fog and sky colour selection, light matrix upload/scaling, and scene lookat/fog accessors.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x002201C0` | `DrawFieldTerrainTiles` | For drawSceneField: streams 5 neighbour tile GS tasks by FUN_0021c8d0, updates DAT_003335b8 tile-visible flags, then debug-overlays regs/coordinates. [inferred] | `0x00220938` | `0x00203A50`, `0x0021C8D0`, `0x0027EEE0`, `0x00204C48` |
| `0x002207D0` | `DrawSceneArcRibbonFaded` | For drawSceneCloudHill/drawSceneField: maybe-draws an arc ribbon via drawArcRibbon then fades ribbon-color toward ~0 by distance bands (60<<band RGB). [inferred] | `0x00220938`, `0x00220B20` | `0x002224F8`, `0x00226610`, `0x00226F70` |
| `0x00225790` | `UpdateSceneLightsAndFog` | Sets fog/sky distance+colour via scene enum, uploads light matrix and 3 light-pair matrices from tod table; called by many render scenes. [inferred] | `0x00208848`, `0x0020F950`, `0x00210430`, `0x002105B8`, `0x0022AEB0`, `0x0022BFD0`, `0x0022C280`, `0x0022CD38`, `0x00246250`, `0x002455C0`, `0x00216738` | `0x00272E08`, `0x002729E0` |
| `0x00226058` | `ScaleCarLightMatrix` | Per render: scales rows of the (param_2) light matrix by a fading param (clamped 0..1.25) and re-uploads the light matrix to VU0. [inferred] | `0x00221788` | `0x00272970`, `0x00226868` |
| `0x00226180` | `GetSceneCamLookatIndex` | Returns the active CAM lookat points table: 1=>&DAT_0029e950, else default &DAT_0029e9b0 — pointer getter for the cars/props rig. [inferred] | `0x002236F8`, `0x00222F08` | — |
| `0x002262C8` | `GetSceneFogParamsPtr` | Returns &DAT_0177f730 — fog distance param pointer for scene rending. [inferred] | `0x0022AEB0` | — |
| `0x002262D8` | `SetDrawSkyColours` | If car type at param_2+0x22 not in {-1,0x11,0x22}, returns 1.0 RGBA; else writes scaled DAT_0177f754..56 floats → sky-dome colour choice. [inferred] | `0x00225608` | — |

