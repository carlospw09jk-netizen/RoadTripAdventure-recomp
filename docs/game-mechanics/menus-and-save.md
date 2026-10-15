# Menus/UI and the memory-card save system

How Road Trip's 2D menu machinery drives the title, pause, options, course select, shop,
garage and paint-editor screens, and how that same machinery plus a set of
sceMcSync state machines persists the player's Adventure progress to a PS2 memory
card.

Grounded in `docs/native-reference/functions.tsv` rows for the menu cluster
(`docs/native-reference/functions/menu-core.md`, `menu-pause-options.md`,
`menu-shop-ui.md`, `paint-editor.md`) and the save cluster
(`docs/native-reference/functions/memory-card-save.md`), `config/game_symbols.toml`
(the `uiMenuWindow`/`uiMenuCreate`/`uiMenuAddItem`/`uiMenuRun` widget, the
`vifUploadCamera`/`vifDirect`/sprite helpers), and `config/game_state.toml` for the
save layout (BASLUS-20398 / BASLUS-20398, 13,384-byte progress block,
`gs_progress_ram_address 0x2C22C8` [brief] / `0x177F760` [config], `pause_task`, the
[progress], [tasks] and [controls] sections). Claims not directly observed are
marked `[inferred]`.

## How it works — menus / UI

Menus are a generic 2D task tree layered on top of the per-frame packet machinery
(`uiBatchInit`/`uiBatchFlush`/`uiSpriteWrite` and the HUD atlas at TBP `0x1A40` with
the CLUT bank at `0x1BC0`). A menu "page" is a task whose update callback runs every
frame, builds its 2D packet into a `UiBatch` arena and returns; the main loop
(`0x2003B8` / `0x200534` via the task-pool walker `0x204F98`) flushes the batches at
the end of the frame. The shared primitives are `UiMenuWindow` (`0x205350`) — a
generic panel with rows, scroll bar, cursor and an open/close fade — and its helpers
`uiMenuAddItem` (`0x2058A8`), `uiMenuRun` (`0x205C08`), `uiMenuCreate` (`0x205C20`).
All menus draw through these, so panel framing, scrolling and cursor animation share
code; per-screen code only chooses items and resolves confirm/cancel. Sound SFX
prompt helpers (one entry point at `0x258E50` taking an SFX id) accompany most
transitions, and the per-frame helpers `uiDrawHintBar` (`0x203240`),
`uiDrawPanel9` (`0x202350`), `uiDrawPanelCells` (`0x204568`) plus the `uiArena*`
family (`uiArenaInit` `0x24F558`, `uiArenaSubmit` `0x24F578`, `uiArenaSprites`
`0x24F7C8`, etc.) build the per-frame packets.

At the top of the tree sits the menu flow itself: `StartMenuFlow` (`0x2052D0`)
spawns the title's initial tasks, configures the task callback, and resets menu
state; the per-frame `UpdateMenuFlowTask` (`0x205088`) interprets input and
selects the next mode (Adventure / Quick Race / 2 Player / Options). The flow
dispatches into a `DispatchMenuTask` (`0x20A2B0`) → `RunMenuNavigationTask`
(`0x209CB0`) pair; the latter feeds `UpdateMenuScreenTask` (`0x20A580`), the four
generic run tasks `RunMenuSelectionTask` (`0x209848`), `InitializeMenuPanelTask`
(`0x2086B0`), `RunMenuPanelTask` (`0x208848`) and `UpdateMenuOptionTask`
(`0x20A150`), plus `UpdateMenuConfirmTask` (`0x20A520`) for yes/no prompts. All
five of these run tasks converge on `NavigateGridSelection` (`0x206D28`) to
translate directional input into a wrapped `(row, col)` cursor inside the page's
grid dimensions. Procedural drawing is shared: `DrawMenuGridBackground`
(`0x212190`), `DrawMenuGridBorder` (`0x2119A8`), `DrawMenuListItems` (`0x211F68`),
`DrawMenuCursorAndIcons` (`0x212720`) and `InitMenuElementLayout` (`0x212A00`)
together produce the panel background, border, text rows and the blinking cursor
for almost every screen.

Specialised pages reuse that skeleton. Course selection chains
`RunCourseSelectHubTask` (`0x22CD38`) → `RunSelectionMenuTask` (`0x22C8A0`) /
`RunCourseSelectMenuTask` (`0x22CA28`); they enumerate 20 (area, course) pairs,
build a bitmask of available courses, render them by name and pass the chosen
index back to the parent (event codes `0x1a` accept, `0x1b` cancel). The first
tick of `RunCourseSelectHubTask` builds camera, light and music
(`buildCamera 0x21F698`, `updateLightDirection 0x2261A0`) and then drives
`DrawCourseList` (`0x239A80`). Two unresolved siblings `UNKNOWN_0x00264498` and
`UNKNOWN_0x0026B848` plus its children `0x26B1B8` / `0x26B470` / `0x26B5C8`
implement the actual track picker with keyboard/left-right accept-decline and
scroll, and feed `HandleMenuScrollSelect` (`0x26D020`) which converts the
selection into an event for the parent task.

