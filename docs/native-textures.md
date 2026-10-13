# Textures, GS memory and render targets (Phase 5 survey)

Survey for Phase 5 of the native renderer: no VU1/GS emulation, textures as GPU images created
from the game's own uploads. Branch `re/native-tex-survey`, read-only on the runtime. Addresses are
the USA release (SLUS-20398); names are in `config/game_symbols.toml` (they show up in
`scripts/ghidra/fn.py`). Companion to `docs/native-render.md` (draw lists, models, VU1 helpers).

Evidence tags: *verified* = read in the decompiled code, the disc data or the state survey;
*likely* / *guess* = inferred. Nothing was run on the GPU. Numbers about the disc come from
`scripts/gs_upload_survey.py <disc dir> [--models <rt_model_dump outputs>]` (research tool, prints
numbers only). Numbers about draws come from `RT_GS_STATE_SURVEY` (`build/scratch/hwgs/states.md`
and `survey-*.tsv`: 13 game processes, 2.19 G vertex kicks, 629 distinct draw states, 59,095
transfers; town, field areas, Q's Factory, quick races, menus).

## 1. Summary

- **Every host-to-local upload is a file's prebuilt GIF chain sent through one funnel,
  `sendTexturePackets` (0x207B50: DMA channel 2, chain mode, CHCR 0x105)**, plus one boot-time
  clear (`sceGsExecLoadImage` 0x26EB30, 21 uploads of 640x32 CT32, from `gameMainLoop` 0x2003B8).
  No other code builds a host-to-local transfer: the only A+D `BITBLTBUF/TRXPOS/TRXREG/TRXDIR`
  templates in the ELF data are the three local-to-local ones below. *verified*
- The funnel has 44 callers. Besides uploads it also sends 2D draw packets (PATH3 sprites;
  0x208A88, 0x209520, 0x209848 and 0x209CB0 send packet 0x177A9D0) and the local-to-local copies.
  A hook there sees every upload with its final addresses (the loaders rebase TBPs before sending).
- **Local-to-local (TRXDIR=2) copies exist in three places**: the car slot copy (`copyCarModel`
  0x20AD18, template 0x29AD90), the texture animation copies (26 call sites 0x23E140..0x241AC8,
  template 0x2A3490, table 0x2A2BF0 with 69 records) and `texAnimCopyB` (0x262678, template
  0x2D9F50). All 26 + 1 call sites checked in the decompiled code. *verified*
- **No readback in play**: `sceGsExecStoreImage` (0x26ECB0) has one caller, `debugScreenRead`
  0x20DCD8, reached only from the debug "Screen Save" 0x20E500, which nothing calls. *verified*
- **Render-to-texture is one thing**: the post pass (`postPassSprite` 0x208120), a full-frame
  sprite at the end of each frame's chain that samples **the buffer it draws into** (survey: TBP =
  FBP * 32 in 99.2% of its 208,540 draws; batch logs: fbp 0 / tbp 0, fbp 70 / tbp 2240) as a CT24
  1024x256 texture, region-clamped to 640x224, scaled by about 1% and blended at 25% (FIX 0x20), or
  at 50% shrunk into (2,1)-(637,222) (FIX 0x40: the loading blur). *verified (packets, survey, logs)*
- Textures are named by **absolute TBP only**: no texture ids, no size tables on disc. A draw's
  TEX0 (TBP, TBW, PSM, TW, TH, CBP, CPSM, CSA, CLD) is (a) in the model chains' material blocks
  (static, always the TBP of an upload of the same file), (b) built in code from a slot number
  (cars, props, parts), (c) a hard-coded immediate (2D/UI, section 5). *verified*
- Formats on disc and in draws: PSMCT32, CT24, CT16, T8, T4. **No T8H/T4HL/T4HH, no CSM2 (CSM1
  only), no TEX2 in the survey scenes.** *verified (survey scenes + disc)*

## 2. GS memory map (blocks of 256 bytes; 16,384 blocks = 4 MB)

