# Race HUD & Scoreboard

In-race HUD rendering: the per-vehicle HUD dispatch, speedometer / tachometer / boost gauges, gear and race-time formatting, and the score counter readouts.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0022E780` | `DrawSlotScoreNumber` | Draws the value at DAT_0177fdb4 for slot param_3 at (param_1, param_2*2-6) via FUN_00202d10. [inferred] | `0x002161E0`, `0x00268E40` | `0x00202D10` |
| `0x0022E7C0` | `DrawScoreCounter` | Draws the score counter DAT_0177fdb4 at fixed screen position (0x1b8,0x1a) via FUN_00202d10. | `0x0022AEB0`, `0x00261AC8` | `0x00202D10` |
| `0x0022E7F0` | `DrawScoreAndRemaining` | Draws score and clamped score-param_1 (returns -1 underflow, 1 overflow) plus a digit glyph via FUN_00202d10. | `0x002364B0`, `0x00236858`, `0x002564E8`, `0x00256640`, `0x00261638`, `0x00261DE0`, `0x00268E40`, `0x00269568`, `0x00269900` | `0x00202D10`, `0x00203E70` |
| `0x0022E8C0` | `DrawAltScoreAndRemaining` | Draws clamped (score - param_1) at (0x1b8,0x4a) with a glyph strip, returning -1/0/1 for under/ok/overflow. | `0x0023A460`, `0x00246250` | `0x00202D10`, `0x00203E70` |
| `0x0022E960` | `DrawTextListRows` | Draws up to four text rows from FUN_00243848 via FUN_00204310, laying a panel with FUN_00204568 for the first. [inferred] | `0x00246250` | `0x00204310`, `0x00204568`, `0x00243848` |
| `0x0022EA08` | `GetPlayerScoreboardPtr` | Returns the score/status pointer for entry param_1: a per-slot block when the flag nibble is 0, else a house table entry. | `0x00236858`, `0x00236D28`, `0x00237080`, `0x00237878` | — |
| `0x0022EDB8` | `DrawDualScoreNumbers` | Draws DAT_0177fdb4 at (0x140,0x2a) and DAT_017831fc at (0x1b8,0x4a) via FUN_00202d10. | `0x00248398` | `0x00202D10` |
| `0x002490A0` | `UpdateRaceHud` | Updates and dispatches HUD rendering functions for each player vehicle via jump table PTR_LAB_002a8fb0 [inferred] | `0x0020F950`, `0x00210430` | `0x0024E1A0`, `0x0027E840` |
| `0x00249458` | `CalculateSpeedometerValue` | Computes vehicle speedometer display value from velocity vector, clamping to 186 max and scaling by input range [inferred] | — | `0x0021D358` |
| `0x002494E8` | `CalculateSpeedometerDigits` | Calculates vehicle speed from velocity vector and converts the integer speed to BCD digit format [inferred] | `0x0024E1A0` | `0x0021D358`, `0x0024F4A0` |
| `0x00249520` | `CalculateTachometerValue` | Computes tachometer RPM display gauge value from vehicle engine RPM, clamping to 10000 and scaling by param_2 [inferred] | `0x0024E1A0` | — |
| `0x00249568` | `CalculateBoostGaugeValue` | Calculates boost/gauge bar value from vehicle meter field at 0x23c, clamping to 0x40000 and scaling [inferred] | `0x0024E1A0` | — |
| `0x002495B8` | `GetVehicleGear` | Returns the 1-byte current transmission gear or state at vehicle offset 0x1ff [inferred] | `0x0024E1A0` | — |
| `0x002495C0` | `GetVehicleTransmissionMode` | Maps the transmission/shift mode byte at vehicle offset 0x244 (-1, 1, or other) to 1, 2, or 0 [inferred] | `0x0024E1A0` | — |
| `0x002495E8` | `GetVehicleRaceTime` | Returns the 32-bit time/counter field at vehicle structure offset 0x14 [inferred] | — | — |
| `0x002495F0` | `FormatRaceTimeBCD` | Converts tick timer at offset 0x14 into packed MM:SS digit format with colon separator [inferred] | — | — |
| `0x00249670` | `GetRaceTimeRank` | Evaluates race elapsed time at offset 0x14 against duration thresholds and returns a rank index [inferred] | — | — |
| `0x0026D700` | `DrawRaceHudNumbers` | Packs timer (+0xa0/+0xa8) and formats counters +0x90..0x98, +0x19c, +0x1f8 into numeric HUD slots via FUN_0027eee0/FUN_00203a50. [inferred] | `0x0021BA70` | `0x00203A50`, `0x0021C8D0`, `0x0027B7B0`, `0x0027EEE0` |