The pause menu (`pause_task 0x217148`, spawned by the town task at `0x211144`,
jump table `pause_pages 0x2EAD60` with 12 entries) keeps a small state block at
`pause_state 0x1727B90` (`+0` page shown, `+4` page requested, `+8` frames on the
page, `+C` leave). Its per-frame handler is `UpdatePauseMenuTask` (`0x214AD0`),
which renders background, borders, list items and cursor; `UpdateVerticalMenuNavigation`
(`0x212AC8`) handles cursor movement and scroll SFX. Submenu pages come from
`UpdatePauseSubmenuTask` (`0x214EF8`), the restart/quit dialog from
`UpdatePauseConfirmDialogTask` (`0x215220`) via `HandleConfirmationMenuInput`
(`0x2135C8`), audio settings (`UpdateAudioSettingsMenuTask` `0x2156D0`) use
`HandleAudioSettingsMenuInput` (`0x2137B0`) to drive BGM / SFX volumes through
the SNDMOD helpers (`0x2589A0` / `0x258A48` / `0x258AF0` / `0x258B30`), the
toggle page is `HandleToggleMenuInput` (`0x213A78`), and `UpdateWorldMapMenuTask`
(`0x216E00`) overlays the minimap with a player arrow. Controller remapping lives
on the Settings page: `HandleControllerConfigMenuInput` (`0x214470`) calls
`GetControllerButtonConfig` (`0x213C40`) / `SetControllerButtonConfig` (`0x213078`)
plus `ResetControllerConfigDefaults` (`0x214220`) to read and write the per-port
word-order masks at progress `+0x12E8`, refusing duplicate assignments, and pops
`PromptConfigConfirmationTask` (`0x2173D0`) before commit. `SetGamePausedState`
(`0x24FB38`) toggles pause flag `0x2000` at `task+0x28` and pairs it with the
sound pause/resume at `0x258FF0` / `0x259050`. The whole pause tree is initialized
once per page entry by `InitPauseMenuOptions` (`0x216FF8`) and torn down by
`UpdateOptionsMenu` (`0x2174E0`), the per-frame options-screen loop that
re-inits state on a page change and kills the task on Exit (the title's Title >
Options handler returns `0x18F` on Exit per `settings_page 0x216738`).

Shopping and tuning form a second sub-tree. `TuningGarageSceneTask` (`0x254430`)
allocates a 0x8D70-byte state block, plays the garage BGM and spawns six cars; it
runs timed stages (menus, engine sounds, camera fade) and tears them down on
exit. `CarShopVisitSceneTask` (`0x254D20`) allocates 0x8CD0, plays BGM `0xA100201`,
spawns a subtask and waits for the car (`DAT_0177ACE4`) to either reach a shop
position or match a saved-car condition before advancing flags. Inside the shop,
`GetTuningParamFromCarSlot` (`0x254C30`) reads the engine/tuning table at
`car + 0x600 + slot*12 + 0x74` (only when task flag bit 2 is set). The item-shop
flow is a three-state machine: `UNKNOWN_0x0026E50` (`0x269E50`) initialises and
flags buy-enabled, `UNKNOWN_0x00269E88` (`0x269E88`) selects one of the three sub-
handlers, and the buy/sell dialogs (the unresolved pair `0x268428` / `0x268890`,
plus `0x268E40` / `0x269568` / `0x269900`) drive the chooser. Money flows through
`SpendTimerBudget` (`0x23DC58`), which subtracts from the slot timer
`DAT_0177FDB4[slot]`, fails if insufficient, caps at 999999 and plays SFX
`0x305`. DrawShopRow (`0x237680`) emits one shop row (name, resident, price,
quantity) from the table at `0x1780750`; `BrowseItemsMenuTask` (`0x26CF20`) drives
the 2-row cursor. Outside the shops, `OpenPaintEditorScene` (`0x256958`) copies
the 32-bit paint value from the current progress entry, sets a grayscale flag
when R/G/B nibbles are mirrored, builds the menu sprites via
`BuildPaintEditorMenuSprites` (`0x255070`) and chains into
`SlideInPaintEditorWindow` (`0x256A78`); the editor itself
(`PaintEditorEditTask` `0x256640`) handles input via `HandlePaintEditorInput`
(`0x2560F0`), draws the hex digits (`DrawPaintEditorDigits` `0x2558B8`) and the
cursor pair (`DrawPaintCursorSprites` `0x2557A0`), and on confirm hands off to
`PaintEditorConfirmTask` (`0x2564E8`) which writes the new value back to the
progress entry, compares against the previous value and charges payment through
`0x23DC58`; `ClosePaintEditorWindow` (`0x2568D8`) plays a panel slide-out
animation when the editor closes.

## How it works — memory-card save

The save format on disc is the icon.sys-style PS2 layout: an `icon.sys` block and
`choroQ.ico` icon, followed by the player's save file. According to
`config/game_state.toml`, the file name is `BASLUS-20398` (a directory entry for
the game) containing two `BASLUS-20398` files (one per save slot). The playable
content is a single block of `0x3448` bytes per slot, logically equal to a slice
of the 13,384-byte "Adventure progress" block; tests and tools treat the block
verbatim from EE RAM into the save file, so the same offsets describe both
images. Progress lives at `gs_progress_ram_address` `0x2C22C8` [brief] /
`0x177F760` [config] — the brief's `0x2C22C8` is also the documented
`gs_houses_residents` residents pointer table, so the live RAM image sits in the
same buffer; `config/game_state.toml`'s resident pointer table is loaded from
that buffer. The exact mapping between the brief's `0x2C22C8` "save layout" and
the config's `0x177F760` "progress" base is `[inferred]` — they appear to be the
same progress buffer, with the differences in offsets matching the save-line /
RAM-image distinction.

The save state machine has three entry paths. **Load** is
`LoadSaveSlotTask` (`0x228B10`), which on its first tick probes the slot with
`MemcardGetInfoTask` (`0x2282C0`) — an `sceMcGetInfo`/`sceMcSync` wrapper that
classifies the type/files into a result byte — and on success calls
`MemcardReadSlotTask` (`0x228510`) to `sceMcOpen` RDONLY and `sceMcRead` the
0x3448-byte per-slot buffer, then chains to `MemcardReadFlushHandler` (`0x283E0`)
which dispatches diagnostic strings and notifies the caller. The user-facing
load menu runs through `RunCardOpSequenceTask` (`0x22A320`), `ShowSaveLoadSpinner`
(`0x228D10`) (localised "Loading..." prompts) and `SaveSlotConfirmDialog`
(`0x2280D8`) (overwrite / cancel). **Create** follows the same pattern through
`CreateOrOverwriteSlotTask` (`0x2291C8`) which expands into
`MemcardCreateSlotTask` (`0x228708`) — a mkdir → `icon.sys` → `choroQ.ico` →
save-file sequence using `sceMcOpen` / `sceMcWrite` / `sceMcClose` — plus
`MemoryCardOpenWriteTask` (`0x22A168`) and `FormatMemoryCardTask` (`0x22A218`,
which runs `sceMcFormat`/`sceMcSync`). The user face for create is
`ShowSaveCreateSpinner` (`0x229538`). **Pick-or-create** is `LoadOrCreateCardSlotTask`
(`0x228F40`), which probes slot occupancy (info + read), then dispatches between
overwrite / create / load via `MemcardChooserDialogHandler` (`0x227380`) — a
state machine over dozens of dialog phase IDs (0..0x1F) that loads localised
labels through `UiMenuAddItem` (`0x2058A8`) per phase and reports
confirm/cancel back to the parent.