| Blocks (hex / dec) | Content | Evidence |
|---|---|---|
| 0x0000..0x08BF / 0..2239 | frame buffer 0: 640x224 CT32, FBW 10, FBP 0 (70 pages of 32 blocks) | survey `fbp=0 fbw=10`, `gsInitDisplay` 0x2000C8 |
| 0x08C0..0x117F / 2240..4479 | frame buffer 1 (FBP 70) | survey `fbp=70`; `postPassSprite` uses `n * 0x8C0` |
| 0x1180..0x1A3F / 4480..6719 | Z24, ZBP 140, shared by both | survey `zbp=140 psm31` |
| 0x1A40.. / 6720.. | textures and palettes (9,664 blocks = 2.36 MB) | boot clear covers 0..0x1A3F exactly (21 x 0x140 blocks) |
| 0x1A40..0x1BBF | HUD font: T4 512x384, TBW 8 (`FONT.GSL`/`FONTE.GSL`, `loadFontTextures` 0x20DDE0, rebase 0x1A40) | upload list |
| 0x1BC0 / 7104 | font palette CT16 16x16 | upload list, TEX0 literal in 0x2038F8 |
| 0x1BC2 / 7106 | CARS/PARTS: T4 128x128 + CT16 palette, 4 x T8 64x32 + CT16 palettes (`loadCarPartsSet` 0x20AEF0) | rebase 0x1BC2 |
| 0x1C44 / 7236 | CARS/TIRE: T4 128x128 + CT16 palette at +32 | rebase 0x1C44 |
| 0x1CC6, 0x1D8E, 0x1F2E | scene sets: shop interiors (0x1CC6, 0x1F2E, 0x285E, 0x34BE by sub-step), POSE / LOGO / TITLE (0x1D8E), OPTION2 (absolute at 0x1F2E) | rebase constants, section 3.2 |
| 0x237A / 9082 | PUTI frames (T8 256x184 + CT32 palette, one at a time) and METER gauges | rebase 0x237A |
| 0x243E + n * 0x44 | **car slot n = 0..14**: T8 128x128 (64 blocks, TBW 2) + CT32 16x16 palette at +0x40 (4 blocks) | `loadCarModelAt` 0x20C9D0, `loadCarModel` 0x20C7B8; 15 slots = (0x707B80-0x3FBB80)/0x34000 |
| 10334, 11390, 12446, 13502 | **FLD tile slots** z = 0..3 (the tile name's last base-4 number = `slot & 3`): stride 0x420; a tile's textures can run past its stride | 64 FLD files, absolute TBPs, no rebase |
| 0x2B26 + i * 0x44 | COURSE/ACTION misc-model chains, i = 0..5 (car-like T8 128x128 + palette) | `loadCourseTile` 0x20C188 |
| 12446..16123 | COURSE/ACTION textures (absolute; slot 2's window and above) | 40 files |
| 16169..16289 | EFFECT.GSL (absolute: T4 32x32 / 64x32 effect sprites + 16x2 palettes) | upload list |

Block-level packing (the exact GS block layout of each format, `blocks()` in the survey script):
**no two FLD tiles of different slots share a GS block** (1,536 file pairs, 0 conflicts), so the
four loaded tiles never overwrite each other; two tiles of one slot do (streaming), partly,
because their extents differ. Inside one file, 26,270 upload pairs share blocks; they are by design
(relative chains rebased several times, streamed frames to one TBP, palette blocks, section 6).

## 3. How textures get in

### 3.1 The funnel and the chain format

- `sendTexturePackets(p)` (0x207B50): `sceDmaGetChan(2)`, `FlushCache(0)`, `sceDmaSync`, then
  QWC = 0, TADR = p, CHCR = 0x105 (chain mode, start). Every caller first waits for channel 1
  (`sceDmaGetChan(1)`, `sceDmaSync`): the VIF1 draw chain must be idle.
- A file's texture section is a chain of CNT tags (id 0x10 in byte 3) ending in an END tag (0x70).
  Each CNT holds one upload: an A+D GIF tag with BITBLTBUF, TRXPOS, TRXREG, TRXDIR (= 0), then an
  IMAGE tag and the pixels (linear rows in the destination PSM: **the payload is the texture's rows,
  in order, before the GS swizzles it**). One CNT carries at most 0x7FFF qwords (512 KB); the
  largest upload is a 640x448 T8 picture (286,720 B).
- Loaders **rebase** a file by walking the chain and adding a constant to the DBP of the first A+D
  of every CNT (`*(u64 *)(tag + 0x20)`, bits 32..45): font + 0x1A40, cars + 0x243E + n * 0x44 and
  so on. FLD/COURSE/ACTION tile chains carry absolute TBPs and are sent as they are. Palettes are
  in the same file (texture, then its palette) and are rebased with it.
- Every upload on the disc is a whole image: 8,944 uploads in 335 files, 101.5 MB (T8 84.3 MB in
  1,424 uploads, T4 14.8 MB in 3,118, CT32 1.7 MB in 4,289 palettes (2,981 of 16x2, 1,308 of
  16x16), CT24 0.5 MB in 39, CT16 38 KB in 74 palettes). **All with DSAX = DSAY = 0** (no partial
  uploads). *verified* (`scripts/gs_upload_survey.py`)

| Group | Files | Uploads | Per file (max) | Typical content |
|---|---:|---:|---|---|
| FLD (tiles) | 64 | 3,129 | 292 uploads, 489 KB | T4 64x64, 128x128, T8 64x64, CT32 16x2 / 16x16 palettes |
| COURSE | 19 | 1,683 | 192 uploads, 773 KB | as FLD, plus T4 256x256, T8 512x96 |
| ACTION | 21 | 1,506 | 316 uploads, 648 KB | as FLD, CT24 128x128 |
| SHOP | 22 | 964 | 157 uploads, 8.2 MB (T02) | T8 640x384 interior pictures (249), T4 32x32 |
| SYS | 40 | 1,172 | 204 uploads, 4.8 MB (PUTI) | PUTI 100 x T8 256x184, TITLE 640x448, FONT, EFFECT, STAMP 64x64 |
| ITEM | 12 | 174 | 122 uploads, 263 KB | meters (T8 256x128, T4 128x120), parts |
| CAR0..CAR4 | 150 | 300 | 2 (T8 128x128 + CT32 16x16, 17,408 B) | one per car |
| CARS | 3 | 16 | 12 | PARTS, TIRE, Q150 (WHEEL has none) |

### 3.2 Who loads what, and when

Load requests are tasks: `loadRequestTask` (0x20DC50) records a request word at 0x333374 (bits
16..19 = kind, 0..7 = index, 0x100000 = no blur) and sets 0x333370 (busy); it sets drive flag
0x400000 (the loading blur, section 4) unless the request has 0x100000. `loadStep` (0x20D120) is the
per-frame state machine (state at 0x3356C0, 0x8000 = a CD read pending; reads are `sceCdRead` into
0x100000 through `cdReadEntry` 0x20E1F0 / `cdReadEntryPart` 0x20E250; the DMA of the chain is one
state). Uploads therefore happen **at scene or tile load time**, one DMA per file section, a few
frames apart; there is no per-frame texture upload traffic except the copies of 3.3.

| Kind (request >> 16 & 0xF) | Loader | Files (table) | TBP rebase |
|---|---|---|---|
| 0, index 0 | `loadCarPartsSet` 0x20AEF0 | SYS entries 1..4: EFFECT (absolute), PARTS, TIRE, WHEEL (models) | PARTS 0x1BC2, TIRE 0x1C44 |
| 0, index 1 | 0x20B348 | SYS entry 9: POSE.GSL | 0x1D8E |
| 0, index 3 | `loadStep` | METER1/10 (0x29B410) | 0x237A |
| 0, index 4/5 | `loadShopSet` 0x20B5E8 | SHOP/Tnn (0x29B29C), SYS entry 11 | 0x1F2E, 0x1CC6, 0x285E, 0x34BE (sub-steps) |
| 0, index 6 | `loadStep` | S02.GSL (0x29B054) | 0x285E |
| 1 | `loadStep` | TITLE.GSL (SYS entry 6) | 0x1D8E |
| 2 | `loadStep` | TAKARA.E3D (models to 0x707B80), LOGO.GSL | 0x1D8E |
| 4 | `loadStep` | SYS entry n (0x29AF58 + n * 12) | none seen (absolute) |
| 5 | `loadCourseTile` 0x20C188 | COURSE/ACTION (0x29BE50) | none for the tile; misc chains 0x2B26 + i * 0x44 |
| 6 | `loadStep` | PUTI.BIN frame n (0x29B084; 0xC000 bytes each) | 0x237A |
| 8 | `loadStep` | ITEM/PARTS.GSL (0x29B150) | 0x2C7E |
| 9 | `loadFieldTile` 0x20BAC8 | FLD tile (0x29BB50), slot = index & 3 | none (absolute) |
| 0xA | `loadStep` | METER0/8 (0x29B398) | 0x237A |
| 0xC | `loadCarModelAt` 0x20C9D0 | CAR0..4 / Qnn (0x29B428) | 0x243E + n * 0x44 |
| 0xD | `loadStep` | OPTION2 (0x29B168) | none (absolute 7982) |

Files are matched to table entries by sector count and, where known, by TBP (ambiguous matches such
as the SCOREA files are not resolved); kinds 3, 7 and 0xB were not read. Boot: `gameMainLoop`
clears blocks 0..0x1A3F with zeros (21 uploads of 640x32 CT32 through `sceGsExecLoadImage`, no
chain) and `loadFontTextures` sends the font. `loadFieldTile` streams tiles while driving: state 0
reads the 0x800-byte header, 1 the rest (up to 0x3A8000 bytes), 2 sends the texture chain
(`sendTexturePackets` on the whole section at once: FLD/223 = 284 uploads, 489 KB), 3 sends the
misc models' chain (no rebase for FLD; `loadCourseTile` rebases it to 0x2B26 + i * 0x44).

### 3.3 Per-frame GS memory writes: the copies

All run from per-frame tasks while a scene lives (survey: 4,687..13,087 copies per run and tuple,
i.e. once per game frame). *verified (templates, call sites); once per frame is likely*

- **Car slot copy** (`copyCarModel` 0x20AD18, when a model is duplicated): two packets. Texture:
  BITBLTBUF SBP = a * 0x44 + 0x243E, DBP = b * 0x44 + 0x243E, SBW = DBW = 2, PSM T8 both, TRXREG
  128x128; palette: SBP/DBP + 0x247E, SBW = DBW = 1, CT32 16x16.
- **Texture animation copies** (26 sites; template 0x2A3490 holds up to 8 x {BITBLTBUF, TRXPOS,
  TRXREG, TRXDIR = 2}; NLOOP 4 / 0xC / 0x10 / 0x14 / 0x20 = 1, 3, 4, 5, 8 copies): each record of
  `texAnimTable` 0x2A2BF0 (69 x 0x20 bytes: sbp, dbp, tbw, psm, w, h, scroll multipliers and
  masks) copies a w x h window of a taller T4 (or T8) strip (SSAX/SSAY from the frame counter, a
  4-phase table, masks that keep the window inside the strip: *likely*) into the visible texture at
  DSAX = DSAY = 0. Examples: COURSE/C02: T4 64x256 at 12773 into T4 64x64 at 12755 and 12764;
  FLD/203: T4 128x256 at 13549 into T4 128x128 at 13502; the mip levels are copied separately
  (64x16 chunks). Every record's source and destination are upload starts of one file (69 of 69).
  Copies move **indices**: the destination keeps its own palette.
- `texAnimCopyB` (0x262678): the same with template 0x2D9F50 (3 copies), driven by the scene's task.

## 4. Render targets and the display

- `gsInitDisplay` (0x2000C8): `sceGsResetGraph(0, 1, 2, 1)` (interlaced, NTSC, FFMD field mode) and
  two `sceGsSetDefDBuffDc` (CT32, 640x224, Z test GREATER, Z24, clear on) at 0x335C80 and 0x335FB0
  (same settings; the second is used for the frame in which drive flag 0x400000 is set, the loading
  blur). `videoMode` 0x333288 = 0 (a 512x256 alternative exists, unused). Buffers: FBP 0 and 70,
  Z at page 140 (survey). Every vblank shows a new 224-line field.
- `gameMainLoop`, per frame (k = `drawBufferIndex` 0x335908): build the VIF1 chain, run the tasks
  (`gs_tasks_walker` 0x204F98), `draw2dListBegin`, the post pass (`postPassSprite(k ^ 1, bit 22)`,
  skipped when drive flag 0x8000 is set), kick, `sceGsSyncV(0)` (returns the field, saved at
  0x335909), half-pixel offset by field on both contexts of the draw environments of the next frame
  (0x26F340 / 0x26F3C8), then `sceGsSwapDBuffDc(db, k)` and `k ^= 1`. *verified (code)*. Which
  buffer is on display while the next one is drawn is **not settled by the code alone**: the
  call passes `k ^ 1` as the post pass's TBP block, and the measured draws sample the buffer they
  draw into (next bullet), so the draw environment `sceGsSwapDBuffDc` installs targets `k ^ 1` in the
  following iteration. Open question 1.
- **Post pass** (`postPassSprite` 0x208120): a DIRECT packet from 0x29A930 + variant * 0x90 with
  TEX0 = TBP n * 0x8C0 (n = the argument), TBW 10, CT24, TW 10, TH 8 (1024x256), TCC 1, CLD 0; CLAMP
  region (MINU/MAXU 0..639, MINV/MAXV 0..223); ALPHA (Cs - Cd) * FIX + Cd; TEST with Z always; one
  sprite, UV (0,0)-(640,224): variant 0 (FIX 0x20) to screen (0,0)-(639,223), variant 1 (FIX 0x40,
  the loading blur, drive flag 0x400000) to (2,1)-(637,222). **Measured: the texture is the draw's
  own frame buffer** (survey `self=1` in 206,896 of the FIX 32 draws and 1,644 of the FIX 64
  ones, 26 FIX 64 draws sample the other buffer; `RT_GS_BATCH_LOG` of the boot frames: vblank 10
  `fbp=0 tbp=0 psm=1 box=2,1,637,222`, vblank 11 `fbp=70 tbp=2240 psm=1`, alternating).
  Effect: the finished frame is blended with a slightly smaller copy of itself (a soft filter; a
  zoom smear while loading). A GS reads and writes the same pixels in raster order, so the result
  of the shifted variant differs from a blend against a snapshot of the frame; the hardware GS
  uses a snapshot. *verified (packets, survey, logs); the purpose is a guess*
- **A 1x1 CT32 TEX0 inside the frame buffer area, TME on, TCC 0** appears in 926 PATH1 strip draws
  (a few frames per run, e.g. frame 1366): TEX0 left at its reset value (TBP 0) while TME is on.
  Source unidentified (open question 3).
- Nothing samples the Z buffer. Nothing reads a render target back (no `ExecStoreImage` in play).
  The title and loading pictures (640x448 T8, TITLE.GSL, SCOREA06) are ordinary uploads.
- The hardware GS keeps one image per FBP (a snapshot when a draw samples its own target) and calls
  `unsupported()` only for "local-to-local copy out of a render target" and "local-to-host
  readback": neither happens in the survey scenes.

## 5. How draws name textures

Sources of TEX0 (survey: 2,840 distinct materials in the chains of FLD/COURSE/ACTION/SHOP/CAR0/CARS/SYS
.BIN, `rt_model_dump` + `gs_upload_survey.py --models`):

1. **Model chain material blocks** (A+D TEX1_2, TEX0_2, CLAMP_2, optional MIPTBP1_2; native-render.md).
   *All 2,840 TEX0 TBPs are the start of an upload in the same file*; PSM, TW, TH equal the
   upload in 2,837 (the other 3: FLD/103 T4 64x128 over 64x78 uploaded rows, ACTION/A04 T4 64x32 over 64x20,
   ACTION/A01 T4 128x64 over 128x62). CLD = 1 in 2,803, 0 in 37; CPSM CT32, CSM1, CSA 0 in
   all of them. 185 have MIPTBP1_2 (FLD 148, ACTION 22, COURSE 15; MXL 1..3; levels 1 and 2 are
   separate uploads at TBP1/TBP2 with TBW 2). Wrap: CLAMP 31%, REPEAT 37%, mixed 28%, REGION_CLAMP 2.9%
   (draws; sprite cut-outs of atlases such as the font). Their CLUT blocks are uploaded in all but
   one case (FLD/220, section 6 case 6).
2. **Built in code from a slot** (`vifBatchRegs` 0x226C28): cars `TBP = 0x243E + slot * 0x44`,
   TBW 2, T8 128x128, TCC 1, MODULATE, `CBP = TBP + 0x40`, CPSM CT32, CLD 1 (`drawCar` 0x221788,
   0x222360, 0x223238, 0x223588, 0x2228A8, 0x222B70); wheels/parts: a first TEX0 with PSM T8
   and CLD 1 (CPSM CT16) that **loads the 256-entry palette**, then a second TEX0 with PSM T4,
   CLD 0 and CSA = part index (0..9) that picks a 16-colour bank: `drawCar` 0x221788 loop, TBP 0x1BC2 /
   0x1C44 (PARTS, TIRE). Props (0x25BE98..0x25D5F8) use `vifFixedRegs` with T8 128x128 at
   fixed TBPs 11320 .. 15544 (+64 = CBP).
3. **Hard-coded immediates** (2D/UI, passed to `gifSpritePacketInit` 0x21D718; about 40 distinct):
   font T4 CT16 512x512 (TBP 0x1A40, CBP 0x1BC0: *over 384 uploaded rows*), LOGO (7566 T8 512x512
   TBW 6), TITLE/PUTI/shop pictures (7982, 9082 T8 TW 10 or 8: **TW/TH exceed the uploaded
   640x384 / 256x184**), OPTION2 (7982 T8 256x256), coins (7496 T4 64x64), EFFECT (16169 T4 32x32,
   15866 T4 1024x128), map/mini-game sheets (11046.. T4 256x128, 15544 T8 128x128).
4. The post pass (section 4).

Draw-level facts (survey, share of draws): T4 + CT32 palette 57.7%, T8 + CT32 34.7%, T4 + CT16 5.1%
(font, car parts), CT24 2.5% (post pass, CT24 uploads), CT32 926 draws; 128x128 41%, 64x64 39%,
256x128 6%; filters bilinear (MMAG LINEAR) everywhere, mip only 1.5% of draws (`MXL` 1 with
LINEAR_MIP_NEAREST); TEXA (128, 1, 64) in 99.99%, other values (ta0 = 0..120 in steps of 8, ta1 =
ta0 / 2) from `draw2dPushTexa` 0x203990 (CT16 fades); no CSM2.

**Texture ids**: none. A file's identity is its upload list; the only stable key across runs is the
content (FNV-1a of the decoded pixels, as the texture dumps do) or `(file, upload index)`.

## 6. Cases that need real GS-memory semantics

Everything else is "texture = the upload at this TBP". These are not, in order of importance
(counts from the disc scan and the survey):

1. **Block-granular overwrite (slot reuse).** Tiles of one FLD slot, COURSE/ACTION over slot 2's
   window, scene sets sharing 0x1D8E / 0x237A / 0x1F2E / 0x285E, PUTI frames and shop pictures
   streamed to one TBP (100 uploads of T8 256x184 to TBP 0 in PUTI.BIN). A new upload kills the
   surfaces it touches at block level; the old ones are partly overwritten, not replaced.
2. **Palette blocks shared with the next texture.** A T4 palette is uploaded as 16x2 CT32 (2,981
   uploads) = two blocks: CBP and CBP+1 are two 16-colour palettes, and the following texture
   often starts at CBP+1, overwriting the second one (889 CT32/T4 pairs in FLD alone). A palette is
   whatever the blocks hold at the TEX0 write.
3. **CLUT buffer state across TEX0 writes.** A TEX0 with PSM T8 and CLD 1 loads 256 entries; the
   T4 TEX0 that follows (CLD 0, CSA = part) only selects a bank (wheels, tyres, car parts:
   `drawCar` 0x221788, 10 parts). Needs the GS CLUT buffer as state (CLD 0..5, CSA, CT16 = 512
   entries; CSM1 swizzle: entries 8..15 swap with 16..23).
4. **One palette, several formats.** CT32 16x16 (T8, 4 blocks), CT16 16x16, 16x2 and CT32 read as a
   CLUT: uploaded as pixels, read through the CLUT path.
5. **TEX0 larger than the upload.** TW/TH 1024x512 over 640x384 / 640x448 pictures, font 512x512
   over 512x384, and 3 materials of 2,840 (FLD/103 64x128 over 64x78 rows, ACTION/A04 64x32 over
   64x20, ACTION/A01 128x64 over 128x62): the missing rows are whatever the blocks hold.
6. **A palette overwritten by a later texture**: FLD/220.BIN, T8 64x256 at 11210 with its CT32
   16x16 palette at 11322, then T8 64x32 at 11320 covers blocks 11322..11325. Needs the memory
   (or a decision that it is garbage by design). 1 of 2,840 materials.
7. **Local-to-local copies**: index copies between surfaces of different TBW, windows taken from
   taller strips (T4 64x256 to 64x64, 128x256 to 128x128), partial destinations (32x16 into
   32x32, 64x16 chunks into mip levels), car slot copy of texture and palette; once per frame.
8. **Self-sampling render target** (post pass), CT24 view of a CT32 target, region clamp, TW 1024
   over a 640-wide target; plus 926 draws with a 1x1 TEX0 at TBP 0 (the frame buffer) and TME on.
9. **Mip chains from separate uploads**: MIPTBP1_2 names the TBPs of levels 1 and 2 (185 materials);
   each level is its own upload (with its own palette block) and is animated by its own copy.
10. **Save states** need GS local memory exactly (the shadow memory the CPU GS keeps).

Not needed (checked): no partial uploads (DSAX/DSAY always 0), no T8H/T4HL/T4HH/Z textures, no
CSM2/TEXCLUT, no readback, no texture whose CLUT blocks were never uploaded (except case 6), no
two FLD tiles of different slots on one block.

## 7. Proposed design

1. **Hook at the funnel.** Wrap `sendTexturePackets` (0x207B50) and `sceGsExecLoadImage`: copy the
   chain from EE RAM, parse CNT/A+D/IMAGE, and for each upload keep (psm, w, h, dbp, dbw, payload).
   Also feed the existing CPU GS transfer path so the shadow memory (save states, fallback) stays
   exact. The payload is already linear rows: **no unswizzle**.
2. **Surfaces and ownership.** `Surface {tbp, tbw, psm, w, h, blocks, image, gen}`; a 16,384-entry
   block owner table (exact block sets from the GS layout, as `blocks()` in
   `scripts/gs_upload_survey.py`). An upload clears the owners of its blocks and drops surfaces that
   lose all blocks; a surface that loses some is marked partial (fallback below). Lookup at a
   TEX0: `owner[tbp]` is a surface starting exactly there with the same PSM and extent >= the
   used region (true for 2,837 of 2,840 materials).
3. **Content keys.** Index images (T8 / T4 expanded to one byte per texel, R8) are keyed by FNV-1a
   of (psm, w, h, payload): re-loading a tile, the 150 cars' identical 17 KB blocks and returning to
   a town hit the cache; the same bytes at another TBP share one image. Direct formats (CT32, CT24,
   CT16) keep raw texels; TEXA is applied at sample time (AEM, TA0, TA1; 99.99% 128/1/64).
   Resident set is bounded by GS memory (<= 2.36 MB of texture blocks, < 8 MB as indices).
4. **Palettes.** `Clut` objects keyed by hash of the palette bytes + format (CSM1 swizzle undone at
   creation, 256 entries CT32 or 256/512 CT16). Replace the "palette reloaded every frame" churn
   (CLD 1 in 2,803 of 2,840 materials, the same bytes each time): a TEX0 write with CLD resolves
   `owner[cbp]` to the palette upload (or the shadow memory in cases 2 and 6) and sets the current
   CLUT; CLD 0 keeps it (case 3). An RGBA variant per (surface, palette) is made once on the GPU
   (hardware sampler for filtering, wrap, mips, anisotropy, texture packs) with the existing
   stable-key / content-hash naming of dumps (hash of decoded pixels) so packs keep working.
5. **Copies** (cases 7): when source and destination rectangles each lie inside one surface of the
   same PSM, copy indices on the GPU (`vkCmdCopyImage` of R8 images), bump the destination's
   generation (its RGBA variants rebuild lazily: at most a few 64x64..128x128 images a frame) and
   update the shadow memory. Otherwise fall back to the shadow copy and drop the touched surfaces.
   The car slot copy is a surface alias (same image under the new TBP, palette likewise).
6. **Render targets**: one image per FBP (0, 70) + the shared Z, as now. A TEX0 whose TBP is a
   target's base samples that image (CT24 view: alpha ignored; TW 1024 over 640: normalise by the
   TEX0 size, clamp by the region). The post pass needs a snapshot of its own target (self-sampling);
   keep the hardware GS's snapshot, and decide the raster-order question below. Z is never sampled.
7. **Fallback = today's path**: any TEX0 that is not an exact surface hit (cases 2, 5, 6, partial
   surfaces, the 1x1) decodes from the shadow memory as `gs_hw_textures.cpp` / `resolvePack` do
   (page-version keys, `effectiveRect`). Count them (probe 1): the aim is a handful of materials.
