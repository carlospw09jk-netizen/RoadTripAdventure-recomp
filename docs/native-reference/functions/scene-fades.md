# Fades, Intros & Showcases

Screen fade ramps shared by menus and cutscenes, the intro/long fade sequence tasks, and the scene intro and showcase render tasks.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00211948` | `FadeOutMenuTask` | Advances a screen fade step by calling FUN_002080f8, and notifies parent task before terminating at step 0x40. | `0x00211170`, `0x00211320` | `0x002080F8`, `0x00204DE8` |
| `0x0022BFD0` | `RunSceneIntroTask` | First tick sets up transforms, camera, light and VIF upload; later ticks render and finish on input bits 0x50. [inferred] | `0x0023B168`, `0x002665B8` | `0x00206650`, `0x002075F0`, `0x0021D718`, `0x0021DAE8`, `0x0021F910`, `0x00223200`, `0x002236D8`, `0x00225790`, `0x00272C38`, `0x00273268`, `0x0021F698`, `0x00204DE8`, `0x00204C48`, `0x002261A0`, `0x00226F70`, `0x002266E8`, `0x00272A48` |
| `0x0022C280` | `RenderShowcaseView` | Initializes object matrices, camera and light, uploads via VIF and renders the model with mode param_3. | `0x00248580` | `0x0021F910`, `0x00223200`, `0x002236D8`, `0x00225790`, `0x00272C38`, `0x00273268`, `0x0021F698`, `0x002261A0`, `0x002266E8`, `0x00272A48` |
| `0x0022C418` | `RunFadeOutWaitTask` | Raises a fade flag via FUN_002080f8 and sets bit 0x400000; past tick 63 with input idle it finishes with result 5. | `0x0022AEB0`, `0x0022CD38` | `0x002080F8`, `0x00258F50`, `0x00204DE8` |
| `0x0022C4A8` | `RunFadeHoldTask` | Sets the fade flag and bit 0x400000 while tick is 0 or input is active, unless tick equals 64, then kills itself. [inferred] | `0x0020F950` | `0x002080F8`, `0x00204DE8` |
| `0x0022EAC8` | `RunFadeOutExitTask` | Fades out over 64 ticks via FUN_002080f8, then clears globals, reports result -1, kills the task and sets a completion bit. | `0x0022DC68` | `0x002080F8`, `0x00204DE8` |
| `0x00263710` | `SeqIntroFadeTask` | Task initing 0x8CBC work buffer, plays BGM flag 0xA100201, state machine ramps a short via FUN_002080F8 then kills task on done. [inferred] | — | `0x002080F8`, `0x0020DF50`, `0x0021C928`, `0x00233860`, `0x002355F0`, `0x00258FF0`, `0x00258EC8`, `0x00204DE8`, `0x00204C48`, `0x0027E840` |
| `0x002642B8` | `SeqLongFadeTask` | Task inits 0x8CC0 buffer + BGM, state machine ramps 2 values (states 0-4), sets screen fade via FUN_002080F8, kills itself. [inferred] | — | `0x002080F8`, `0x0020DF50`, `0x0021C928`, `0x00233860`, `0x00235320`, `0x00258EC8`, `0x00204DE8`, `0x00204C48`, `0x0027E840` |