Two cross-cutting helpers keep the format consistent. `InitSaveSlotDefaults`
(`0x277B0`) — called once at slot creation from `InitProgressRamDefaults`
(`0x229FD8`) — memsets the 0x3448-byte slot, then seeds the per-port action map,
default flags, limits and strings, and copies two sub-tables (UnnamedRacer and
the `gs_locations_table` prefix). `InitProgressRamDefaults` itself resets the
two save subsystems, sets the player's starting money to 1000, copies 150 slot
pointers, and installs `gs_progress_ram_address` (so `SelectProgressSlot`
`0x229F70` later re-points both `gs_progress_ram_address` and `DAT_017804d0[slot]`
at the slot's buffer from `PTR_DAT_002ED724`). For brand-new games,
`InitPlayerSlotStrings` (`0x227B28`) writes the "PLAYER 1" / "PLAYER 2" 8-byte
names and several control-flag bytes at `DAT_0177FD8C`. Per-frame drawing of the
save listings uses `DrawSaveSlotEntry` (`0x227F90`) — name, track, time,
free-blocks per row — and `RunConfirmDialogTask` (`0x22A050`) — first tick
spawns a UI panel, clamps two globals to 3, polls the panel and kills the task on
success. The save flow is also tied to the RAM image via
`RunSaveProgressTask` (`0x22BB88`), which writes course / progress records into
the save buffer, spawns follow-up tasks, rebuilds a menu panel, and prints
"Acc/Mue" debug strings `[inferred]`.

The dialog and chooser pieces are interleaved: every chooser phase in
`0x227380` produces a `UiMenuAddItem` call against `0x2058A8`; every confirm/cancel
hands back to the parent task through the task-pool's `result` slot (pool base
`0x177A0D0`, walker `0x204F98`, spawn `0x204C48`, kill `0x204DE8`); and every
visual prompt updates through the localized "LANGUAGE" globals
(`0x333284` `language`, `0x333288` `videoMode`).

### Save layout (13,384 bytes, BASLUS-20398)

The `config/game_state.toml [progress]` block, plus a handful of `INIT_` calls in
the decompiles, are the authoritative summary. All offsets are absolute within the
13,384-byte progress block (== the per-slot 0x3448 image plus its header, with the
top of the block being the slot header). Confidence labels track what's been
directly observed:

- `+0x00D` — equipped-parts bytes (six categories; `+0x0E` = equipped engine index).
- `+0x508` — `photos_taken` 16-byte bitmap (bit n-1 set = photo n).
- `+0x518` — `earned_offset` (notebook stamps, two u64, 100 stamps at bits 0..99).
- `+0x528` — town-visited word (bit n = town n visited `[inferred]`).
- `+0x538` — `flags_offset` 128-bit story flag set; WGP stage n flag = bit
  `0x65 + n - 1`. Carries `scene` low word and `town_entered`.
- `+0x548` — 16-byte `coins_missing` bitmap (100 - popcount = current coins found).
- `+0x5A8` — `mail_inbox` (128 bytes: 1B mail id + 0x80 once read); also `body_offset`
  for the President's body Q149.
- `+0x628` — `mail_count` u32.
- `+0x62C` — player name, 19 bytes.
- `+0x63F` — currency name, 21 bytes ("Enter currency unit").
- `+0x651` — licence u8 (0 none, 1 B, 2 A, 3 Super A).
- `+0x654` — money u32 (1000 at New Game).
- `+0x658` — `mileage_m` metres; save summary shows `/ 1609` as miles.
- `+0x674` / `+0x676` — WGP teammate bytes (0 = none; opcode 7 checks both).
- `+0xBD8` — `unvisited_doors` per location (40 bytes: u32 each); init masks
  `[0, 0x3DFFF, 0x7FBFF, 0x1FFFFF, 0xFBF, 0xF, 0xFFFFF, 0x3FFFF, 0x17F,
  0x7FFFDF]` per town.
- `+0xFF0` — `race_results` 35 bytes (place - 1 per table entry; 0xFF = not raced).
- `+0x1040` — `best_lap` 312 bytes (u32 frames per race entry; 0x7FFFFFFF = none).
- `+0x1178` — `best_total` 312 bytes (same layout, whole race).
- `+0x12C8` — White Mountain ski-jump best (u32 metres, 0 until first jump).
- `+0x12DA` — `towns_unvisited` u16 (0x3FF at New Game).
- `+0x12DC` — `location` u8 (save point: 1 = Peach Town .. 9 = My City).
- `+0x12DD` — `radio_station` u8 (0 off, 1 PEACH FM (SOUND/1CH), 2 E-RADIO
  (SOUND/3CH)).
- `+0x12E2` — `last_photo` u16.
- `+0x12E8..+0x12FA` — controller remap u16s (game word order), populated by
  `InitTuningConstants` (`0x229F20`).
- `+0x3300` — `queue_offset` u32 (queued stamps, up to 8 u16 following).
- `+0x3310` — `results_offset` (WGP stage results).
- `+0x32E240` (in ELF) — `captions_address` ("mail %d/%d" then 100 captions,
  stamp 100 first).

A handful of additional state lives inside the same image but is not in
`[progress]`:

- `0x177FD8C` — `DAT_0177FD8C` per-port "PLAYER 1"/"PLAYER 2" string block
  (initialised by `InitPlayerSlotStrings`, called from the selection task;
  `[inferred]` offset relative to `gs_progress_ram_address`).
