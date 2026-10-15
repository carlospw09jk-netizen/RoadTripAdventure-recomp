# Car Model Rendering

Showroom and in-world car model drawing: light/env and color-palette uploads, car render register templates, the curated showroom rig, and attachment point selection.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x002230C8` | `UploadPaintBgColors` | Loads 4 RGBA paintIntensity entries indexed from RAM palette bytes and vifUploadColours them (background swap). [inferred] | `0x00244F60` | `0x00226A18` |
| `0x00223150` | `UploadPackedColors` | Decodes 6 luminance indices from packed 16-bit mask, expands to paintIntensity RGBA, then vifUploadColours two 12-byte halves. [inferred] | `0x00244F60` | `0x00226A18` |
| `0x00223200` | `ResetShowroomLighting` | Selects 5-cycle DMA, uploads 16-light At16 env, and resets DAT_003356e8 (showroom render init step). [inferred] | `0x0022AEB0`, `0x0022BFD0`, `0x0022C280`, `0x0022CC38`, `0x00216738` | `0x00226F70`, `0x00226940` |
| `0x00223238` | `DrawShowroomCarModel` | Uploads light matrix + object matrix + GIF regs for a car and vifCallModel into &carModels[idx*6]+0x10; per-model showroom drawer. [inferred] | `0x00216738` | `0x00226868`, `0x00226B18`, `0x00226C28`, `0x00227048` |
| `0x00223410` | `InitCarRenderRegs` | Zeroes 0x800 bytes (DAT_0177eb10) then writes four GIF2 mscal paths (0x2b26/0x2b66) — car-render registers template. [inferred] | `0x00241490` | `0x0027E840` |
| `0x00223490` | `UploadCarColorPalette` | Loads two colours from 0x29d2d0/0x29d2e0 and kicks a 2-cycle DMA (texture/colour palette apply for car). [inferred] | `0x0022AEB0` | `0x00226F70`, `0x00226A18` |
| `0x002236A8` | `UploadCarAt16Env` | Loads At16 light env from 0x177f5e0 and starts a 4-cycle DMA (car env re-upload). [inferred] | `0x0022CC38` | `0x002267A8`, `0x00226F70` |
| `0x002236D8` | `DrawCarVariant0` | Trivial wrapper: drawCar(0,0) — used as the "no animation" car draw path. [inferred] | `0x0022AEB0`, `0x0022BFD0`, `0x0022C280`, `0x0022CC38` | `0x00221788` |
| `0x002236F8` | `DrawCarShowroomRig` | Gets showroom view (FUN_00226180), centres rig at (0,-0.5,0) via FUN_00273038, then drawCarShowroom — the curated showroom pose. [inferred] | `0x0022AEB0`, `0x0022CC38` | `0x00226180`, `0x00273038`, `0x00221F70` |
| `0x00223760` | `SelectCarAttachmentPoint` | Selects an attachment-point xy (from arg's wheel/bumper fields) by case on arg3, then vu0ApplyMatrix through view matrix param. [inferred] | `0x00244F60` | — |

