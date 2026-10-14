# How the game draws 2D (menus, HUD, text, fades) and sets GS state

Survey for Phase 5 (no VU1/GS emulation; the game's draw calls feed a native renderer). Branch
`re/native-2d-survey`, on top of `re/native-render` (`docs/native-render.md` has the 3D side: the
VIF1 packet helpers, VU1 entries, models). Addresses are the USA release (SLUS-20398). Names are in
`config/game_symbols.toml` (the new ones are marked "2D survey" there) and show up in
`scripts/ghidra/fn.py`.

What is verified and how: the pseudocode (Ghidra, `scripts/ghidra/fn.py`) of every function named
below was read, and every GIF packet and tag template quoted here was **decoded from the bytes of
the ELF's data** (a small GIF-tag/A+D decoder; the GS register layouts are the standard ones).
"Inferred" marks scene names taken from strings or call shape, and statements about volume that
come from the GS state survey (below) instead of from a per-scene run. Nothing here was run on the
GPU: the survey files are the existing ones (`build/scratch/hwgs/survey-*.tsv`, 13 processes of the
regression suite, 2,188,621,031 vertex kicks).

## 1. The three paths and the frame

All GS traffic of the game is one of:

| Path | How it leaves the EE | Carries |
|---|---|---|
| PATH1 | VU1 microprograms (`vifCallModel` chains, MSCAL) | all 3D: terrain, cars, props, sky, minimap (`docs/native-render.md`) |
| PATH2 | **`vifDirect` 0x226F70**: a VIF1 `FLUSH` + `DIRECT n` for a GIF packet that lies anywhere in EE RAM, sent by a REF tag (`{qwc|0x30000000, addr, 0x11000000, qwc|0x50000000}`) | every 2D draw, every GS state change of the scenes (TEST/ALPHA/SCISSOR/FOG packets), the frame blend |
| PATH3 | **`sendTexturePackets` 0x207B50** (DMA channel 2 straight into the GIF, chain mode, CHCR 0x105) and libgraph's `sceGsExecLoadImage` 0x26EB30 | texture uploads (43 call sites: loaders, .GSL files), and the title/logo pictures (an upload chain with a sprite at its end) |

`vifDirect` is the **only** writer of a DIRECT VIF code (grep of every function: the other
`0x50000000` constants are unrelated), and `D2_CHCR`/`D1_CHCR` are only touched by `vifKick`,
`sendTexturePackets` and libgraph (with its libdma wrapper `sceDmaSend` 0x2704D0, whose only caller 0x2710F0 is libgraph code). So a hook at `vifDirect` (and one at `sendTexturePackets`) sees
every non-VU1 GS write the game makes, except libgraph's draw/display environments (below).

The frame (`FUN_002003B8`, named `gameMainLoop`; the endless loop after boot):

1. `vifBeginPacket(0)` starts the **main chain** (buffer 0x1750BF0..0x17523D0, only 6 KB: it holds
   REF tags, not data).
2. `gs_tasks_walker(0x177A0D0, 0x335900)` runs every task. A scene that draws 3D builds its own
   chain (`vifBeginPacket(1)`, models, `vifEndPacket(1)`, `vifKick`) and kicks it at once, so 3D
   reaches the GS before the main chain; 2D calls `vifDirect` into whichever chain is open (inside
   a scene's own chain for backdrops, as the shop's packet from 0x22AEB0, else into the main chain).
3. `uiBatchFlush` (0x203810): the main sprite batch (section 3), the topmost layer.
4. `frameBlend` (0x208120) unless `drive_flags & 0x8000` (section 4).
5. `vifEndPacket(0)`, `vifKick`, `sceGsSyncPath`, `sceGsSyncV`, the field half-offset
   (`FUN_0026F340` / `FUN_0026F3C8`, `sceGsSetHalfOffset`), `sceGsSwapDBuffDc` (DISPFB and the draw
   env of the other buffer), `DAT_00335908 ^= 1`.

Display facts: `DAT_00333288` (named `videoMode`) is 0 for 640x224 fields (`sceGsSetDefDBuffDc(env,
CT32, 0x280, 0xE0, ZTST GEQUAL, Z24, clear)`) and non-zero for 512x256 (PAL). `DAT_00333284` is the
language (0 EN, 1 FR, 2 DE, ...; indexes the hint-bar and "PRESS START" strings). All UI is
authored in a **640 x 448** space: every UI vertex writer computes, for NTSC, `X = (x << 4) +
0x6C00` and `Y = (y << 3) + 0x7900` (the GS offsets 1728 and 1936 in 1/16 pixel units: the 224-line
field is centred in the primitive space, y counts half lines); for PAL `X = (x << 6) / 5 + 0x7000`,
`Y = (y << 3) + 0x7800` (x scaled by 0.8 to 512). So a native renderer can take UI coordinates as
(640x448) pixels and ignore the field/PAL arithmetic.

libgraph (standard `sceGsSetDefDBuffDc`/`DrawEnv`/`DrawEnv2`/`Clear`, 0x26F450, 0x26FB90,
0x26FF68, 0x26FD78): both contexts get FRAME (FBP per buffer, FBW 10, CT32), ZBUF (Z24, ZMSK 0, ZBP
0x8C), XYOFFSET (1728, 1936), SCISSOR (0..639, 0..223), PRMODECONT 1, COLCLAMP 1, DTHE 0, TEST
`0x50000` (ZTE on, ZTST GEQUAL: `sceGsSetDefDrawEnv` turns its `ztest` argument 2 into `(2 << 17) | 0x10000`).
The clear packet (`sceGsSetDefClear`; `FUN_002000C8` also stores TEST `0x32001` in both clear packets: ATE NEVER,
AFAIL ZB_ONLY, ZTST ALWAYS) is a sprite that rewrites only Z. Context 1 is the UI's, context 2 the
3D's (every PATH1 model and the 3D state packets use `_2` registers and `CTXT = 1`).

## 2. GIF tag templates (ELF data) and the packet builders

Tags are copied from the ELF by small builders; decoded:

| Tag | Builder | NLOOP | EOP | PRE/PRIM | FLG | REGS | Use |
|---|---|---|---|---|---|---|---|
| 0x29CBC0 | `gifAdPair` 0x21DB48 | 1 | 1 | no | PACKED | A+D | one A+D write (data, register): colour (RGBAQ) or any register |
| 0x29CBD0 | `gifSetTexRegs` 0x21D788 | 1 | 1 | no | REGLIST | TEX0_1, CLAMP_1 | texture switch; `uiArenaSetTexture` patches the 2nd register to TEX0_1 (CLD 0 + CSA = bank) |
| 0x29CBE0 | `gifSetSpriteCount` 0x21DAE8 | n*5 | 1 | yes: PRIM 0x156 | PACKED | A+D | the data tag of a sprite batch (SPRITE, TME, ABE, FST, context 1) |
| 0x29CBF0 | `gifQuadTag` 0x21DB20 | 1 | 1 | yes: PRIM 0x154 | PACKED | CLAMP_1, UV, XYZ2, UV, XYZ2, UV, XYZ2, UV, XYZ2 (NREG 9) | a textured **triangle strip** quad (rotated sprites) |
| 0x29CC00/10/20/30 | `gifTexQuadTag` 0x21DAB8 (0x29CC20), others | 0 (patched) | 1 | yes: TSTRIP, IIP/TME/FGE vary, ABE | PACKED | ST, RGBAQ, XYZF2 | EE-built Gouraud strips with STQ (VU0-transformed) |
| 0x29CC40/50 | | 4 | 1 | yes: TSTRIP, ABE 0 | PACKED | ST/RGBAQ/XYZF2 or RGBAQ/XYZF2 | opaque variants |
| 0x29CC60 | `gifFlatRectTag` 0x21DBE0 | 2n | 1 | yes: LINESTRIP (IIP, ABE) | PACKED | A+D | used only by `uiArenaFlatRects` 0x24F148 (the survey never saw a line draw: unused path, guess) |
| 0x29CC70 | `gifBeginSpriteBatch` 0x21D718 | 5 | 1 | no | PACKED | A+D | the 5 state writes of a batch header (below) |
| (builder 0x207D80) | `gifChainEndTag` | | | | | | builds a one-tag DMA chain (END, qwc) for `sendTexturePackets` |

**Batch header** (`gifBeginSpriteBatch(base, tex0)` 0x21D718, 6 qw): tag 0x29CC70 then A+D
`TEST_1 = 0x5001B` (ATE GEQUAL aref 1 KEEP; ZTE, ZTST GEQUAL), `ALPHA_1 = 0x44`
((Cs-Cd)*As+Cd), `TEX1_1 = 0x60` (linear mag/min, no mips), `TEX0_1 = tex0`, `CLAMP_1 = 5`
(CLAMP, CLAMP). Followed by the **data tag** (`gifSetSpriteCount`), then the entries.

**Sprite entry** (5 A+D qwords = 0x50 bytes), written by the `uiSprite*` family (section 3):

| qw | A+D reg | data |
|---|---|---|
| 0 | CLAMP_1 (8) | `WMS = WMT = 2` (REGION_CLAMP), `MINU = u`, `MAXU = u+w-1`, `MINV = v`, `MAXV = v+h-1` (the atlas cell is the clamp region) |
| 1 | UV (3) | `(u*16, v*16)` of the first corner (negative w/h swap the corners: mirroring) |
| 2 | XYZ2 (5) | corner 1: X, Y (formulas of section 1); its Z is not written by the emitter (irrelevant for SPRITE) |
| 3 | UV (3) | `((u+w)*16 + 16, (v+h)*16 + 16)`: one texel more than the screen rectangle (`w` pixels): a native renderer that filters bilinearly must reproduce the +1 |
| 4 | XYZ2 (5) | corner 2, **Z = 0xFFFFFE** (the arena emitters use `0xFFFFFF - (record.z + base)`) |

Hiding a sprite = replacing the XYZ2 register selectors (0x5) with 0xD (XYZ3, no kick): `uiHideSprite`
0x207998, used by the stamp book to blank uncollected stamps. A native decoder must honour XYZ3.

State packets sent with `vifDirect` (prebuilt in the ELF; decoded with their registers):

| Address (qw) | Sent by | Content |
|---|---|---|
| 0x29AB70 (6) | `uiFade` 0x2080F8 (colour patched at 0x29ABB0) | PRIM SPRITE ABE; ALPHA_1 0x44; TEST_1 0x3180A (ATE off, ZTST ALWAYS, AFAIL FB_ONLY); flat quad (0,0)-(640,224) with RGBAQ = the colour (A = fade strength) |
| 0x29F510 / 0x29F560 (5) | `FUN_0022EA78` / `FUN_0022EAA0` (from 0x25EB70) | the same flat quad, the top / bottom half band (letterbox bars; inferred) |
| 0x29AA50 / 0x29A930 (9) | `frameBlend` 0x208120 | NTSC (0x29A930): TEX0_1 = the *other* frame buffer (CT24, 640 wide, TH 256, DECAL; TBP `buf * 0x8C0`), ALPHA_1 `0x2000000064` ((Cs-Cd)*FIX+Cd, FIX 0x20), TEST_1 0x30802, CLAMP (0..639, 0..447), one sprite (0,0)-(640,224) with the texture. The `+0x90` variants (flag `drive_flags >> 22 & 1`, set for a frame after a scene change): FIX 0x40. PAL (0x29AA50): 512x256, TBP `buf << 11` |
| 0x29D0F0 (6) | `FUN_002207D0`, `FUN_002212C8` (3D scenes, car shadows) | PRIM `0x246` (SPRITE ABE, context 2), ALPHA_2 `0x8000000052` ((0-Cs)*Ad+Cd), TEST_2 `0x7D80B` (DATE, DATM 1, ATE GEQUAL 0x80), full-screen flat sprite RGBAQ 0x20: the shadow darkening pass (3D side) |
| 0x29CFC0, 0x29CFF0, 0x29D240, 0x29D290, 0x29D2F0, 0x29D3B0, 0x29D4A0 | 3D scenes (`drawScene*`, `drawCars*`, props, sky) | TEST_2/ALPHA_2/TEX1_2/CLAMP_2/TEX0_2 combinations (TEST_2 0x537FB, 0x737FB, 0x5380B, 0x5001B, 0x53001, 0x3001A; ALPHA_2 0x44 / 0x48) |
| 0x29D020 (2) | race/action scenes | SCISSOR_2 (0..639, 0..223) |
| 0x2A43B0 / 0x2A43F0 / 0x2A4410 / 0x2A4430 | `gs_map_draw_functions_*` | TEST_2 0x3380B + ALPHA_2 0x44, then a SCISSOR_2 for the minimap window (x 45..158 y 156..207; x 36..126 y 156..207; x 48..208 y 68..200) |
| 0x2A8B20 (3) | `drawCarouselScene` 0x2455C0, `FUN_00246250` | TEST_2 0x537FB + ALPHA_2 0x44 |

## 3. The 2D batchers

There are four ways the game assembles a 2D packet. All end in `vifDirect` (or, for pictures,
`sendTexturePackets`).

### 3.1 `UiBatch`: the immediate-mode library (0x2008C0..0x2046C0, 0x2063C8..)

Globals: `uiBatchPtr` 0x335680 (current buffer), `uiBatchCount` 0x335684 (entries, qwords),
`uiBatchPalette` 0x335688 (the TEX2 CSA currently set, -1 = none). Two buffers `0x3362E0` and
`0x340350`, 0xA070 bytes each: the header (6 qw), the data tag at +0x60, then up to **0xA00 entries**
from +0x70 (emitters stop at 0x9FC: 511 sprites a frame at most). `uiBatchInit` 0x2038F8 builds
both headers with TEX0 `0x2013780E65321A40`: TBP0 0x1A40, TBW 8, PSMT8 512x512, TCC 1, DECAL, CBP
0x1BC0, CPSM CT16, CLD 1 (the HUD/font atlas and its 16-bank palette). `uiBatchFlush` 0x203810
(once a frame, after all tasks): appends a TEXA entry if there is room, patches NLOOP in the data
tag (`count`), `vifDirect(base, count + 7)`, and starts the other buffer with two entries:
`ALPHA_1 = 0x8000000044` and `TEXA = 0x4000008080` (TA0 0x80, AEM 1, TA1 0x40) (`count = 2`).

The atlas is used as 4-bit with 16 palettes: `uiSetPaletteBank` 0x2039E8 (and the inlined copies
in every drawer, cached in `uiBatchPalette`) writes `TEX2_1 = (bank << 56) | 0x13780001400000`:
PSMT4, CBP 0x1BC0, CPSM CT16, CSM1, **CLD 0** (no reload; the header's TEX0 loaded the CLUT). Banks
seen: 0 (default), 1, 2, 5, 7, 8, 0xB, 0xD, 0xE (text colours, button icons, panel styles).

Sprite writers (they fill 5 entries at a pointer, section 2):

| Function | Name | Notes |
|---|---|---|
| 0x2063C8 | `uiSpriteWrite(entry, x, y, u, v, w, h)` | the workhorse (31 callers), Z 0xFFFFFE |
| 0x206650 | `uiSpriteWriteZ` | same with an explicit Z |
| 0x206040 | `uiSpriteWriteWrap` | REPEAT/REGION_REPEAT variants (tiling backgrounds): WMS/WMT 2 or 3 chosen by whether the cell equals the screen size |
| 0x206B00 | `uiSpriteWriteFull` | picture sprites |
| 0x2068D0 | `uiSpriteWriteWH` | separate source and destination size |
| 0x2075F0 / 0x2076B8 / 0x2078A0 | `uiSetSpriteXY` / `uiSetSpriteUV` / `uiSetSpriteClamp` | the parts, for the scaled-text path |
| 0x2079A8, 0x207A28, 0x207A88 | `uiMoveSprite`, `uiSetQuadXY`, `uiFlatVertexWrite` | in-place edits of retained entries; the last builds a flat colour vertex pair `{RGBAQ, XYZ2}` |

Drawers on top (positions in the 640x448 space, `y` argument usually doubled by the caller):

| Function | Name | Notes |
|---|---|---|
| 0x2017B0 | `uiDrawGlyph(x, y2, shade, str)` | one character, returns the next x; handles Shift-JIS (kana/kanji 20x20 cells: `uiKanaGlyphUV` 0x200FE8, `uiKanjiGlyphUV` 0x2012F8), the accent codes, space = 8 or 10 |
| 0x203FC8 / 0x204100 / 0x204230 / 0x204310 | `uiDrawText` / `uiDrawTextRaw` / `uiDrawTextShade` / `uiDrawTextCell` | strings (the last on the 20x12 cell grid: x = col*20+10, y = (row*12+5)*2); the first, second and fourth take the palette bank (TEX2 CSA) |
| 0x201C80 | `uiDrawTextRight` | right-aligned |
| 0x203F40 | `uiTextWidth` | sums the width table `uiGlyphWidths` (0x29A880 + char - 0x20: 8..16 px; Japanese 20) |
| 0x201420 / 0x2045D0 | `uiDrawGlyphScaled` / `uiDrawTextScaled` | scaled text (the sprite is built in three parts with `uiSetSpriteXY/UV/Clamp`) |
| 0x2008C0 | `uiDrawSmallText` | the 10x20 small digit/letter set (atlas row v = 0xB4+20*row) |
| 0x201E48, 0x201FD0, 0x2046C0 | `uiDrawNumber16` family | 16x20 digits (atlas u = d*16+0x1B8, v = 200 / 220) |
| 0x202150, 0x204460 | `uiDrawNumber10` (with thousands commas), `uiDrawNumberAuto` | 10x20 digits |
| 0x203B78 | `uiDrawTime` | `h:mm:ss.cc` from frames (0x7FFFFFFF = "--:--") |
| 0x200A60 | `uiDrawPadGlyph(x, y, mask)` | the PS2 pad icons: masks 1/2/4/8 (L2, R2, L1, R1: 24x12 at (0x70/0x88, 0xEC/0xE0)) and 0x10/0x20/0x40/0x80 (triangle, circle, cross, square: 20x20 at u 0xB4/0xC8/0xDC/0xA0, v 0xE0), palette bank 1. `ButtonGlyphs.cpp` repaints these atlas cells |
| 0x200DC0, 0x203E70, 0x203CD0, 0x203098, 0x203D90 | `uiDrawIcon3`, `uiDrawIcon20`, `uiDrawArrow`, `uiDrawBar3`, `uiDrawBarWithArrow` | small icons, a 3-slice bar (8 + stretch + 8, bank 0xB), cursors |
| 0x202350, 0x204568 | `uiDrawPanel9(x, y, w, h, style)`, `uiDrawPanelCells(c0,r0,c1,r1)` | the window frame: 3x3 sprites (9-slice) of 12x12 corners/edges and a stretched centre; style picks the bank (7, 0xE, 8) |
| 0x2028B0, 0x202B70 | `uiDrawDialogueFrameA/B` | the big message-window frame (bank 7); only used by the dialogue task 0x23B168 |
| 0x203240 | `uiDrawHintBar(kind)` | the bottom hint line (5 languages) + a panel |
| 0x203990 | `uiSetTexa` | TEXA entry (fades text with TA0) |
| 0x203A50 | `uiDebugText8` | the 8x8 debug font (21 callers: `FOG`, `ZONE`, camera values: debug flags only) |
| 0x205350 | `uiMenuWindow` | the generic list window widget (struct: x, y, w, h, rows, cursor, items; draws `uiDrawPanel9`, rows with `uiDrawText`, scroll bar, cursor icon, animated open/close through TEXA); 10 callers (Pause pages, Options, mini-game menus) |

Callers of this library, by scene (the strings are from the decompiled code; the scene names are inferred from them):

| Scene | Functions |
|---|---|
| Title: Press Start / New Game-Continue / language / no controller | 0x208A88, 0x209520, 0x209848, 0x209CB0 (PATH3 picture + `uiDrawHintBar` + `uiMenuWindow`) |
| Loading screen "NOW LOADING..." | 0x20A150, 0x20D120 (2863 B, also the .GSL loader: "GSL DMA Tag Error!"), 0x20E140 |
| Town Start menu pages (task 0x217148) | 0x214AD0 (list), 0x214EF8, 0x215220, 0x2156D0, 0x2161E0 ("Choro Q Coins", " Mile"), 0x216738 (button setup = our Options replacement), 0x216E00, 0x216FF8: arena engine + `uiMenuWindow` |
| Memory card / save | 0x228708 (" Now Saving...", /BASLUS-20398), 0x22A218 ("Now formating..."), 0x227F90 ("mile", " Player 1") |
| Dialogue / message window | 0x23B168 ("next msg #%d"), 0x23A8E8, 0x23A140 ("Earned ", "by driving ", "miles."), 0x23A320/0x23A460 |
| Race/results tables | 0x235B58 ("ENTRY NAME", "RANK", " PTS", "TIME"), 0x2364B0 (" Bestlap"), 0x236858 ("RANKING", "TEAM NAME"), 0x236D28, 0x237080, 0x237388 ("World GPX stage"), 0x2359E8 ("retire"), 0x235F10 (" RECORD"), 0x236328 (" PRIZE"), 0x237680 ("Best Rank / Lap / Time"), 0x242FE8 ("+++ Q's Rally +++", "DESTINATION") |
| Shops / selling | 0x246250 (5463 B: "Selling", "Buying", "You have "), 0x248398 ("Sell Player 1 parts."), 0x261638 ("How much do you wish to bet?") |
| Mini-game menus and score screens | 0x2477A8 (" laps", "Entry"), 0x247AC8, 0x247BF0 (" Best Total Time"), 0x247E38 ("Course Score"), 0x248180, 0x248580, 0x248A70/0x248BF0 ("Quit Games", "End Game": the two-player pause), 0x252880 ("%d HOLE", "PAR %d"), 0x252CB0 ("Hole Par Score Best"), 0x25AB28/0x25B0C0 ("STEP%d", "Pylon hit"), 0x259138 |
| Mail / ending / misc | 0x264498 ("Subject", "mail box"), 0x269F48 ("BEST LAP", "WIN :"), 0x26A928 ("ending_scroll"), 0x266B78..0x267060 (result screens) |
| Debug overlays | 0x218EE0 ("FX %d %d", "TIRE %d RPM %d"), 0x21BA70, 0x2201C0, 0x250068, 0x250738, 0x26AD48..0x26E1F8 (`uiDebugText8`) |

### 3.2 The arena / layout engine (0x24F148..0x24F928)

A **retained, rebuilt-every-frame** packet: an arena object `{u32 limit, u32 used, u32 off, ...}` with
the data at +0x10; `FUN_0024F558(arena, size)` (`uiArenaInit`) sets `limit = size - 0x10`, `used =
0` (or `limit = 0` = hidden when `arena[2] != 0`). All emitters reserve from the arena and give up
(arena disabled for the frame) when it is full. Per frame a screen re-inits its arena, then:

```
uiArenaBeginBatch(arena, texTable, i)   0x24F5C8  reserve 0x60: gifBeginSpriteBatch with TEX0 = table[i*16+8]
uiArenaSetTexture(arena, table, i, csa) 0x24F640  reserve 0x20: gifSetTexRegs (TEX0, then TEX0 again = entry[i].lo + csa<<56: CLD 0 + CSA)
uiArenaSetColour(arena, rgbaq)          0x24F6D0  reserve 0x20: gifAdPair (RGBAQ)
uiArenaDataTag(arena, n)                0x24F728  reserve 0x10: gifSetSpriteCount (NLOOP = n*5, clamped to the room left)
uiArenaSprites(arena, recs, n, dx, dy, dz)       0x24F7C8  n sprites, 0x50 B each, from 32-byte records
uiArenaSpritesWrap(arena, recs, n, dx, dy, dz)   0x24F928  the REPEAT variant, 40-byte records
uiArenaFlatRects(arena, recs, n, dx, dy, dz)     0x24F148  flat coloured vertices (linestrip tag; unused path)
uiArenaRotQuads(arena, recs, n, ...)             0x24EB10 / 0x24EE30  rotated quads (VU0 matrix) as a 4-vertex strip (tag 0x29CBF0)
uiArenaSubmit(arena)                    0x24F578  vifDirect(arena + 0x10, used >> 4)
```

Sprite record (`uiArenaSprites`), 32 bytes, each field a 32-bit word of which the low 16 bits are
used: `x, y, u, v, w, h` (word offsets 0..5), `z` (word 6), `flags` (word 7: bit 0 mirror X,
bit 1 mirror Y). The sprite's depth is `0xFFFFFF - (z + base)`: **UI depth is real** (ZTE, GEQUAL,
ZMSK 0): layers are ordered by Z, and the main batch's fixed `0xFFFFFE` sits *behind* an arena
sprite of `z = 0`, in front of `z >= 2`. The wrap variant has 40-byte records (wrap size at word 6,
z at word 8, flags at word 9). Texture tables are arrays of 16-byte entries `{clamp/tex2 template,
TEX0}` (e.g. `hudTexTable` 0x29F5F8: three TEX0 values for the HUD atlases, CLD 1, T4/CT16).

Users: the **race HUD** (task `raceHudTask` 0x22DDD0: allocates 0x8CB0 bytes of the "extra buffer"
`0x15A7B80`, `raceHudInit` 0x2321B8 sets the element states at +0x8000.., `raceHudUpdate` 0x232C18 and
about 60 tiny element updaters 0x234128..0x235778 poke the numbers and gauges, `raceHudDraw` 0x233860
rebuilds the whole arena through ten element builders 0x22EE70, 0x22F338 (the item panel: 8 slots),
0x22F750, 0x22FE50, 0x230520, 0x230C20, 0x231208, 0x2318B0, 0x231C90, 0x231F28 and submits it), and
the Start-menu pages, scorecards and mini-game HUDs listed in 3.1. (The task is named from its
callees: it spawns the race music, the minimap and per-car updates; inferred.)

### 3.3 Retained packets built once (the shop packet area)

`getShopPacketArea` (0x22C3A8, "SHOP PACKET AREA SIZE OVER!", limit 0x1000 qw) returns the static
area `0x1785FF0`; `getExtraPacketArea` (0x20DF50, "EXTRA BUFF SIZE OVER!", 0x180000 bytes) returns
`0x15A7B80`. A screen builds a packet there once (header, data tag, sprites with the `uiSprite*`
writers, flat quads), edits entries each frame (`uiMoveSprite`, `uiHideSprite`, colour/alpha
words) and calls `vifDirect(area, n)`:

* stamp book (`stampBookBuild` 0x22C648, from task 0x22D1E0; inferred: 13 stamp sprites at 0x29F472,
  three pages at 0x29F508, hidden with XYZ3 unless the stamp flags `FUN_0023D488/0023D778` say collected),
* the shop room (`shopScene` 0x22AEB0, "GetShopPacketArea(%d)": a 640x224 backdrop sprite with TEST
  0x5380B, EE-transformed floor/wall quads, then the car models; the packet goes into the 3D chain),
* the Quick Race course carousel (`drawCarouselScene` 0x2455C0: 16 panels of 4 EE-built quads,
  headers with ALPHA `0x8000000044`, TEST `0x3001B`), `FUN_00244D60`, `FUN_00244F60` (lists clipped
  with SCISSOR packets: 2 qw before each 0xD qw group),
* the ending credits (`FUN_0026C588`, `vifDirect(0x1772670, 0x9D)`), "Need more money" shop
  (`FUN_00256640`, `vifDirect(0x175B570, 0x32C)`), `FUN_002564E8`, `FUN_002568D8`, `FUN_00256A78`.

### 3.4 PATH3 pictures

`FUN_00208A88`, `FUN_00209520`, `FUN_00209848`, `FUN_00209CB0` (title screens) build in `0x177A9D0` a
chain of: one END tag (`gifChainEndTag`, qwc 0x14), a batch header with TEX0
`0x20043DCE69329D8E` (TBP 0x1D8E, T8, 1024x512), a full-screen sprite (640x448 via
`uiSpriteWriteFull`), then a second header (`gifSetTexRegs`, TEX0 `0x20047B4E253221F2`, T8) and a
sprite, and send it with `sendTexturePackets` **every frame** (the picture upload itself, 640x448
T8 host->local, comes from the file loader: `FUN_0020DDE0` / `FUN_0020E030`). The survey saw it as
the only PATH3 sprite state (0.27% of 2D vertices).

## 4. Effects that read the frame buffer

* **Frame blend** (`frameBlend` 0x208120, one sprite a frame, FIX 0x20 = 25%, 0x40 on the frame after a
  scene change flag): draws the *previous* field's buffer over the new one. It is the game's own
  interlace-flicker reduction (survey: "textures read from the frame buffer" is 98.8% this packet).
  A native renderer that draws progressive frames would drop it (the repo's progressive-fields
  mode already forces `halfoff` to 0); `drive_flags & 0x8000` disables it.
* Fades (`uiFade` 0x2080F8, 51 callers; tasks 0x208270 fade out `(0x40-t)*0x2000000`, 0x2082D8 fade in,
  0x208358 white flash): colour = RGBA word, alpha in the top byte, ALPHA_1 0x44 so the strength is
  A/128 of the colour; TEST 0x3180A (ZTST ALWAYS).
* The loading screen blurs the menu picture (survey: `FIX=0x40`, 1,678 vertices, plus the
  `path1 tristrip CT32 self=1`, 926 vertices): not traced here.

## 5. The GS state combinations that occur in 2D (survey, 16,287,697 non-PATH1 vertices = 0.74% of all)

(All counts are vertices; sprites are 2 vertices; both frame buffers merged. The "emitter" column is
inferred from the headers above: the state signature fixes the header, not the scene.)

| Group | Vertices | Share of 2D | Emitter |
|---|---:|---:|---|
| Sprite, T4/CT16 CSM1, DECAL TCC 1, REGION_CLAMP, **alpha 0x44**, TEST 0x5001B | 8,217,622 | 50.5% | `gifBeginSpriteBatch` header unchanged: arena engine (race HUD, Start-menu pages, scorecards) |
| Same with **alpha 0x8000000044** | 5,341,276 | 32.8% | `UiBatch` (its first entry rewrites ALPHA_1) and scenes that set 0x8000000044 themselves (carousel, 0x235880, 0x255070, 0x26C588) |
| Triangle strip (4 vertices, FST), T4/CT16, 0x44 | 728,016 | 4.5% | `gifQuadTag`: `uiArenaRotQuads` (rotated sprites: gauge needles, dials) |
| Sprite, T8 (CT32 palette CSM1), 0x44, TEST 0x5001B / 0x5380B | 438,960 + 49,678 | 3.0% | stamp book (0x22C648), shop/title backdrops |
| Sprite with REPEAT/REGION_REPEAT/CLAMP mixes (WMS 3, WMT 0/2/3, ...), 0x8000000044 | 289,520 + 192,896 + 96,448 + 80,936 | 4.1% | `uiSpriteWriteWrap` / `uiArenaSpritesWrap`: tiling backgrounds, scrolling plates |
| Frame blend sprite (CT24 self-read, FIX 0x20; FIX 0x40 at scene changes), TEST 0x30802 | 211,688 + 1,678 | 1.3% | `frameBlend` |
| Per-frame clear sprite (TME 0, opaque, TEST 0x30000 / 0x32001) | 105,851 + 3,328 + ... | 0.7% | `sceGsSetDefClear` / the env: not a game draw |
| Triangle strip with STQ (FST 0), T8, TEST 0x5380B, ALPHA 0x44 (also wrap 3/3) | 198,712 + 99,356 | 1.8% | EE-built `ST,RGBAQ,XYZF2`: shop interior (0x22AEB0 via `eeQuadBuild`), carousel panels |
| MODULATE (TFX 0) T8 sprite 0x44; STQ strips TEST 0x3001B | 40,320 + 22,968 | 0.4% | `uiArenaSetColour` tints |
| Flat blended sprite, TEST 0x3180A, ALPHA 0x44 | 20,206 | 0.12% | `uiFade` / letterbox |
| PATH3 sprite (T8, 0x44) | 43,232 | 0.27% | title pictures |
| Context-2 flat opaque sprites (TEST 0x30000, ALPHA 0x8000000044/0x48/0) | 80,538 + 21,958 + 3,328 | 0.65% | 3D side: clear/shadow passes |

Register values that occur in 2D (these are all of them): TEST_1 0x5001B, 0x5380B, 0x3001B, 0x30802,
0x3180A, 0x30000, 0x32001; ALPHA_1 0x44 (FIX 0x80 or 0), 0x64 with FIX 0x20/0x40, 0x48 (context 2);
TEX1 0x60 (linear, no mips) throughout; TEXA (128, 1, 64) throughout (the other TA0 values of the
full survey are `uiSetTexa` fades: `TA0 = x, AEM, TA1 = x/2`); CLAMP: REGION_CLAMP 2/2 almost
always, REGION_REPEAT 3, CLAMP 1, REPEAT 0 in the wrap group; texture formats T4 (CT16 CSM1, 16
banks at CBP 0x1BC0), T8 (CT32/CT16 CSM1), CT24 (the frame buffer); TFX DECAL (98%) or MODULATE; no
FOG, no AA1, no dithering, COLCLAMP 1; FRAME CT32, FBMSK 0, Z24 ZMSK 0 with ZTST GEQUAL on UI
sprites (depth matters, section 3.2), ALWAYS on fades and blends.

Primitive types in 2D: SPRITE (14.7M vertices), TRIANGLE STRIP (0.73M textured FST + 0.30M STQ); no
lines, fans, points or triangles in 2D.

### Ranked by draw volume

1. Arena-engine sprite batches (race HUD, Start/Pause/Items pages, scorecards): ~8.2M vertices (50%); with the
   rotated-quad strips ~9.0M (55%).
2. `UiBatch` immediate text/number/window sprites and scenes using the 0x8000000044 header: ~5.3M (33%);
   with the tiling/wrap sprites ~6.0M (37%).
3. T8 pictures/stamp book/shop backdrops: ~0.5M (3%).
4. EE-transformed STQ strips (shop interior, carousel): ~0.3M (1.8%).
5. Frame blend: ~0.21M (1.3%) (droppable).
6. Fades and letterbox flat quads: ~0.02M by volume (0.12%) but 51 call sites.
7. PATH3 title pictures: ~0.04M (0.27%).

Not measured (probe needed): per-scene draw-batch counts (how many `vifDirect` per frame in a race,
a town, a menu) and sprites per frame; the survey cannot be split by scene. A run with
`RT_GS_STATE_SURVEY=<file>` per saved state (race, town street, Pause pages, Quick Race menu, shop,
title) gives it. Static caps: `UiBatch` 511 sprites a frame; the race HUD arena is initialised with
0x8000 bytes, so 0x7FF0 / 0x50 = 409 sprite slots at most (less the 0x70 bytes of every batch header).

## 6. Proposed native draw kinds

State (all observed; 2D fits in this record):

```
struct Tex2D   { u64 tex0;        // TBP0/TBW/PSM/TW/TH/TCC/TFX/CBP/CPSM/CSM/CSA/CLD, as the game writes it
                 u8 csa;          // palette bank after a TEX2_1 / the second TEX0 (CLD 0)
                 u8 wms, wmt; u16 minu, maxu, minv, maxv;   // CLAMP_n (REGION_CLAMP rect = the atlas cell)
                 bool linear; }    // TEX1 0x60 -> linear mag/min, no mips (always true in 2D)
struct Blend2D { u8 a, b, c, d; u8 fix; }                   // 0x44, 0x48, 0x52, 0x64 + FIX seen
struct Test2D  { bool ate; u8 atst, aref, afail; bool date, datm; bool zte; u8 ztst; }
struct Fb2D    { u32 fbp, fbw, psm, fbmsk; u32 zbp; bool zmsk; }  // env: constant per frame buffer
```

Draw kinds (each is a list the renderer batches by `{Tex2D, Blend2D, Test2D}`):

1. **Sprite** `{x0, y0, x1, y1 (1/16 px, primitive space), z, u0, v0, u1, v1 (1/16 texel), rgba}`
   with a `Tex2D`. Covers about 98% of 2D: UI text, numbers, panels, HUD, pictures, tiling (wrap
   clamp modes), flat fills (`tme = 0`, colour only) and the frame blend (texture = a frame buffer).
2. **Quad strip** (4 vertices, textured, FST): rotated sprites. Same state as 1, four screen points.
3. **STQ strip** (EE-transformed, Gouraud, `ST,RGBAQ,XYZF2`): shop interior, carousel; a textured
   triangle strip with perspective-correct STQ, optional fog (`FGE`). Same kind as the 3D batches.
4. **Self copy / blit** (frame blend, the loading blur): a sprite whose texture is a frame
   buffer; natively: skip, or a present-time blend.
5. **Z clear** (the `0x32001` sprite): a native renderer owns depth and clears it itself.

Registers a decoder of PATH2 packets must understand (the complete list, from the templates and
emitters above): `PRIM` (via the GIF tag's PRE field: 0x156 sprite, 0x154 strip, 0x46 flat sprite, 0x246
context-2 flat sprite, plus the STQ strip PRIMs of the tag table), `RGBAQ`, `UV`, `ST`, `XYZ2`, `XYZ3`
(hidden sprites), `XYZF2`, `TEX0_1/2`, `TEX1_1/2`, `TEX2_1`, `CLAMP_1/2`, `ALPHA_1/2`, `TEST_1/2`,
`TEXA`, `SCISSOR_2` (map windows, list clipping), `FOGCOL` (3D side); and for PATH3 the transfer
registers `BITBLTBUF/TRXPOS/TRXREG/TRXDIR/HWREG` (the uploads).

### Hook points

| Level | Where | What it gives | Cost |
|---|---|---|---|
| A (safest) | `vifDirect` 0x226F70 (`param_1` = EE address, `param_2` = qwc) | every PATH2 packet, decoded as a GIF packet: tag (PRE/PRIM, NLOOP, A+D / REGLIST / PACKED) then data. Decode at the call (the arenas and `UiBatch` are final there; the double-buffered `UiBatch` is not rewritten until the next frame) | a GIF-packet interpreter limited to the registers above; no game-code change |
| A2 | `sendTexturePackets` 0x207B50 (chain address) | PATH3: upload chains (BITBLTBUF/TRXREG + IMAGE) and the picture sprite | the texture side belongs to the texture survey |
| A3 | `vifBeginPacket(1)` / `vifEndPacket(1)` / `vifKick` (0x227108/0x227178/0x2271F0) | the order of 3D chains against the main chain (3D first, then 2D, section 1) | frame structure |
| B (semantic) | `uiArenaSubmit` 0x24F578, `uiBatchFlush` 0x203810 | the arena / main batch as a list of 5-qword sprites with their layer Z, without a stream decoder | the arena record layout (3.2) lets HUD elements be re-laid out (widescreen anchoring: `WideLayout` works on GS packets today; here each arena is one element set) |
| B2 | `uiSpriteWrite*` 0x2063C8.. | the individual sprite | too fine: entries are patched afterwards (`uiMoveSprite`, XYZ3), so intercept the submit |
| C (state) | libgraph `sceGsPutDrawEnv` 0x26FE80 / `sceGsPutDispEnv` 0x26FA08 / `sceGsSwapDBuffDc` 0x26F738 | the per-field frame buffer, DISPFB and the half offset | `RT_PROGRESSIVE_FIELDS` already forces `halfoff` 0 |

Ordering rule to keep: **3D chain(s) first, then PATH2 packets in call order, `UiBatch` last, frame
blend after it**; inside 2D the Z of each sprite decides overlaps (`0xFFFFFF - z` for arenas,
`0xFFFFFE` for `UiBatch`), not the call order.

## 7. Open questions / what a probe would settle

* Per-scene sprite and batch counts (section 5): one `RT_GS_STATE_SURVEY` run per checkpoint.
* The exact Z the fade quad carries (0xFFFF00 in the packet: it passes ZTST ALWAYS; whether it
  writes Z is ZMSK 0 from the env: guess that it does).
* Which tasks set the `0x400000` scene-change flag (`*(param_2 + 0x28) |= 0x400000`) that picks
  the 0x40 frame blend: inferred from `FUN_00208120`'s argument `drive_flags >> 22 & 1`.
* The 3D-side PATH2 packets (fog 0x172FF50, car shadow) are only listed; their native handling
  belongs to the VU1/3D survey.
* `uiArenaFlatRects`' linestrip tag (0x29CC60) is never drawn in the survey: confirm unused.
* The local-to-local GS transfers in the survey (T4->T4 32x16 and 64x64, ~63k) are not 2D; they are
  per-frame texture copies by the car/town code (guess).