- `0x177FDB4` — `DAT_0177FDB4` per-slot timer (decremented by `SpendTimerBudget`,
  999999 cap).
- `0x17804D0` — `DAT_017804D0[slot]` per-slot pointer table (re-pointed by
  `SelectProgressSlot`).
- `0x2ED724` — `PTR_DAT_002ED724` table of 150 slot pointers, copied by
  `InitProgressRamDefaults`.
- `0x2C22C8` — `gs_houses_residents` (per-location resident pointer table; the
  brief uses this as the progress base).

## Key functions

### Menu / UI cluster (`menu-core.md`, `menu-pause-options.md`, `menu-shop-ui.md`, `paint-editor.md`, `ui-widgets-layout.md`)

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x2003B8` | `gameMainLoop` | Per-frame loop: kicks tasks, flushes the UI batch, swaps buffers | — | `0x204C48`, `0x204F98`, ui helpers |
| `0x2038F8` | `uiBatchInit` | Two UiBatch buffers: header + TEX0 atlas `0x1A40` + CLUT bank `0x1BC0` | `0x2000C8`+ | ui drawers |
| `0x205350` | `uiMenuWindow` | Generic list panel (rows, scroll bar, cursor, fade) | 10 callers | `0x205AD0`, ui draws |
| `0x2058A8` | `uiMenuAddItem` | Append a row to a `uiMenuWindow` | save + menu flows | `0x205C20`, `0x205C08` |
| `0x205088` | `UpdateMenuFlowTask` | Top-level menu state: input → selected mode | `0x2052D0` | `0x20A660`, `0x20AB10`, `0x20E018`, `0x258578`, `0x204C48` |
| `0x2052D0` | `StartMenuFlow` | Boots menu flow, configures task callback, resets state | `0x2003B8` | `0x204ED0`, `0x258578`, `0x204C48` |
| `0x206D28` | `NavigateGridSelection` | Translates directional flags into wrapped grid index | all grid menus (10 callers) | `0x258E50` |
| `0x2086B0` | `InitializeMenuPanelTask` | Initialises a menu panel task + transition state | `0x20A2B0` | `0x2063C8`, panel helpers, `0x204C48`, `0x226F70` |
| `0x208848` | `RunMenuPanelTask` | Per-frame panel update: spawns sub-tasks, transitions | `0x20A2B0` | `0x21F910`, `0x225790`, ui arena + sprite, `0x226F70`, `0x2272A48` |
| `0x209848` | `RunMenuSelectionTask` | Selection driver: pad → state → child menu | `0x205088` | menu + chooser, `0x204C48`, `0x207B50` |
| `0x209CB0` | `RunMenuNavigationTask` | Navigation driver: present the selected item | `0x20A2B0` | menu helpers, `0x204C48`, `0x207B50` |
| `0x20A150` | `UpdateMenuOptionTask` | Per-option update + transition dispatch | `0x20A2B0` | `0x204310`, `0x2080F8`, sound helpers, `0x204DE8` |
| `0x20A2B0` | `DispatchMenuTask` | Picks and spawns the next menu task | `0x205088`, `0x2052D0` | `0x204D60`, `0x204ED0`, ui arena, `0x204DE8`, `0x204C48` |
| `0x20A520` | `UpdateMenuConfirmTask` | Spawns confirmation task, processes completion | `0x20A2B0` | `0x204DE8`, `0x204C48` |
| `0x20A580` | `UpdateMenuScreenTask` | Screen tick + timer/transition | `0x209CB0` | `0x2080F8`, `0x204DE8`, `0x204C48` |
| `0x2119A8` | `DrawMenuGridBorder` | Procedural grid borders, cell dividers, frames | 6 menu drawers | `0x24F640`, `0x24F728`, `0x24F7C8` |
| `0x211F68` | `DrawMenuListItems` | Menu text labels inside grid cells | 4 menu drawers | `0x24F530`, `0x24F640`, `0x24F728`, `0x24F7C8` |
| `0x212190` | `DrawMenuGridBackground` | Tiled background panels per grid style | 6 menu drawers | `0x24F640`, `0x24F728`, `0x24F7C8`, `0x27E840` |
| `0x212720` | `DrawMenuCursorAndIcons` | Cursor arrows (blink anim) + item icons | 6 menu drawers | `0x24F640`, `0x24F728`, `0x24F7C8`, `0x27E840` |
| `0x212A00` | `InitMenuElementLayout` | Element layout positions for vertical menu items | 6 menu drawers | — |
| `0x212AC8` | `UpdateVerticalMenuNavigation` | Vertical cursor nav + scroll SFX + transition anim | `0x214AD0`, `0x214EF8` | `0x206D28`, `0x258E50` |
| `0x2135C8` | `HandleConfirmationMenuInput` | Three-option dialog nav/confirm/cancel | `0x215220` | `0x206D28`, `0x258E50` |
| `0x2137B0` | `HandleAudioSettingsMenuInput` | BGM/SFX volume levels + options | `0x2156D0` | `0x206D28`, `0x2589A0`/`0x258A48`/`0x258AF0`/`0x258B30`, `0x258E50`, `0x2590B0` |
| `0x213A78` | `HandleToggleMenuInput` | Toggle-page transitions; sets bit 0x80 on confirm/cancel | `0x2161E0` | `0x258E50` |
| `0x213C40` | `GetControllerButtonConfig` | Reads button bindings at player-state `+0x12E8`, returns config index | `0x214220`, `0x214470` | — |
| `0x214078` | `SetControllerButtonConfig` | Writes button assignments back into player control mapping | `0x214220`, `0x214470` | — |
| `0x214220` | `ResetControllerConfigDefaults` | Resets button bindings, checks for collisions, updates fields | `0x214470` | `0x213C40`, `0x214078`, `0x229F20` |
| `0x214470` | `HandleControllerConfigMenuInput` | Button remap nav, validates duplicate assignments | `0x216738` | `0x206D28`, `0x213C40`, `0x214078`, `0x214220`, ui arena |
| `0x214AD0` | `UpdatePauseMenuTask` | Pause menu nav + render: bg, borders, items | `0x217148` | `0x203240`, `0x2119A8`, `0x211F68`, `0x212190`, `0x212720`, `0x212A00`, `0x212AC8`, ui arena, `0x204C48` |
| `0x214EF8` | `UpdatePauseSubmenuTask` | Secondary pause submenu nav + render | `0x217148` | `0x2119A8`, `0x211F68`, `0x212190`, `0x212720`, `0x212A00`, `0x212AC8`, ui arena |
| `0x215220` | `UpdatePauseConfirmDialogTask` | Restart/quit dialog input + render | `0x217148` | `0x2119A8`, `0x212190`, `0x212720`, `0x212A00`, `0x2135C8`, ui arena, `0x204C48` |
| `0x2156D0` | `UpdateAudioSettingsMenuTask` | Audio options: volume sliders, time of day, SFX toggles | `0x217148`, `0x211E0` | `0x2119A8`, `0x211F68`, `0x212190`, `0x212720`, `0x212A00`, `0x2137B0`, `0x24EB10`, ui arena, `0x27E840` |
| `0x216100` | `UNKNOWN_0x002161E0` (Garage) | Pause-menu garage page (items, descriptions, mileage) | `0x217148` | `0x202150`, `0x202350`, `0x203F40`, `0x205350`, ui arena, `0x213A78`, `0x22E780`, `0x23D488`, `0x23D5B0` |
| `0x216E00` | `UpdateWorldMapMenuTask` | World map page; draws player marker at mapped world coords | `0x217148` | `0x24F530`, `0x24F558`, ui arena, `0x27E840` |
| `0x216FF8` | `InitPauseMenuOptions` | Initialises pause state, populates option list by unlock flags | `0x2174E0`, `0x217148` | `0x27E840` |
| `0x2173D0` | `PromptConfigConfirmationTask` | Confirmation popup, returns index to parent | `0x214470` | `0x2058A8`, `0x205C08`, `0x205C20`, `0x204DE8` |
| `0x2174E0` | `UpdateOptionsMenu` | Per-frame options loop: inits on entry, resets buffer on page change, kills task on exit | — | `0x216FF8`, `0x216738`, `0x204DE8`, `0x27E840` |
| `0x21F698` | `buildCamera` | Builds a 0x190-byte camera block (matrices +0/+0x40/+0x80/+0xC0/+0x100) | `0x22CD38`+scene setters | matrix helpers |
| `0x22C8A0` | `RunSelectionMenuTask` | Up to 10 unlocked progress entries → reported index (event `0x1A`/cancel `0x1B`) | `0x22CD38` | menu primitives, `0x23D488`, `0x23D5C8`, `0x243468`, `0x258E50`, `0x204DE8` |
| `0x22CA28` | `RunCourseSelectMenuTask` | 20 (area, course) pairs → bitmask, render by name | `0x22CD38` | `0x205350`, `0x205C20`, `0x205DA0`, `0x208228`, `0x23D5F8`, `0x258E50`, `0x204DE8` |
| `0x22CD38` | `RunCourseSelectHubTask` | First tick builds camera/light/music, then routes to selection menus + course | `0x22D1E0` | `0x21F910`, `0x225790`, ui arena, `0x23C7B0`, `0x23D5B0`, `0x258E50`/`0x258F30`, `0x272C38`, `0x274AF8` |
| `0x22D1E0` | `OpenCourseSelectEntry` | Awards stamp `0x28` if eligible, resets progress entries, chains to hub | `0x2105B8`, `0x211708`, `0x211808` | `0x22C648`, `0x22CD38`, `0x23D2C0`, `0x23D628`, `0x23DF38`, `0x23C8A8` |
| `0x239A80` | `DrawCourseList` | Range of courses with names + ranks; on confirm spawns message task `0x23BB00` | `0x23B168` | menu primitives, `0x23D628`, `0x258E50`, `0x204DE8`, `0x204C48` |
| `0x23DC58` | `SpendTimerBudget` | Subtracts argument from `DAT_0177FDB4[slot]`; fails if insufficient, cap 999999, plays SFX `0x305` | 10 callers (shops, betting UI, paint editor) | `0x258E50` |
| `0x2455C0` | `drawCarouselScene` | Quick Race course carousel (tile 1 cells) | scene drawer | carousel helpers |
| `0x254430` | `TuningGarageSceneTask` | 2-player garage/tuning scene state machine (0x8D70 state, BGM, 6 cars, timed stages) | — | `0x207F88`, `0x2080F8`, car spawns, sound + ui arena, `0x204DE8`, `0x27E840` |
| `0x254CC8` | `GetTuningParamFromCarSlot` | Tuning-table value from car `+0x600 + slot*12 + +0x74` (only when task flag bit 2 is set) | — | `0x253680` |
| `0x254D20` | `CarShopVisitSceneTask` | Car shop visit: BGM `0xA100201`, waits for car at shop position or saved-car match | — | `0x2080F8`, car spawns, `0x235708`/`0x2349C8`/`0x2352D8`, sound + ui arena, `0x2729D0` |
| `0x255070` | `BuildPaintEditorMenuSprites` | Sprite packet: 0x175B570 bytes with grid, RGB swatches (R/G/B/M/C/Y), four labels | `0x256958` | sprite write helpers, `0x226F70`, `0x206040`/`0x2063C8` etc. |
| `0x2557A0` | `DrawPaintCursorSprites` | Two 32x32 cursor sprites from table `0x2AC600`, blink timer → static sprites | `0x256640` | `0x2063C8` |
| `0x2558B8` | `DrawPaintEditorDigits` | Digit sprites for 6 paint-value nibbles, dim by player cursor, hex digit chars, item count banner, turn arrows | `0x2564E8`, `0x256640` | `0x202150`, `0x203CD0`, `0x2063C8`, `0x207F90`, `0x207998` |
| `0x2560F0` | `HandlePaintEditorInput` | Cursor over 6 nibbles/palette; +1 nibble of RGB in progress record, wraps, plays SFX `0x1D`, swaps RGB | `0x256640` | `0x258E50` |
| `0x2564E8` | `PaintEditorConfirmTask` | Confirm step: redraws digits, on commit stores 32-bit paint into progress entry, compares → `SpendTimerBudget` | `0x256640` | `0x22E7F0`, `0x23DC58`, `0x2558B8`, `0x258E50`, `0x226F70` |
| `0x256640` | `PaintEditorEditTask` | Edit step: input → cursor/digit draw, blink timer, X/cancel transitions to confirm/cancel/no-money dialog | `0x2564E8`, `0x256A78`, `0x256958` | `0x207A28`, `0x22E7F0`, `0x2557A0`, `0x2558B8`, `0x2560F0`, `0x258E50`, `0x226F70`, `0x204C48`, `0x226F70` |
| `0x2568D8` | `ClosePaintEditorWindow` | Slide-out: `task+2` timer `--4`, moves panel sprite, kills task when offset hits `-0x38` | `0x2564E8`, `0x256640` | `0x2063C8`, `0x204DE8`, `0x226F70` |
| `0x256958` | `OpenPaintEditorScene` | Reads 32-bit paint value from progress entry, sets grayscale flag if RGB mirrored, builds menu sprites, slide-in or hand off | `0x22AEB0` | `0x255070`, `0x256A78` |
| `0x256A78` | `SlideInPaintEditorWindow` | Slide-in opposite of close: increments `task+2` by 4 until 0, moves sprite, then hands off to edit task | `0x256958` | `0x2063C8`, `0x226F70` |
| `0x24FB38` | `SetGamePausedState` | Toggles pause flag `0x2000` at `task+0x28`; pairs with sound pause/resume `0x258FF0`/`0x259050` | `0x251B60`, `0x25A120`, `0x260DC0` | `0x258FF0`, `0x259050` |
| `0x26C588` | `creditsScene` | Ending-credits packet (vifDirect `0x1772670`, 0x9D qw) | ending | `0x226E88` |

### Save cluster (`memory-card-save.md`)

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x227380` | `MemcardChooserDialogHandler` | Dialog phase state machine (phases 0..0x1F): localises labels via `uiMenuAddItem`, advances phase, marks confirm/cancel | `0x209848`, `0x228B10`, `0x228F40`, `0x2291C8` | `0x2058A8`, `0x205C08`, `0x205C20`, `0x258E50`, `0x204DE8` |
| `0x2277B0` | `InitSaveSlotDefaults` | Memset 0x3448 save slot; seeds action map, flags, limits, strings and two sub-tables (UnnamedRacer + `gs_locations_table` prefix) | `0x229FD8` | `0x250DD0`, `0x27E840` |
| `0x227B28` | `InitPlayerSlotStrings` | Builds "PLAYER 1"/"PLAYER 2" 8-byte names + control-flag bytes at `DAT_0177FD8C` | `0x209848` | — |
| `0x227C68` | `InitControlsBindings` | Bootstraps control pad remap/wpad tables via `0x23C418`; pre-registers 0x80 phases via `0x23DF38` | `0x26B848` | `0x23C418`, `0x23D2C0`, `0x23D7A8`, `0x23DF38` |
| `0x227F90` | `DrawSaveSlotEntry` | One save listing row: player name, track, time, free-blocks (row offset `param_2`) | `0x2280D8`, `0x22A050` | `0x201E48`, `0x202150`, `0x204310`, `0x204568` |
| `0x2280D8` | `SaveSlotConfirmDialog` | Save-confirm dialog (overwrite prompt) via `uiMenuAddItem` + `DrawSaveSlotEntry` | `0x228B10`, `0x2291C8` | `0x2058A8`, `0x205C08`, `0x205C20`, `0x227F90`, `0x204DE8` |
| `0x2282C0` | `MemcardGetInfoTask` | `sceMcGetInfo` async wrapper; on phase 0 kicks GetInfo, waits via `sceMcSync`, classifies type/files → result byte; timeouts abort | `0x228B10`, `0x228F40`, `0x2291C8`, `0x22A320` | `0x27A1A8`, `0x27A2E0`, `0x27EA30`, `0x204DE8` |
| `0x2283E0` | `MemcardReadFlushHandler` | Async `sceMcSync` handler: branches size error/ok; on success dispatches "Close"/"Done" diagnostics, notifies caller | `0x228510` | `0x279C78`, `0x27A1A8`, `0x27EA30`, `0x204DE8` |
| `0x228510` | `MemcardReadSlotTask` | `sceMcOpen` RDONLY; `sceMcRead` 0x3448 bytes into per-slot progress buffer; chains to flush handler | `0x228B10`, `0x228F40`, `0x2291C8`, `0x22A320` | `0x279B08`, `0x279E98`, `0x27A1A8`, `0x27EA30`, `0x204DE8` |
| `0x228708` | `MemcardCreateSlotTask` | Multi-step mkdir → `icon.sys` → `choroQ.ico` → save file via `sceMcOpen`/`Write`/`Close` | `0x2291C8`, `0x22A168` | ui draws, `0x258E50`, sceMc helpers, `0x204DE8`, `0x204C48` |
| `0x228B10` | `LoadSaveSlotTask` | Async state machine wrapping GetInfo → Read → save-confirm dialog (load selected slot) | `0x228D10` | `0x204C48`, `0x258E50`, `0x204DE8` |
| `0x228D10` | `ShowSaveLoadSpinner` | Localised "Loading..." spinner per language; chains to `LoadSaveSlotTask` once confirmed | `0x209520`, `0x20A580` | `0x204C48`, `0x204DE8`, menu primitives, `0x258E50` |
| `0x228F40` | `LoadOrCreateCardSlotTask` | Probes slot occupancy (info + read), dispatches overwrite/create/load via `MemcardChooserDialogHandler` and kill targets | `0x209848` | `0x204C48`, `0x204DE8` |
| `0x2291C8` | `CreateOrOverwriteSlotTask` | Full create/overwrite flow: spawns info/read/create (`0x28708`) and confirm (`0x280D8`) tasks by sub-status | `0x229538`, `0x229758` | `0x204C48`, `0x258E50`, `0x204DE8` |
| `0x229538` | `ShowSaveCreateSpinner` | Localised "Saving..." spinner; transitions to `CreateOrOverwriteSlotTask` | `0x23B168` | `0x204C48`, `0x204DE8`, menu primitives, `0x258E50` |
| `0x229758` | `CommitSaveSlotTask` | Copies slot buffer to staging `DAT_00110000`, async calls `CreateOrOverwriteSlotTask` with packed slot index, copies back on finish | `0x248BF0` | `0x204C48`, `0x204DE8` |
| `0x229AF8` | `MemcardSpaceCheckDialog` | Localised prompts ("Memory card 8MB... is not inserted", "Do you want to start?") via `uiMenuAddItem`; chains to `0x22A320` | `0x20A520` | `0x204C48`, `0x2058A8`, `0x205C08`, `0x205C20` |
| `0x229F20` | `InitTuningConstants` | Writes 10 fixed u16 tuning constants at offsets `0x12E8..0x12FA` of passed object | `0x214220` | — |
| `0x229F70` | `SelectProgressSlot` | Points `gs_progress_ram_address` and `DAT_017804D0[slot]` at the slot buffer from `PTR_DAT_002ED724` | `0x26D438` | `0x23D2C0` |
| `0x229FD8` | `InitProgressRamDefaults` | Resets two save subsystems, sets money to 1000, copies 150 slot pointers, sets `gs_progress_ram_address` | `0x209848`, `0x211170`, `0x26B848`, `0x26D438` | `0x277B0` |
| `0x22A050` | `RunConfirmDialogTask` | First tick spawns UI panel + clamps two globals to 3; later polls panel, kills task on success | `0x209848` | `0x2058A8`, `0x205C08`, `0x205C20`, `0x227F90`, `0x204DE8` |
| `0x22A168` | `MemoryCardOpenWriteTask` | `sceMcOpen` step: reports error code on failure + kills task, else installs next handler | `0x2291C8` | `0x204DE8`, `0x27EA30`, `0x279B08` |
| `0x22A218` | `FormatMemoryCardTask` | Draws dialog → `sceMcFormat` → `sceMcSync`, reports result to parent, kills task | `0x2291C8` | `0x204310`, `0x204568`, `0x204DE8`, `0x27EA30`, `0x27A798`, `0x27A1A8` |
| `0x22A320` | `RunCardOpSequenceTask` | Spawns `0x2282C0`; maps result (1→1, other→2) to parent, or `0x228510` on result 2 | `0x229AF8` | `0x204DE8`, `0x204C48` |
| `0x22BB88` | `RunSaveProgressTask` | Writes course/progress records to save buffer, spawns follow-ups, rebuilds menu panel, prints Acc/Mue debug | `0x22AEB0`, `0x248580` | `0x205350`, `0x205C20`, `0x205DA0`, `0x23C418`, `0x204DE8`, `0x204C48`, `0x27EA30` |