8. **Mips**: build the chain from the game's levels (MIPTBP1/2 owners), not by generating them.

Order of work: (a) the exception census (probe 1) and the hook with parsing only (compare every
upload with the shadow memory), (b) surfaces + content cache + index images for the 2D/UI path
(few draws, fixed TBPs), (c) palettes and CLUT state, (d) copies, (e) 3D materials and mips,
(f) removal of page hashing.

## 8. Open questions and probes (none run: no GPU jobs)

1. Which buffer is displayed while the next is drawn, and why `postPassSprite(k ^ 1)` ends up
   self-sampling (section 4)? Probe: `RT_GS_BATCH_LOG` for a few frames with `RT_GS_BACKEND=cpu`
   headless, printing DISPFB writes beside the draws. Matters for the raster-order feedback of the
   shifted variant (snapshot vs in-place).
2. Census of draws whose TEX0 is not an exact surface hit (the survey records no TBP): log
   (tbp, psm, tw, th, cbp, cld, csa) at each batch in town, field, a race, menus and compare with
   the upload list (`m_uploads` in `gs_hw_backend.cpp` already holds dbp -> psm/w/h).
3. The 926 draws with a 1x1 TEX0 at TBP 0 and TME on (frame 1366 in one run): which function.
4. Is every animation copy inside its source strip (masks in `texAnimTable`)? Probe: assert in the
   CPU GS transfer path that SSAX + w, SSAY + h stay inside the source upload.
5. Kinds 3, 7, 0xB of `loadStep`, the SYS entries loaded by kind 4 and their rebases; the SCOREA
   and METER files are matched by sector count only.
6. FLD/220.BIN's overwritten palette (case 6): is the material drawn?
7. Texture packs and dumps today are keyed by decoded pixels per palette; a scrolling texture
   produces a new content every frame (the worker's `m_knownMiss`): confirm the copies' rate.
