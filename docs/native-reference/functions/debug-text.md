# Debug Text & Adjuster

Text-API debug print helpers for doors, cameras, vectors and map coordinates, plus the debug adjuster menu that nudges offsets/scales and dumps its state.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0026ABD0` | `ApplyAdjusterOffsets` | Reads 0xC table type, applies signed deltas to item fields 0xB0/0xB2, flag bits adjust floats 0x90/0x94/0x98, rebuilds VU0 matrix. [inferred] | — | `0x00207EC8`, `0x00272AE8`, `0x00272B90`, `0x00272C38`, `0x00273268`, `0x00272A48` |
| `0x0026AD48` | `DrawAdjusterDebug` | Text-dumps adjuster state: two shorts at 0xB0/0xB2/0xB4 and floats 0x90/0x94/0x98 via FUN_0027EEE0+FUN_00203A50. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026AE88` | `ApplyAdjusterScales` | Applies scale ops (+1 / x399 when flags 2/8 set) to float fields 0x3C/0x15C/0x158/0x38 and prints result at 0x168. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027BA50`, `0x0027BAA8`, `0x0027BDC0`, `0x0027C110`, `0x0027EEE0` |
| `0x0026B018` | `AdjusterMenuTask` | Task creating 0x32F8D0/E/F0 menu items, selects via FUN_00205C08; status 2 toggles 0x10000 mode + plays sfx 0x1A, 0 exits task. [inferred] | — | `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x002080F8`, `0x00258E50`, `0x00204DE8` |
| `0x0026D880` | `PrintDoorParams` | Decodes door struct: pos(0-2),scale with 32FB48 label, ypr shorts *0xB4/0x8000 deg, prints via FUN_0027EEE0/FUN_00203A50. [inferred] | `0x0021F198` | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DA90` | `PrintDoorBase` | Dumps base struct to text: d 0x335630 = arg2, plus floats off param_1+0x30/0x34/0x38. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DB80` | `PrintCamPair` | Converts two floats via FUN_0027B7B0/FUN_0027EEE0 into text api calls 0x32FB78/0x32FB88 (camera near/far?). [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DC10` | `PrintCamProj` | Dumps arg2 via 32FB98 plus param_1 via 0x335668 with size 0x20 - camera aspect/fov debug print. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DC98` | `PrintUiSlotN` | Sets UI text api slot: id param_1 via a0 335670 plus current via 0x33xx label, prints with size param_1<<3. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DCF8` | `PrintXyzRow` | Converts 4 args (id + x/y/z or w) via FUN_0027B7B0 into one 0x32FBA8 text api record of size 0x50. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DDB0` | `PrintXyzTable` | Rows table (stride 0x30) at param_1: for each of 4 rows prints 4 floats as one 0x32FBA8 record at base param_2<<3. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DE80` | `DrawMapCoordinateValues` | Formats 4 vector words via FUN_0027b7b0 into template 0x32fbb8 and draws at slot 0x10 row param_2*8, used by map draw fns. [inferred] | `0x002427D0`, `0x00242A88` | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DF30` | `PrintVec2Short` | Prints 2 shorts from param_1 as one 0x32FBD0 record of size 0x10 at param_2<<3. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |
| `0x0026DFB0` | `PrintVec3Norm` | Writes 0x32FC08 (x,y,z) or normalized 0x32FBE0 (x,y,z,x/z,y/z) record for vec at param_1, size 0x10 at param_2<<3. [inferred] | — | `0x00203A50`, `0x0027B7B0`, `0x0027EEE0` |

