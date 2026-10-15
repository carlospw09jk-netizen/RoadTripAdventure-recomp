# Paint Editor

The car paint editor: swatch grid sprites, RGB nibble digit drawing and editing, confirm-with-payment flow, and the window slide-in/slide-out animations.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00255070` | `BuildPaintEditorMenuSprites` | Fills a 0x175b570 sprite packet buffer with a grid of menu sprites, RGB color-coded swatches (red/green/blue/magenta/cyan/yellow), and 4 text labels for the paint editor. [inferred] | `0x00256958` | `0x00206040`, `0x002063C8`, `0x002079A8`, `0x00207A28`, `0x0021D718`, `0x0021D788`, `0x0021DAE8`, `0x0021DB20`, `0x0021DB48` |
| `0x002557A0` | `DrawPaintCursorSprites` | Draws two 0x20x0x20 cursor sprites selected from a table (0x2ac600), or static sprites when blink timer is zero; second sprite driven by item index. [inferred] | `0x00256640` | `0x002063C8` |
| `0x002558B8` | `DrawPaintEditorDigits` | Builds digit sprites for 6 paint-value nibbles with dimming by player cursor, draws hex digit characters, item-count banner text, and turn arrows, into the paint editor packet buffer. [inferred] | `0x002564E8`, `0x00256640` | `0x00202150`, `0x00203CD0`, `0x002063C8`, `0x002075F0`, `0x00207998` |
| `0x002560F0` | `HandlePaintEditorInput` | Controller input for paint editor: moves cursor among 6 nibbles/palette, increments nibble of RGB value in progress record, wraps at bounds, plays beep 0x1d, swaps RGB fields. [inferred] | `0x00256640` | `0x00258E50` |
| `0x002564E8` | `PaintEditorConfirmTask` | Paint-editor confirm step: redraws digits, on commit stores edited 32-bit paint value into gs_progress_ram entry, compares to previous value and charges payment via FUN_0023dc58; toggles between confirm and edit tasks. [inferred] | `0x00256640` | `0x0022E7F0`, `0x0023DC58`, `0x002558B8`, `0x00258E50`, `0x00226F70` |
| `0x00256640` | `PaintEditorEditTask` | Paint editor main task: runs input handler, redraws menu and cursors, honors blink timer, on X/cancel transitions to confirm task FUN_002564e8, cancel dialog, or "Need more money" dialog. [inferred] | `0x002564E8`, `0x00256A78`, `0x00256958` | `0x00207A28`, `0x0022E7F0`, `0x002557A0`, `0x002558B8`, `0x002560F0`, `0x00258E50`, `0x00204C48`, `0x00226F70` |
| `0x002568D8` | `ClosePaintEditorWindow` | Window slide-out animation: decrements 16-bit timer at task+2 by 4, moves the panel sprite up with FUN_002063c8, kills task when offset hits -0x38. [inferred] | `0x002564E8`, `0x00256640` | `0x002063C8`, `0x00204DE8`, `0x00226F70` |
| `0x00256958` | `OpenPaintEditorScene` | Opens paint editor scene: copies paint value from gs_progress_ram entry, detects mirrored RGB nibbles to set grayscale flag, builds menu sprites, then starts slide-in anim or hands off. [inferred] | `0x0022AEB0` | `0x00255070`, `0x00256A78` |
| `0x00256A78` | `SlideInPaintEditorWindow` | Slide-in animation opposite of close: increments timer at task+2 by 4 until it reaches 0 while moving the panel sprite, then transitions to FUN_00256640. [inferred] | `0x00256958` | `0x002063C8`, `0x00226F70` |