## Data structures / globals

### Menu/UI

- `0x177A0D0` — task pool base, `0x30` tasks of `0x30` bytes each
  (`[tasks.pool]` in `config/game_state.toml`).
- `0x335908` — `frameParity` (frame buffer 0/1).
- `0x335680 / 0x335684 / 0x335688` — `uiBatchPtr` / `uiBatchCount` /
  `uiBatchPalette` (current UiBatch buffer entry point, qw count, TEX2 CSA
  cached by drawers).
- `0x3362E0 / 0x340350` — `uiBatchBufferA / uiBatchBufferB` (0xA070 bytes,
  header + 0xA00 entries).
- `0x333284` — `language` (0 EN .. 5+); picks UI hint strings.
- `0x1727B90` — `pause_state` (`pause_task` state block): `+0` page shown, `+4`
  page requested, `+8` frames on page, `+C` leave.
- `0x2EAD60` — `pause_pages` jump table (12 entries).
- `0x335923` — `location_address` (u8 current town).
- `0x33590A` — `game_mode` (0 Adventure, 1 Quick Race, 2/3/6 2 Player, 8 attract
  demo; only `0x20A150` sets the demo byte).
- `0x3356C4 / 0x3356D8 / 0x3356CC` — `scene3d_active` / `scene3d_handler` /
  `ui_context` (active 3D scene state).
