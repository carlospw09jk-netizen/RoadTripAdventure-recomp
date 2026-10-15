# Collection, Flags & Slot Queries

Progress, stamp and state bit flags; flagged-slot scanning and scoring; unlock conditions; the shared byte queues; actor slot helpers; and the knock/mail/hint collectible mask gates.

Entries marked *(unresolved)* carry a placeholder `UNKNOWN_` symbol; their behaviour below comes from decompilation notes only.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0023BF28` | `FindFlaggedSlotIndex` | Scans 160-bit per-slot flag set (5 words, stride 0xd12) for Nth set bit with optional filters in param_4; returns index or -1 | `0x0023D5B0`, `0x00246250`, `0x00267F48`, `0x002681A8`, `0x00268E40` | `0x00243670` |
| `0x0023C150` | `CountFlaggedSlots` | Counts set bits in per-slot flag set (5 words) with same filters; with param_2=-1 returns total count, else rank of given bit | `0x0023D5C8`, `0x00246250`, `0x00268E40` | `0x00207B20`, `0x00243670` |
| `0x0023C418` | `SetObjectAttribute` | Writes byte attribute at obj+id+0xc and updates obj+8 flag bits (ids 1,2,7-12) from table lookups; no callees | `0x00227C68`, `0x0022BB88`, `0x0023B168` | — |
| `0x0023C6B0` | `FindIndexByMaskedByte` | Searches a byte table for an entry matching value (param3 mod param2 + base) with mask 0x7f; returns found index [inferred]. | `0x0023DFA0`, `0x0023DFC0` | — |
| `0x0023C7B0` | `ReleaseStampFlag` | Clears stamp-bit for id at +0x21 in DAT_01780338[+0x23]; if slot mask empties calls FUN_0023cb68(0x19); sets house-stamp bit | `0x0022AEB0`, `0x0022CD38`, `0x0023AD00`, `0x0023B168` | `0x0023CB68` |
| `0x0023CB68` | `SetProgressFlagAndQueue` | Sets progress bit in DAT_0177fdd0, enqueues a code byte in 128-byte ring buffer, awards stamp 0x33 when all bits set | `0x002364B0`, `0x0023AD00`, `0x0023C7B0` | `0x00207B20`, `0x0023C8A8` |
| `0x0023CD58` | `CheckUnlockCondition` | Returns bool whether a progress gate is satisfied: table lookup of (char,char) pairs plus special cases on params and time value [inferred] | — | — |
| `0x0023D058` | `ComputeSlotScore` | Computes a weighted score for slot param_1 by summing per-slot flags/counts and table weights [inferred] | `0x00243A78` | — |
| `0x0023D2C0` | `SetFlaggedSlotBit` | Sets bit param_2 in 5-word per-slot flag set for group param_3; fills first free word, bit set in words 0-4 in order | `0x00227C68`, `0x00229F70`, `0x0022D1E0`, `0x00239378`, `0x00239F68`, `0x002406F8`, `0x0025BE98`, `0x0025C1D8`, `0x0025C300`, `0x0025C528`, `0x0025C818`, `0x0025CA08`, `0x0025D3D8`, `0x0025D4E8`, `0x00268E40`, `0x00269568`, `0x00269900` | — |
| `0x0023D390` | `ClearFlaggedSlotBit` | Clears bit param_2 in the 5-word per-slot flag set, checking words in reverse order | `0x00239508`, `0x0023AD00`, `0x00268E40` | — |
| `0x0023D488` | `GetFlaggedSlotCount` | Returns how many of the 5 per-slot flag words have bit param_2 set; for slot 0/15 returns single-word bit test | `0x002161E0`, `0x0022C648`, `0x0022C8A0`, `0x0023AD00`, `0x00242FE8`, `0x00246250`, `0x0025BE98`, `0x0025C1D8`, `0x0025C300`, `0x0025C528`, `0x0025C818`, `0x0025CA08`, `0x0025D3D8`, `0x0025D4E8` | — |
| `0x0023D548` | `TestFlaggedSlotBit` | Returns the masked bit for param_2 from the per-slot flag set (word 4, or word 0 for slot 0/15) | `0x00268E40`, `0x00269568`, `0x00269900` | — |
| `0x0023D5B0` | `CallFindFlaggedSlot` | Thin wrapper: tail-calls FindFlaggedSlotIndex (FUN_0023bf28) | `0x002161E0`, `0x0022CD38` | `0x0023BF28` |
| `0x0023D5C8` | `CallCountFlaggedSlots` | Thin wrapper: tail-calls CountFlaggedSlots (FUN_0023c150) | `0x0022C8A0` | `0x0023C150` |
| `0x0023D5E0` | `TestObjectAttribute` | Returns whether obj byte at param_2+0xc equals param_3 | `0x0023AD00` | — |
| `0x0023D5F8` | `TestStampFlag` | Returns bit param_2 of stamp word DAT_01780338[param_1]; 0 if param_2 >= 32 | `0x0022CA28`, `0x0023AD00`, `0x0026BE50` | — |
| `0x0023D628` | `TestStateBitFc78` | Tests bit (id-1) in state bitmask DAT_0177fc78; id 0 checks all-set sentinel [inferred] | `0x002105B8`, `0x0022AEB0`, `0x0022D1E0`, `0x002364B0`, `0x00237878`, `0x00239A80`, `0x0023AD00`, `0x00240CD8`, `0x00249208`, `0x0025BE98`, `0x0025C088`, `0x0025C1D8`, `0x0025C300`, `0x0025C528`, `0x0025C818`, `0x0025CA08`, `0x0025D3D8`, `0x0025D4E8`, `0x0025E630`, `0x0025F6A0`, `0x0025FBF8`, `0x002631A0`, `0x00263E68`, `0x002652B8`, `0x002678E0`, `0x002679C8`, `0x00267A80` | — |
| `0x0023D6A0` | `TestStateBitFc88` | Tests bit (id-1) in state bitmask DAT_0177fc88; returns 0/1 as 64-bit value [inferred] | `0x002652B8` | — |
| `0x0023D6E0` | `SetStateBitFc78AndFc88` | Sets bit id in both DAT_0177fc78 and DAT_0177fc88 state bitmasks [inferred] | `0x00265568` | — |
| `0x0023D730` | `SetStateBitFc88` | Sets bit (id-1) in state bitmask DAT_0177fc88 [inferred] | `0x0023AD00` | — |
| `0x0023D778` | `TestFlagBitFc98` | Returns whether bit id is set in flag bitmask DAT_0177fc98 | `0x002105B8`, `0x0022C648`, `0x0023AD00`, `0x0023F0A0`, `0x0023FCB0`, `0x00240CD8`, `0x00259138`, `0x0025C088`, `0x0025D328`, `0x00267A80` | — |
| `0x0023D7A8` | `SetFlagBitFc98` | Sets bit id in flag bitmask DAT_0177fc98 | `0x00227C68`, `0x0023AD00` | — |
| `0x0023D7E0` | `ClearFlagBitFc98` | Clears bit id in flag bitmask DAT_0177fc98 | `0x002105B8`, `0x0023AD00` | — |
| `0x0023D818` | `PopByteQueue` | Pops and returns first byte from the DAT_01782a64 queue, shifting remaining entries; returns 0 when empty | `0x00265568` | — |
| `0x0023D878` | `AwardHouseStampIfUnset` | If house slot id<10 has no stamp in DAT_01780338, awards its stamp via gs_stamps_award_function | `0x002105B8` | `0x0023C8A8` |
| `0x0023D8C8` | `GetOrSetPresidentRaceResult` | Stores param into gs_president_race_result_address when not -1; always returns the stored value | `0x0020F3B8`, `0x00235B58`, `0x00237C48`, `0x00238950`, `0x0023AD00`, `0x00252738`, `0x00254430`, `0x00254D20`, `0x002595F0`, `0x0025A120`, `0x0025DBB8`, `0x0025E278`, `0x0025E630`, `0x0025EB70`, `0x0025F6A0`, `0x0025FBF8`, `0x0025FF00`, `0x00260638`, `0x00262458`, `0x00262678`, `0x002631A0`, `0x002638C0`, `0x00263C38`, `0x00263E68` | — |
| `0x0023D8E8` | `TestFlagFc68` | Tests bit (id-1) in 128-bit flag pair DAT_0177fc68/fc70; id 0 checks all-set sentinel [inferred] | `0x0023AD00`, `0x002660F8`, `0x002665B8` | — |
| `0x0023D968` | `CountBitsFca8Fcb0` | Popcount of the two 64-bit words DAT_0177fca8/fcb0 for slot param_1 (stride 0x689) | `0x0023AD00`, `0x00241BA8` | — |
| `0x0023DA58` | `InitActorSlot` | Copies slot template fields from DAT_0177ea50[param_1] into slot param_2 and sets its class/state bytes; calls FUN_0020ad18, FUN_00250dd0 [inferred] | `0x00239378` | `0x0020AD18`, `0x00250DD0` |
| `0x0023DB00` | `GetSlotDataPtr` | Returns data pointer for slot param_2 (or resident table entry when param_1 nonzero); NULL when unset | `0x002359E8`, `0x00235F10`, `0x00239378`, `0x00239508`, `0x00239748`, `0x00239900`, `0x00247BF0`, `0x00247E38`, `0x00248580`, `0x00264498`, `0x00264CB0`, `0x00269F48` | — |
| `0x0023DB80` | `MatchSlotPairState` | Returns 0/1/2 based on whether slot pair entries DAT_0177fdd4/fdd6 match params (char pair) [inferred] | `0x0020E708`, `0x0020ED30`, `0x00210158`, `0x002105B8`, `0x00211530`, `0x00211708`, `0x00211808`, `0x0023AD00`, `0x0026BE50`, `0x0026D108` | — |
| `0x0023DBC8` | `CheckSlotPresence` | Returns 0 if both slot-A (fdd4) and slot-B (fdd6) entries are nonzero, else 1/2 [inferred] | `0x00239748`, `0x0023AD00` | — |
| `0x0023DBF0` | `TestFlagFca8Clear` | Returns true when bit id is clear in DAT_0177fca8 word set (or both words zero for id 0) [inferred] | `0x0023AD00`, `0x00241BA8`, `0x00241E90`, `0x00241D38` | — |
| `0x0023DCC0` | `TestFlagFcb8` | Tests bit param_2 in 64-bit word of DAT_0177fcb8 array (index 9 for param 0xd, else param-1) | `0x0023AD00` | — |
| `0x0023DCF8` | `SetFlagFcb8` | Sets bit param_2 in DAT_0177fcb8 word (index 9 for param 0xd, else param-1) | `0x00237C48` | — |
| `0x0023DE30` | `TestProgressFlag` | Returns whether bit param_1 is set in progress bitmask DAT_0177fdd0 | `0x00207280`, `0x00219ED8`, `0x0023AD00`, `0x0021FB38`, `0x00220030` | — |
| `0x0023DE50` | `TestProgressFlagForPair` | Looks up (char,char) pair in 25-entry table at 0x2a2b71; returns progress bit for that entry, false if not found | `0x0023AD00` | — |
| `0x0023DEB8` | `IsMapEntryEnabled` | Returns whether the mapped flag for an in-range entry is set; treats invalid entries as enabled. [inferred] | `0x00259138`, `0x0026BE50` | — |
| `0x0023DF00` | `GetQueuedByte` | Returns the low seven bits of a queued byte by index, or -1 when beyond the queue length. [inferred] | `0x00264498` | — |
| `0x0023DF38` | `AppendQueuedByte` | Appends a byte to a 128-byte queue, shifting older entries left when full. [inferred] | `0x00227C68`, `0x0022D1E0`, `0x002364B0`, `0x00241BA8` | — |
| `0x0023DFE0` | `HasQueuedByteFlag` | Returns bit 7 of the indexed byte in the shared 128-byte queue. [inferred] | `0x00249208`, `0x00264498` | — |
| `0x0023DFF8` | `SetQueuedByteFlag` | Sets bit 7 on the indexed byte in the shared 128-byte queue. [inferred] | `0x00264498` | — |
| `0x002678E0` | `KnockCollectMaskGates` | Queries 7 knock slots (0x5C,0x27,0x61,0x13,0x43,0x59,0x3B) and ORs result bits into caller mask[0]. [inferred] | `0x00267F08` | `0x0023D628` |
| `0x002679C8` | `MailCollectMaskGates` | Queries 5 mail slots (0x5A,0x2C,0x11,0x2F,0xE) and ORs check bits (4/8/0x20/0x80/0x100) into caller mask. [inferred] | `0x00267F20` | `0x0023D628` |
| `0x00267A80` | `HintCollectMaskGates` | Queries 14 hint/GPS slots (d628/d778) setting bits across mask[0..4]; maps slot IDs to collection/hint flags. [inferred] | `0x00267DB0` | `0x0023D628`, `0x0023D778` |
| `0x00267C30` | `UNKNOWN_0x00267C30` *(unresolved)* | Remaps a bitmask to a packed slot table and dispatches the matching action for the selected index. | `0x00267F48`, `0x002681A8` | — |
| `0x00267F48` | `UNKNOWN_0x00267F48` *(unresolved)* | Finds the next active bit in a mode-specific bitmask and returns its slot index or -1. | `0x00246250`, `0x00269568`, `0x00269900` | `0x0023BF28`, `0x0024F3B8`, `0x00267C30` |
| `0x002681A8` | `UNKNOWN_0x002681A8` *(unresolved)* | Counts active bits before the requested index in a packed result mask and returns the slot offset. | `0x00246250`, `0x00269568`, `0x00269900` | `0x0023BF28`, `0x0024F3B8`, `0x00267C30` |
| `0x00269BD0` | `FlagIndexGetNth` | Scans flag words (param_2<<5 bits); returns bit index of the Nth set flag, or -1; -1 count asks FUN_0024F3B8. [inferred] | — | `0x0024F3B8` |
| `0x00269C80` | `FlagIndexBitsUpTo` | Rewrite of 0x00269BD0: counts set bits before global index param_3 and reports whether that bit is set. [inferred] | — | `0x0024F3B8` |

