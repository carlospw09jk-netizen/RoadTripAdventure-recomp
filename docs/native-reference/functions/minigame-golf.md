# Golf Mini-Game

The 9-hole golf mini-game: ball launch and physics with terrain bounce, cup attraction and sinking, course/pars tables, hole intro banner and scorecard drawing.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00250F00` | `InitMiniGameBallState` | Initializes mini-game ball entity position, camera, and velocity physics state for the selected hole/stage [inferred] | `0x00251B60` | `0x0021C928`, `0x0024FB80`, `0x0027E840` |
| `0x00250FE0` | `CalculateBallLaunchImpulse` | Calculates mini-game ball hit impulse vector based on player aim and power meter [inferred] | `0x00251B60` | `0x002727C0`, `0x002727E8`, `0x00272958`, `0x00272988`, `0x00272A38`, `0x00274B10`, `0x002729D0` |
| `0x00251190` | `ApplyCupAttraction` | Checks mini-game ball proximity to hole and applies attraction/sink velocity when near the cup [inferred] | `0x00251B60` | `0x002727C0`, `0x002727E8`, `0x00272958`, `0x00272988`, `0x00274938`, `0x00274A10`, `0x00274B10`, `0x002729D0` |
| `0x00251440` | `UpdateBallPhysics` | Simulates mini-game ball physics: terrain collision, bounce reflection, friction, rotation, and stop check [inferred] | `0x00251B60` | `0x00258E50`, `0x002727C0`, `0x002727E8`, `0x00272940`, `0x00272958`, `0x00272988`, `0x00272CE0`, `0x002732B8`, `0x00274B10`, `0x00272728`, `0x002729D0`, `0x00272A48` |
| `0x00251B60` | `UpdateGolfMiniGame` | Main task state machine for the 9-hole golf mini-game, managing stages, player shot input, and scorecard [inferred] | — | `0x00207F88`, `0x00208008`, `0x002080F8`, `0x0020DF50`, `0x00233338`, `0x00233860`, `0x00233EF0`, `0x00234E98`, `0x00234F20`, `0x00234F38`, `0x00234F50`, `0x00234F68`, `0x00242D20`, `0x00243398`, `0x0024FB38`, `0x00250F00`, `0x00250FE0`, `0x00251190`, `0x00251440`, `0x00252608`, `0x00252738`, `0x00253218`, `0x00258E50`, `0x00258F30`, `0x00258F50`, `0x00258FB0`, `0x002727C0`, `0x002727E8`, `0x00272958`, `0x00273268`, `0x00274B10`, `0x00242A88`, `0x00258EC8`, `0x00204DE8`, `0x00204C48`, `0x0027E840`, `0x002729D0` |
| `0x00252608` | `InitGolfCourseTable` | Initializes 9-hole golf course pars, distances, high scores, and clears player scorecard array [inferred] | `0x00251B60` | — |
| `0x00252738` | `RecordGolfHoleScore` | Records stroke count for current golf hole against par, updates total score, and saves course records [inferred] | `0x00251B60` | `0x0023D8C8` |
| `0x00252880` | `DrawGolfHoleIntro` | Displays golf hole preview banner showing hole number, par, and yardage before play begins [inferred] | `0x00251B60` | `0x00202350`, `0x0024F530`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0024F928`, `0x00258E50`, `0x0027EEE0`, `0x00204DE8` |
| `0x00252CB0` | `DrawGolfScorecard` | Renders 9-hole golf scorecard table displaying hole pars, player scores, and relative totals [inferred] | `0x00251B60` | `0x00202350`, `0x00203F40`, `0x0024F530`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0024F928`, `0x00258E50`, `0x0027EEE0`, `0x00204DE8`, `0x0027F2F4` |
| `0x00253218` | `SetGolfCurrentHole` | Stores the active golf hole index at offset 0x8008 in the golf state structure [inferred] | `0x00251B60` | — |