- `0x177F720` — `sceneParams` (3 qw VU1 0x13: time-of-day weights + fog).
- `0x21C928` (`StoreIndexedPosition`) — fixed-point world position helpers shared
  by car/update and the menu car renderers.
- `0x29AFA0` — main-menu / title UI context ("the progress block is stale there").

### Save / progress

- `0x177F760` (config) / `0x2C22C8` (brief) — `gs_progress_ram_address` —
  Adventure progress block base (13,384 bytes). The brief's `0x2C22C8` is also
  `gs_houses_residents` (residents pointer table); the live RAM image sits in the
  same buffer as the save buffer.
- `0x177FD8C` — `DAT_0177FD8C` per-port "PLAYER 1"/"PLAYER 2" string block.
- `0x177FDB4` — `DAT_0177FDB4` per-slot timer; mutated by
  `SpendTimerBudget 0x23DC58`.
- `0x17804D0` — `DAT_017804D0[slot]` per-slot pointer table (re-pointed by
  `SelectProgressSlot 0x229F70`).
- `0x2ED724` — `PTR_DAT_002ED724` table of 150 slot pointers (copied by
  `InitProgressRamDefaults 0x229FD8`).
- `0x110000` — `DAT_00110000` save-buffer staging area (used by
  `CommitSaveSlotTask 0x229758` while it waits for the create/overwrite task).
