# Slot & Betting UI

The slot-machine betting flow: menu cursor resolution, bet-amount selection, yes/no confirmation dialogs, reel result state, and the payout/results screen that credits funds.

Entries marked *(unresolved)* carry a placeholder `UNKNOWN_` symbol; their behaviour below comes from decompilation notes only.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00261378` | `UNKNOWN_0x00261378` *(unresolved)* | Menu-list cursor resolver: maps D-pad axes to next menu index using per-row neighbor table at DAT_002d9984..0x2d9994; plays SFX on change [inferred] | `0x00261AC8` | `0x00258E50` |
| `0x00261508` | `UNKNOWN_0x00261508` *(unresolved)* | Yes/No confirmation task: lays out a small text panel with two options, awaits FUN_00205c08; returns chosen index to parent and kills self [inferred] | `0x00261638` | `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x00258E50`, `0x00204DE8` |
| `0x00261638` | `UNKNOWN_0x00261638` *(unresolved)* | Bet-amount selection task: arrow keys step a clamped credit value by a power-of-10 step; X confirms -> spawns FUN_00261508; draws amount + range indicator [inferred] | `0x00261AC8` | `0x00202350`, `0x00203E70`, `0x00203FC8`, `0x002046C0`, `0x00206D28`, `0x0022E7F0`, `0x0023DC58`, `0x0024F4E0`, `0x00258E50`, `0x00204DE8`, `0x00204C48` |
| `0x00261AC8` | `UNKNOWN_0x00261AC8` *(unresolved)* | Shop/menu flow task: setup viewport/graphics, then state machine (intro -> bet select -> confirm -> result -> fade) calling helpers 0x261378,0x261638,0x2624b8 [inferred] | `0x00260DC0` | `0x002080F8`, `0x0022E7C0`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x00258E50`, `0x00258FB0`, `0x00261378`, `0x00204DE8`, `0x00204C48`, `0x0027E840` |
| `0x00261DE0` | `UNKNOWN_0x00261DE0` *(unresolved)* | Results/payout screen: draws history rows + final amount; on cross button calls FUN_0023dc58 to credit funds and notifies parent with code 2 [inferred] | `0x00260DC0` | `0x002080F8`, `0x0022E7F0`, `0x0023DC58`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x00258E50`, `0x00204DE8`, `0x0027E840` |
| `0x00262358` | `UNKNOWN_0x00262358` *(unresolved)* | Updates the slot-state result and stores the current/next reel index and win flag for the machine state. | `0x00260DC0` | — |
| `0x00262458` | `UNKNOWN_0x00262458` *(unresolved)* | Compares current credits to target and triggers the win/lose result signal for the slot outcome. | `0x00260DC0` | `0x0023D8C8` |
| `0x002624B8` | `UNKNOWN_0x002624B8` *(unresolved)* | Confirms a bet choice and resolves the selection callback when the confirm button is accepted. | `0x00261AC8` | `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x00258E50`, `0x00204DE8` |
| `0x002625A0` | `UNKNOWN_0x002625A0` *(unresolved)* | Handles the no/deny decision path for the bet dialog and closes it on confirmation. | `0x00261638` | `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x00258E50`, `0x00204DE8` |

