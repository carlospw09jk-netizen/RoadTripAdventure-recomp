# Data Table Lookups

Indexed accessors for the game's static tables — label strings, integer fields, part values and car slot entries — used by the shop and carousel UI.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0021FB20` | `GetCarSlotEntry` | Returns pointer into a 0x20-byte stride base (DAT_0029ce00) indexed by arg; used as a per-car slot accessor. [inferred] | `0x0021E690` | — |
| `0x00243468` | `DispatchByIndex16` | Jump-table dispatch: for index<16 calls function pointer from PTR_FUN_002f1240 and returns its result, else 0 [inferred] | `0x0022C8A0`, `0x00239F68`, `0x00244F60`, `0x00246250`, `0x00268428`, `0x00268890` | — |
| `0x00243490` | `GetTableEntry_ed720` | Returns table pointer at PTR_002ed720 indexed by param_2 with stride 0x44; param_1 ignored [inferred] | — | — |
| `0x002434B0` | `GetLabelEntry_eca70` | Returns string pointer from label table at 002eca70 indexed by param_2 (stride 0x1c) [inferred] | — | — |
| `0x002434D0` | `GetLabelEntry_eced0` | Returns string pointer from label table at 002eced0 indexed by param_2 (stride 0x18) [inferred] | — | — |
| `0x002434F0` | `GetLabelEntry_ed1f8` | Returns string pointer from label table at 002ed1f8 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x00243508` | `GetLabelEntry_ed328` | Returns string pointer from label table at 002ed328 indexed by param_2 (stride 0x1c) [inferred] | — | — |
| `0x00243528` | `GetLabelEntry_ed4e8` | Returns string pointer from label table at 002ed4e8 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x00243568` | `GetLabelEntry_effd8` | Returns string pointer from label table at 002effd8 indexed by param_2 (stride 0x0c) [inferred] | — | — |
| `0x00243588` | `GetLabelEntry_eff40` | Returns string pointer from Headlights label table at 002eff40 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x002435A0` | `GetLabelEntry_f0280` | Returns pointer from table at 002f0280 indexed by param_2 (stride 0x0c) [inferred] | — | — |
| `0x002435C0` | `GetLabelEntry_f02f8` | Returns pointer from table at 002f02f8 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x002435D8` | `GetLabelEntry_f03e0` | Returns pointer from table at 002f03e0 indexed by param_2 (stride 0x14) [inferred] | — | — |
| `0x002435F8` | `GetLabelEntry_f07f8` | Returns pointer from table at 002f07f8 indexed by param_2 (stride 0x0c) [inferred] | — | — |
| `0x00243618` | `GetLabelEntry_f00a0` | Returns string pointer from Normal Horn label table at 002f00a0 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x00243630` | `GetLabelEntry_f06c0` | Returns string pointer from Normal Meter label table at 002f06c0 indexed by param_2 (stride 0x0c) [inferred] | — | — |
| `0x00243650` | `GetLabelEntry_f0810` | Returns string pointer from Stylish Pattern label table at 002f0810 indexed by param_2 (stride 0x08) [inferred] | — | — |
| `0x00243670` | `DispatchByIndex15` | Jump-table dispatch: for index<15 calls function pointer from PTR_FUN_002f1280 and returns result, else 0 [inferred] | `0x0023BF28`, `0x0023C150`, `0x00243A78`, `0x00246250`, `0x00268428`, `0x00268890`, `0x00268E40`, `0x00269568`, `0x00269900` | — |
| `0x00243698` | `GetConst500` | Returns constant 500 | — | — |
| `0x002436A0` | `GetIntField_eca78` | Reads int from table at 002eca78 indexed by param_2 (stride 0x1c) [inferred] | — | — |
| `0x002436C0` | `GetIntField_eced8` | Reads int from table at 002eced8 indexed by param_2 (stride 0x18) [inferred] | — | — |
| `0x002436E0` | `GetIntField_ed200` | Reads int from table at 002ed200 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x002436F8` | `GetIntField_ed330` | Reads int from table at 002ed330 indexed by param_2 (stride 0x1c) [inferred] | — | — |
| `0x00243718` | `GetIntField_ed4f0` | Reads int from table at 002ed4f0 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x00243758` | `GetIntField_effe0` | Reads int from table at 002effe0 indexed by param_2 (stride 0x0c) [inferred] | — | — |
| `0x00243778` | `GetIntField_eff48` | Reads int from table at 002eff48 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x00243790` | `GetIntField_f0288` | Reads int from table at 002f0288 indexed by param_2 (stride 0x0c) [inferred] | — | — |
| `0x002437B0` | `GetIntField_f0300` | Reads int from table at 002f0300 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x002437C8` | `GetIntField_f03e8` | Reads int from table at 002f03e8 indexed by param_2 (stride 0x14) [inferred] | — | — |
| `0x002437E8` | `GetIntField_f0800` | Reads int from table at 002f0800 indexed by param_2 (stride 0x0c) [inferred] | — | — |
| `0x00243808` | `GetIntField_f00a8` | Reads int from table at 002f00a8 indexed by param_2 (stride 0x10) [inferred] | — | — |
| `0x00243820` | `GetIntField_f06c8` | Reads int from table at 002f06c8 indexed by param_2 (stride 0x0c) [inferred] | — | — |
| `0x00243848` | `GetNthStringByCategory` | Switch on category id 1-15 selects a string table; returns the param_3-th NUL-delimited string from it [inferred] | `0x0022E960` | — |
| `0x00243A00` | `GetPartValueU16` | Returns u16 from category tables for id 8/11/12 (else 0) indexed by param_2 [inferred] | `0x00243C50`, `0x00244F60` | — |
| `0x00243A78` | `ComputeScaledPartValue` | Scales the value from DispatchByIndex15 by two FUN_0023d058 readings (/2000 or /4000 by flag); returns int [inferred] | `0x00246250`, `0x00268428`, `0x00268890`, `0x00268E40` | `0x0023D058`, `0x00243670` |