- `0x335928` — `drive_flags` (u32): `0x04 can drive`, `0x20 racing`,
  `0x08 finished`, `0x10000` hides HUD, `0x400000` loading blur.
- `[progress]` block in `config/game_state.toml` — the full save layout
  enumeration (offsets in the 13,384-byte block; confidence labels per field).
- The save state machine's image is the 0x3448-byte per-port block at
  `gs_progress_ram_address + port * 0x3448`; this is the same layout
  `InitSaveSlotDefaults` memsets and seeds.

## Control flow

### Per-frame menu flow

1. `0x2003B8` `gameMainLoop` ticks the task pool walker
   `0x204F98(pool=0x177A0D0, &0x335900)`. Each active task callback fires; menu
   page tasks run their `Update*MenuTask` (e.g. `0x214AD0` for pause,
   `0x2156D0` for audio, `0x216100` for garage) and build their per-frame
   packet into the active UiBatch arena via the `uiArena*` family.
2. Cursor input is decoded by `NavigateGridSelection 0x206D28` and the per-page
   `Run*MenuTask` (`0x209848`/`0x209CB0`/`0x20A150`/etc.). Confirm/cancel routes
   to `UpdateMenuConfirmTask 0x20A520`. Pause transitions use
   `SetGamePausedState 0x24FB38` (flag 0x2000) + sound pause/resume
   `0x258FF0`/`0x259050`.
