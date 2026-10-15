# Stunt Cards & Name Entry

The stunt-card flow (stunt list with per-stamp awards, select menu, results/HUD screen) and the character-grid name entry with its D-pad navigation and commit tasks.

Entries marked *(unresolved)* carry a placeholder `UNKNOWN_` symbol; their behaviour below comes from decompilation notes only.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0025A120` | `UNKNOWN_0x0025A120` *(unresolved)* | Stunt-card task: state machine driving a stunt list (per-stamp awards, line passes, fade to results), drives menu/scissor/music/draw helpers | — | `0x00204D60`, `0x002080F8`, `0x0020DF50`, `0x0021C928`, `0x00233158`, `0x00233860`, `0x00233FC8`, `0x00234128`, `0x00234C80`, `0x00235050`, `0x0023D8C8`, `0x00242D20`, `0x00243398`, `0x0024FB38`, `0x0024FB80`, `0x00258E50`, `0x00258FB0`, `0x00258FF0`, `0x0025AA58`, `0x0025B500`, `0x00242A88`, `0x00258EC8`, `0x0023C8A8`, `0x00204DE8`, `0x00204C48`, `0x0027E840`, `0x002729D0` |
| `0x0025AB28` | `UNKNOWN_0x0025AB28` *(unresolved)* | Stunt-select menu renderer: builds a centered 5-row text list with left/right accept/cancel, draws header/icons and drives input -> selection state | `0x0025A120` | `0x00202350`, `0x00203B78`, `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x0024F530`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0024F928`, `0x00258E50`, `0x0027EEE0`, `0x00204DE8`, `0x0027F1E0` |
| `0x0025B0C0` | `UNKNOWN_0x0025B0C0` *(unresolved)* | Stunt results/HUD screen: renders title, per-stamp stats (time/score, bests, new-record markers) with button-input confirmation handling | `0x0025A120` | `0x00202350`, `0x00203B78`, `0x0024F530`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0024F928`, `0x00258E50`, `0x0027EEE0`, `0x00204DE8` |
| `0x0025B5A0` | `UNKNOWN_0x0025B5A0` *(unresolved)* | Draws name-entry grid: header strip + small alphabet/character grid (with wrap rules) at layout y depending on selection mode | `0x0025BCF0`, `0x0025BE30` | `0x00202350`, `0x00203098`, `0x00203F40`, `0x00203FC8` |
| `0x0025B798` | `UNKNOWN_0x0025B798` *(unresolved)* | Name-entry input: handles accept/cancel + per-letter commit; advances slot when full and transitions to confirm state, plays SFX | `0x0025BCF0` | `0x00258E50` |
| `0x0025B908` | `UNKNOWN_0x0025B908` *(unresolved)* | Name-entry letter D-pad navigation: translates D-pad axes to neighbor letter (with edge wrap) using directional adjacency, plays cursor SFX | `0x0025BCF0` | `0x00258E50` |
| `0x0025BB08` | `UNKNOWN_0x0025BB08` *(unresolved)* | Renders the committed-name preview list under the character grid: title row + per-slot name/dash, centered within a frame | `0x0025BCF0`, `0x0025BE30` | `0x00202350`, `0x00203F40`, `0x00203FC8`, `0x0027F1E0` |
| `0x0025BCF0` | `UNKNOWN_0x0025BCF0` *(unresolved)* | Name-entry controller task: ticks per-frame letter navigation/typing, eventually concatenates name into a buffer and stores it on the parent | `0x0023BE18`, `0x0026B848`, `0x0026D550`, `0x0026D5E0` | `0x00203240`, `0x00203990`, `0x0025B5A0`, `0x0025B798`, `0x0025B908`, `0x0025BB08`, `0x0027EF70` |
| `0x0025BE30` | `UNKNOWN_0x0025BE30` *(unresolved)* | Name-entry exit/confirm task: fades highlight, ends input when slot=0x10, kills self and notifies parent on completion | `0x0025BCF0` | `0x00203990`, `0x0025B5A0`, `0x0025BB08`, `0x00204DE8` |

