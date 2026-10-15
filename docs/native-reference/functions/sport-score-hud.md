# Sport Score Displays

Mini-game sport scoring overlays — ski jump, curling, figure-eight, discipline leaderboards — plus medal/wheel widgets, course progress scorecards and the result-deck callbacks.

Entries marked *(unresolved)* carry a placeholder `UNKNOWN_` symbol; their behaviour below comes from decompilation notes only.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00264D50` | `UNKNOWN_0x00264D50` *(unresolved)* | Applies a perspective warp to the map marker overlay for the current wheel/slider value. | `0x00265568`, `0x00265B30`, `0x002665B8`, `0x00267248` | `0x0024EB10`, `0x00274938`, `0x00274A10` |
| `0x00264FF0` | `UNKNOWN_0x00264FF0` *(unresolved)* | Renders a small row of value tiles for the current selection state without changing the underlying state. | `0x00265568`, `0x00265B30`, `0x002665B8`, `0x00267248` | `0x0024F728`, `0x0024F7C8`, `0x0024F928` |
| `0x00265128` | `UNKNOWN_0x00265128` *(unresolved)* | Builds a stretched icon/beam sprite for the current ride counter and pushes it into the render queue. | `0x002652B8`, `0x00265568` | `0x0024EB10`, `0x0024F640`, `0x0027E840` |
| `0x002652B8` | `UNKNOWN_0x002652B8` *(unresolved)* | Renders the per-vehicle result banner and writes any visited-house text for the current side. | `0x00265568`, `0x00265B30` | `0x0023D628`, `0x0023D6A0`, `0x0024EA00`, `0x0024F530`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0024F928`, `0x00265128`, `0x0027EEE0`, `0x0027F2F4` |
| `0x00265B30` | `UNKNOWN_0x00265B30` *(unresolved)* | Drives a medal/slider wheel with directional motion and spawns the next event when the player accepts. | `0x0026B848`, `0x00217148` | `0x00206D28`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0024F928`, `0x00258E50`, `0x00264D50`, `0x00264FF0`, `0x002652B8`, `0x00204DE8`, `0x00204C48` |
| `0x00265FE8` | `UNKNOWN_0x00265FE8` *(unresolved)* | Adjusts a digit by a directional button increment while enforcing wrap-around and sound-on-change logic. | `0x002665B8` | `0x00258E50` |
| `0x002660F8` | `UNKNOWN_0x002660F8` *(unresolved)* | Draws the per-digit score column and secondary value tiles for the current rank/score state. | `0x002665B8` | `0x0023D8E8`, `0x0024EB10`, `0x0024F4A0`, `0x0024F4E0`, `0x0024F640`, `0x0024F728`, `0x0024F7C8` |
| `0x002665B8` | `UNKNOWN_0x002665B8` *(unresolved)* | Runs the final score/wheel state machine, updating the displayed total and spawning the next view. | `0x0026B848`, `0x00217148` | `0x0023D8E8`, `0x0024F148`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0024F928`, `0x00258E50`, `0x00264D50`, `0x00264FF0`, `0x00265FE8`, `0x002660F8`, `0x00204DE8`, `0x00204C48` |
| `0x00266B78` | `UNKNOWN_0x00266B78` *(unresolved)* | Prints the timer/elapsed value for the score banner when the record flag is enabled. | `0x00267248` | `0x00203FC8`, `0x0027EEE0`, `0x0027F2F4` |
| `0x00266CE8` | `UNKNOWN_0x00266CE8` *(unresolved)* | Draws the ski-jump name/value label and its score line for the active course result. | `0x00267248` | `0x00203FC8`, `0x0027EEE0`, `0x0027F2F4` |
| `0x00266DF8` | `UNKNOWN_0x00266DF8` *(unresolved)* | Draws the curling score label and current value text for the active game state. | `0x00267248` | `0x00203FC8`, `0x0027EEE0`, `0x0027F2F4` |
| `0x00266F10` | `UNKNOWN_0x00266F10` *(unresolved)* | Draws the figure-eight scoring block and the helper labels associated with it. | `0x00267248` | `0x00203B78`, `0x00203FC8`, `0x0027EEE0` |
| `0x00267060` | `UNKNOWN_0x00267060` *(unresolved)* | Prints the per-row total values for the discipline leaderboard and aligns them in the HUD. | `0x00267248` | `0x00203FC8`, `0x0027EEE0`, `0x0027F2F4` |
| `0x00267248` | `UNKNOWN_0x00267248` *(unresolved)* | Drives the sport-score display and spawns the right result panel for the final ranking state. | `0x0026B848`, `0x00217148` | `0x00206D28`, `0x00238A48`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0024F928`, `0x00258E50`, `0x00264D50`, `0x00264FF0`, `0x00266B78`, `0x00266CE8`, `0x00266DF8`, `0x00266F10`, `0x00267060`, `0x00204DE8` |
| `0x00267DB0` | `UNKNOWN_0x00267DB0` *(unresolved)* | Flushes the high-level result deck by invoking the current finalizer callback. | — | `0x00267A80` |
| `0x00267F08` | `UNKNOWN_0x00267F08` *(unresolved)* | Flushes the status callback for the next event by invoking its initializer helper. | — | `0x002678E0` |
| `0x00267F20` | `UNKNOWN_0x00267F20` *(unresolved)* | Invokes the score-entry teardown routine for the result-list state. | — | `0x002679C8` |
| `0x00267F38` | `UNKNOWN_0x00267F38` *(unresolved)* | No-op stub used as a placeholder while the state machine advances. | — | — |
| `0x00269F48` | `UNKNOWN_0x00269F48` *(unresolved)* | Draws the course progress list and score card, allowing selection and update of the active race entry. | `0x0020A580` | `0x00202350`, `0x00203B78`, `0x00203F40`, `0x00203FC8`, `0x00205350`, `0x00205C20`, `0x00205DA0`, `0x0023DB00`, `0x00258E50`, `0x0027EEE0`, `0x00204DE8`, `0x0027F1E0` |
| `0x0026A928` | `UNKNOWN_0x0026A928` *(unresolved)* | Scrolls and renders the course text ticker while it fades out as time advances. | `0x0026CB60`, `0x0026D2C0` | `0x00203FC8`, `0x0027E840`, `0x0027EA30` |