3. The paint editor sub-flow chains `OpenPaintEditorScene 0x256958` (read
   paint, build sprites, slide-in) → `PaintEditorEditTask 0x256640`
   (cursor/nibble/digit draw + input) → `PaintEditorConfirmTask 0x2564E8`
   (store value, charge via `0x23DC58`) → `ClosePaintEditorWindow 0x2568D8`
   (slide-out).
4. At end of frame, `uiBatchFlush 0x203810` appends TEXA, patches NLOOP, runs
   `vifDirect 0x226F70` for the batch, and the main loop swaps buffers via
   `frameBlend 0x208120`.

### Per-frame save flow

1. The save chooser / load-create task is started by the title menu
   (`0x209520` `titleNewGameContinue`) or the pause menu via the New Game /
   Continue path; the first paint of `LoadOrCreateCardSlotTask 0x228F40` probes
   slot occupancy with `MemcardGetInfoTask 0x2282C0` (async `sceMcGetInfo`).
2. Based on the GetInfo result, the chooser either creates the slot (mkdir →
   `icon.sys` → `choroQ.ico` → save file via `MemcardCreateSlotTask 0x228708`)
   or loads it (`MemcardReadSlotTask 0x228510` `sceMcRead` 0x3448 bytes per
   slot, dispatched by `MemcardReadFlushHandler 0x2283E0`).
3. While the I/O is async, `ShowSaveLoadSpinner 0x228D10` /
   `ShowSaveCreateSpinner 0x229538` display the localised "Loading..." or
   "Saving..." prompts through `uiMenuAddItem 0x2058A8` +
   `uiMenuCreate 0x205C20` + `uiMenuRun 0x205C08`. The chooser dialog itself
   runs `MemcardChooserDialogHandler 0x227380`, which owns the menu phase
   machine.
4. On commit, `CommitSaveSlotTask 0x229758` copies the slot buffer to
   `DAT_00110000`, then re-issues the create/overwrite task with the packed
   slot index, copying back on finish. New slots go through
   `InitSaveSlotDefaults 0x277B0` (called from
   `InitProgressRamDefaults 0x229FD8`) which seeds money=1000, the action map,
   the 0/0x3DFFF/0x7FBFF/... `initial_unvisited` door masks and the
   `UnnamedRacer` + `gs_locations_table` prefixes.
5. Once loaded, `SelectProgressSlot 0x229F70` re-points
   `gs_progress_ram_address` and `DAT_017804D0[slot]` at the slot buffer;
   subsequent reads/writes by `RunSaveProgressTask 0x22BB88`, the paint
   editor (`0x2564E8`), the shops (`SpendTimerBudget 0x23DC58` reads the per-
   slot timer) and the WGP step (`0x22CD38`) all hit this buffer.

## Open questions

- The exact relationship between the brief's `0x2C22C8` (used as
  `gs_progress_ram_address`) and the config's `0x177F760` is inferred: they
  appear to be the same RAM buffer, but the brief specifies one and the
  config the other `[inferred]`.
- The chooser/confirm dialog phase ID mapping inside
  `MemcardChooserDialogHandler 0x227380` is broad (0..0x1F) but exact phase
  labels per dialog are unknown.
- `InitSaveSlotDefaults 0x2277B0` seeds two sub-tables "UnnamedRacer" and
  `gs_locations_table` prefix — both names are taken from the decompile note;
  the exact table layout, capacity, and field meanings are not documented
  `[inferred]`.
- The chooser dialog's parent (a `_x0` chooser) — how many slots it supports,
  whether there are two or fifteen, and which `+ slot * 0x3448` slice is the
  active one — is not pinned down `[inferred]`.
- The save-file name uses one or two `BASLUS-20398` files per slot;
  `SaveSlotConfirmDialog 0x2280D8` shows overwrite, but the exact per-port /
  per-slot selection logic is unclear.
- `PauseSettings 0x16738` returns `0x18F` on Exit per the config — but the
  exact page-jump table layout (`0x2EAD60` 12 entries, byte 0x18F=
  exit-callback) is not yet decoded.
- Paint editor: the "grayscale" flag set when R/G/B nibbles mirror one another
  (`OpenPaintEditorScene 0x256958`) and the per-port paint value's location in
  the progress block are `[inferred]`; not yet pinned to a `[progress]`
  offset.
- The `(area, course)` mapping (`0x22CA28`) over 20 entries vs 9 towns and
  their menus — i.e. how the bitmask of available courses is built and where
  the 20-area list lives — is `[inferred]` from the decompile-only notes.
- Shop state machine: the three sub-handlers `0x268E40`/`0x269568`/`0x269900`
  carry UNKNOWN_ prefixes because their exact state semantics aren't fully
  characterised.
- Whether the pause state's `+0`/`+4`/`+8`/`+C` page fields represent an
  open/close animation index beyond 0xC is `[inferred]`.
- The full set of enums that `MemcardChooserDialogHandler 0x227380`'s "0..0x1F"
  covers — load/create/format/no-card/reading/writing/etc. — is implied by
  the per-call labels but not enumerated.
