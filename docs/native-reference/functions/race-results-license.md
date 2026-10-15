# Race Results & License Screens

The results screens: standings and per-player result drawing, license award screen, records screen, player name slots, best-time/score updates, and award messages.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00235880` | `InitScreenLayout` | Initialises a fixed HUD/result layout: sets header fields and builds six text/panel widgets with hard-coded screen coordinates. [inferred] | `0x00235B58`, `0x00237388`, `0x00237508` | `0x00206040`, `0x002063C8`, `0x002075F0`, `0x0021D718`, `0x0021D7B0`, `0x0021DAE8` |
| `0x002359E8` | `DrawStandingsEntry` | Draws one two-column result entry (rank icon plus values) for row (arg2,arg3), picking the string table from the +0x22 type. [inferred] | `0x00235B58` | `0x00201E48`, `0x00203B78`, `0x00203FC8`, `0x0023DB00` |
| `0x00235B58` | `DrawRaceResultsScreen` | Builds the race-results screen: allocates state, lays out labels, draws standings, then ends the task on button input. [inferred] | `0x0020F3B8` | `0x00201E48`, `0x00203FC8`, `0x00204310`, `0x00204568`, `0x002080F8`, `0x0020DF50`, `0x00235880`, `0x002359E8`, `0x00237C48`, `0x0023D8C8`, `0x00204DE8`, `0x00226F70` |
| `0x00235F10` | `DrawPlayerResults` | Draws one player's result block: rank/course labels plus time and score values read from the per-course tables. [inferred] | `0x002364B0` | `0x00203B78`, `0x00203FC8`, `0x00204310`, `0x0023DB00` |
| `0x00236238` | `SumPlayerPoints` | Returns the sum of table points (DAT_0029fcb8) for the course byte at +3 across the active players. [inferred] | `0x00237508` | — |
| `0x00236328` | `DrawLicenseList` | Loops six course slots and draws list rows with point values for each player below the active player count. [inferred] | `0x002364B0` | `0x00202150`, `0x00204310` |
| `0x002364B0` | `DrawLicenseResultsScreen` | Draws the license-award screen (per-player results, list rows, "A/B-License" text) and applies award/state changes on confirm. [inferred] | `0x00237508` | `0x00203FC8`, `0x00204310`, `0x00204568`, `0x0022E7F0`, `0x00235F10`, `0x00236328`, `0x00237C48`, `0x0023CB68`, `0x0023D628`, `0x0023DC58`, `0x0023DF38`, `0x0023DFA0`, `0x0023C8A8`, `0x00204DE8`, `0x00226F70` |
| `0x00236858` | `UpdateRankingsAndAdvance` | Computes 8 score sums, sorts them, draws ranking rows and advances round state; kills task when done [inferred]. | `0x00237388` | `0x00202150`, `0x00203FC8`, `0x00204310`, `0x00204568`, `0x0022E7F0`, `0x0022EA08`, `0x0023DC58`, `0x00258E50`, `0x00204DE8` |
| `0x00236D28` | `DrawRowsAndUpdateState` | Draws 8 rows of text/icon entries using per-row tables, toggles state byte at +0x25 when input bit 0x40 set [inferred]. | `0x00237388` | `0x00202150`, `0x00203FC8`, `0x00204310`, `0x00204568`, `0x0022EA08`, `0x00258E50` |
| `0x00237080` | `DrawEntryRowsForState` | Draws 8-entry rows for a state object, using lookup tables and FUN_00204568/FUN_00204310 draw calls [inferred]. | `0x00237388` | `0x00203FC8`, `0x00204310`, `0x00204568`, `0x0022EA08`, `0x00258E50` |
| `0x00237388` | `DispatchRecordsScreen` | Inits a 0x1f0-byte record-screen state, fills the DAT_01782a74 table, then dispatches to one of three sub-screens by byte +0x25. [inferred] | `0x0020F3B8` | `0x00203FC8`, `0x0020DF50`, `0x00235880`, `0x00236858`, `0x00236D28`, `0x00237080`, `0x00226F70` |
| `0x00237508` | `EnterLicenseScreen` | Exits on cancel, else allocates state, computes player points, sets +0x27 stage, installs FUN_002364b0 and runs it. [inferred] | `0x0020F3B8` | `0x002080F8`, `0x0020DF50`, `0x00235880`, `0x00236238`, `0x00204DE8` |
| `0x00237878` | `DrawLocationList` | Draws the scrolling location list from gs_locations_table and adjusts the +2 scroll offset from d-pad input; closes on confirm. [inferred] | `0x0026BC08` | `0x00203FC8`, `0x0022EA08`, `0x0023D628`, `0x00204DE8` |
| `0x00237AC0` | `LookupPlayerNameSlot` | Searches or creates a name slot (max 20) for the current player id, stores its index at +0x1c/+0x1e, else uses "ANYBODY". [inferred] | `0x00211320` | `0x0027F1E0` |
| `0x00237C48` | `UpdateRaceRecords` | Updates the per-course/per-player best-time and best-score tables from the current result and prints "Player 1/2 WIN!!". [inferred] | `0x0020F3B8`, `0x00235B58`, `0x002364B0` | `0x0023D8C8`, `0x0023DCF8`, `0x0027EA30` |
| `0x00238950` | `AwardRaceResult` | Chooses a win/lose outcome value for message types '&' (0x26) and '(' (0x28) from race stats and passes it to FUN_0023d8c8. [inferred] | `0x0020F3B8` | `0x0023D8C8` |
| `0x00238A48` | `DrawRaceSummaryRow` | Draws a one-line race summary (course name, rank) plus three FUN_00237680 rows in the column selected by arg2 bit 0. [inferred] | `0x00267248`, `0x0026CF20` | `0x00204310`, `0x00237680` |

