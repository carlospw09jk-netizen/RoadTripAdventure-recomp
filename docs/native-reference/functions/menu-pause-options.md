# Pause & Options Menus

The pause menu tree — main menu, submenus, restart/quit confirmation — plus audio settings, controller button remapping, the world map page, and the per-frame options screen loop.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00212AC8` | `UpdateVerticalMenuNavigation` | Updates vertical menu cursor navigation, handles scrolling, plays navigation sound effects, and steps transition animations. | `0x00214AD0`, `0x00214EF8` | `0x00206D28`, `0x00258E50` |
| `0x002135C8` | `HandleConfirmationMenuInput` | Handles navigation, confirmation, and cancel input for a three-option dialog menu [inferred]. | `0x00215220` | `0x00206D28`, `0x00258E50` |
| `0x002137B0` | `HandleAudioSettingsMenuInput` | Handles audio options menu input, updating BGM and sound effect volume levels and settings [inferred]. | `0x002156D0` | `0x00206D28`, `0x002589A0`, `0x00258A48`, `0x00258AF0`, `0x00258B30`, `0x00258E50`, `0x002590B0` |
| `0x00213A78` | `HandleToggleMenuInput` | Manages menu transition states and toggles status bit 0x80 on selection or cancellation [inferred]. | `0x002161E0` | `0x00258E50` |
| `0x00213C40` | `GetControllerButtonConfig` | Converts controller button bindings from player state offset +0x12e8 into configuration index values. | `0x00214220`, `0x00214470` | — |
| `0x00214078` | `SetControllerButtonConfig` | Writes configured button assignments from menu selection indices back into player control mapping fields. | `0x00214220`, `0x00214470` | — |
| `0x00214220` | `ResetControllerConfigDefaults` | Resets controller button bindings to defaults, checks for mapping collisions, and updates config fields [inferred]. | `0x00214470` | `0x00213C40`, `0x00214078`, `0x00229F20` |
| `0x00214470` | `HandleControllerConfigMenuInput` | Handles navigation and button remapping for controller configuration menu, verifying duplicate assignments. | `0x00216738` | `0x00206D28`, `0x00213C40`, `0x00214078`, `0x00214220`, `0x00258E50`, `0x00204C48`, `0x0027E5F8`, `0x0027E840` |
| `0x00214AD0` | `UpdatePauseMenuTask` | Updates pause menu navigation, processes selected menu option, and renders menu background, borders, and items [inferred]. | `0x00217148` | `0x00203240`, `0x002119A8`, `0x00211F68`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x00212AC8`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x00204C48` |
| `0x00214EF8` | `UpdatePauseSubmenuTask` | Handles navigation and renders dialog layout for a secondary pause submenu screen [inferred]. | `0x00217148` | `0x002119A8`, `0x00211F68`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x00212AC8`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8` |
| `0x00215220` | `UpdatePauseConfirmDialogTask` | Updates input and renders dialog frames and options for pause menu confirmation prompt (restart/quit) [inferred]. | `0x00217148` | `0x002119A8`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x002135C8`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x00204C48` |
| `0x002156D0` | `UpdateAudioSettingsMenuTask` | Updates audio settings menu input and renders volume sliders, time of day display, and sound option controls [inferred]. | `0x00217148` | `0x002114E0`, `0x002119A8`, `0x00211F68`, `0x00212190`, `0x00212720`, `0x00212A00`, `0x002137B0`, `0x0024EB10`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x0027E840` |
| `0x00216E00` | `UpdateWorldMapMenuTask` | Displays the pause menu world map and renders the player position marker icon at converted world coordinates. | `0x00217148` | `0x0024F530`, `0x0024F558`, `0x0024F578`, `0x0024F5C8`, `0x0024F640`, `0x0024F728`, `0x0024F7C8`, `0x00258E50`, `0x0027E840` |
| `0x00216FF8` | `InitPauseMenuOptions` | Initializes the pause menu state structure and populates available menu option lists based on feature unlock flags. | `0x002174E0`, `0x00217148` | `0x0027E840` |
| `0x002173D0` | `PromptConfigConfirmationTask` | Displays a confirmation popup dialog with options and returns the selected index to the parent task. | `0x00214470` | `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x00204DE8` |
| `0x002174E0` | `UpdateOptionsMenu` | Per-frame options menu: init state on first call, resets buffer on page change, runs settings page, kills task on exit. | — | `0x00216FF8`, `0x00216738`, `0x00204DE8`, `0x0027E840` |
| `0x0024FB38` | `SetGamePausedState` | Toggles pause flag 0x2000 at param_1+0x28 and calls sound pause (00258ff0) or resume (00259050) [inferred] | `0x00251B60`, `0x0025A120`, `0x00260DC0` | `0x00258FF0`, `0x00259050` |

