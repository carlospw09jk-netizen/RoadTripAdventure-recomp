# Sound engine (SNDMOD RPC) and day/night cycle

Two subsystems documented here: the **sound engine**, an EE-side wrapper cluster around a SIF RPC
service (`SNDMOD`, server id `0x1234567`) running on the IOP; and the **day/night cycle**, a
vblank-tick clock that blends day/dusk/night lighting presets and fog parameters every frame.

## How it works

### 1. Sound engine (SNDMOD RPC)

All game audio is delegated to an IOP-side sound server registered under SIF RPC id `0x1234567`.
The EE never mixes audio itself; it stages a `0x40`-byte request in a shared buffer at `0x175E880`
and issues `sceSifCallRpc` against the client block at `0x175E840`. Boot init (`InitSoundRpcDriver`
at `0x256AF8`, called once from `0x2003B8`) binds the client with `sceSifBindRpc(0x175E840,
0x1234567, …)`, performs a handshake (cmd `0`), sends port/config pairs (cmd `5`, four times) and
register writes (cmd `4`, four times), programs the DGA/DMA2 registers through the packet emitter
`0x2793D0`, then registers the audio container files: three `SoundRpcRegisterFiles` calls (cmd
`0x32`, two names per slot) followed by alternating cmd `6`/cmd `7` requests that attach named
files (from the music file name records around `0x29ADF4`–`0x29AE7C` and `gs_music_file_table`) to
slots 0–3. The request buffer's first two dwords carry the command's parameters; the remaining tail
is either zero-filled (file-registration paths) or copied from the caller's stack tail (music/SE
one-shots) [inferred: only the first qword is semantically meaningful].

At runtime there are three command families. **BGM**: `gs_music_play_bgm` (`0x258EC8`) takes a
packed BGM id (e.g. `0x0a1c0201`) and issues cmd `0xF` param `0` (stop/reset current), cmd `0xE`
with params `(0, id)` (start playback — `MusicRpcPlayBgm` `0x257F30`), then cmd `7` selecting the
file record `gs_music_file_table[(id >> 16) & 0xff]` (12-byte records: name pointer + two words)
with parameter `(id >> 8) & 0xff`. The `PlayBgmSlot1`–`PlayBgmSlot6` wrappers (`0x258F30`–`0x258FD0`)
call `MusicRpcPlayBgm` with `(slot, 0)` for scene music. **Sound effects**: `PlayUiSound`
(`0x258E50`, 125+ callers) forwards `id | 0x40400000` to `SndRpcPlayOrStop` (`0x258308`), which
sends cmd `1` (stop-all) when the masked id is zero, else cmd `0xD` with the id as the request's
first dword; the options/volume path plays SEs via cmd `0x33` + commit cmd `0x34`. **Teardown /
volume**: cmd `0x25`/`0x26` open and fade per-channel voices, cmd `0xF` param `1` stops BGM, cmd
`0x31` pushes master volume (scaled from the amp global `0x1780A3E` in `ApplySoundVolumes`
`0x2590B0`), cmd `0x20`/`0x21` stop-all and set channel volumes. Query commands (`8`, `0x10`,
`0x1F`, `0xFFF0`) read replies back into `0x175F880`.

Every wrapper ultimately calls the bundled `sceSifCallRpc` (`0x287EA8`), which grabs a SIF command
slot, write-backs the D-cache of the send buffer and — for the mode-0 (blocking) calls the sound
wrappers all use — signals a semaphore and waits for the IOP to complete the command.

### 2. Day/night cycle and lighting

The in-game clock is a tick counter at world-state offset `+0x14`, counted in vblanks since
midnight and wrapping at 216,000 (`0x34BC0`) in `UpdateTimeOfDayCounter` (`0x2114E0`, called from
task handler `0x2156D0`) [inferred: 216,000 ticks at 60 Hz ⇒ one full in-game day per real hour].
`updateTimeOfDayLight` (`0x225988`) is the heart of the system: given the clock it computes three
blend weights (day, dusk, night), fog parameters and sky/fog colours, then blends two 4-row light
presets. The clock maps to five phase bands:

| Clock ticks (from midnight) | Phase | Weights (x=day, y=dusk, z=night) | Fog far `0x177F73C` / fog colour |
|---|---|---|---|
| `0` – `49,500` (`0xC15C`) and `166,500` – wrap | night | x=0, y=0, z=`DAT_00332DA8` (const) | far 300, colour `0x80080000` (near-black) |
| `49,500` – `58,500` (`0xE484`) | dawn | x ramps 0→1, z = `DAT_00332DB0` − x | far ramps 300→800 (− fog adjust), grey-ramp sky |
| `58,500` – `153,000` (`0x255A8`) | day | x = `DAT_00332DB4` (const), others 0 | far 800, colour `0x80FFFFFF` (white) |
| `153,000` – `162,000` (`0x278D0`) | sunset | y ramps 0→1, x = `DAT_00332DBC` − y | far 800, sky white→orange ramp |
| `162,000` – `166,500` (`0x28A64`) | dusk→night | z ramps in (`DAT_00332DC0/DC4`), y = `DAT_00332DC8` − z | far ramps 800→300, sky darkens |

The first argument of `updateTimeOfDayLight` is a fog-distance adjust (clamped to 300; the lighting
task passes the global `DAT_0177ACE4`). When it is ≤ −2.5 an alternate fixed short-fog set is used
(far 300, `fogFarParam` 1.0, dimmer fog colours) in every transitional band [inferred: a weather
/ overcast switch]. Weights are stored to `sceneParams` (`0x177F720` x / `0x177F724` y /
`0x177F728` z, the "fog" word at `0x177F730`), and each of the 4 rows of `todLightDay/Dusk/Night`
(`0x29D500/0x29D540/0x29D580`) is blended weight-wise into `lightMatrixBlend` (`0x177F620`); the
second set `todLight2Day/Dusk/Night` (`0x29D5C0/0x29D600/0x29D640`) blends into `lightMatrixBlend2`
(`0x177F660`). Fog outputs land in `fogFarParam` (`0x177F738`, 1.0 night / 0.5 day),
fog far (`0x177F73C`, 300–800), `fogColourRGBA` (`0x177F750`) and sky colour (`0x177F754`, also
written to the caller's `+0x50` field).

The cycle is *scripted* per area rather than free-running: the lighting task `ApplyTimeOfDayLighting`
(`0x26CA48`) is spawned with a step index, sets the clock to `DAT_002DCD28[step]`, spawns town
entities and town mode on its first tick, fades in (`fadeOutTask` `0x208270`) and then calls
`updateTimeOfDayLight` every frame. `AdvanceDayCycleStep` (`0x26D1C0`) watches its own tick counter
against the step target times `DAT_002DCD50[step]` (spawning `fadeInTask` `0x2082D8` 64 ticks
before the switch), kills/respawns the lighting task for each of the steps (0…9, terminating after
step 9), and finally flags completion on its parent struct. Area entry points
`StartAreaIntroDayCycle` (`0x26CB60`, area `0xA`) and `StartDayCycleStepTask` (`0x26D2C0`, area
`0xB`) sequence the steps and their BGM (`0x0a1c0201`, `0x0a1a0201`). Race mode bypasses the clock
with `setFixedLight` (`0x226248`), which copies a 16-byte preset (sky colour, fog colour,
`fogFarParam`, fog far) from the table at `0x29EA10`. On the render side, `UpdateSceneLightsAndFog`
(`0x225790`) uploads the blended matrices and fog/sky values each frame, with `vifUploadSceneParams`
(`0x226480`, weights+fog → VU1 `0x13`), `vifUploadLightMatrix` (`0x226868`, → VU1 `0x14`) and
`vifUploadLightAt16` (`0x226940`, → VU1 `0x10`) doing the VIF uploads.

## Key functions

### Sound RPC (cluster `0x256AF8`–`0x2590B0`)

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x256AF8` | InitSoundRpcDriver | Boot: bind RPC to IOP server `0x1234567`, cmds 0/5/4 config, DGA/DMA2 setup, register BGM/SE files (cmds 6/7/`0x32`) | `0x2003B8` | `0x2575E0`, `0x279290`, `0x2793D0`, `0x287CD8`, `0x287EA8` |
| `0x287CD8` | sceSifBindRpc | Generic SIF RPC bind (semaphore-wait mode 0) | `0x256AF8`, `0x257760`, CD/pad/FS init fns | `sceSifSendCmd`, `CreateSema`/`WaitSema` |
| `0x287EA8` | sceSifCallRpc | Generic SIF RPC call; D-cache writeback + blocking wait (mode 0) | all sound wrappers (113 callers) | `sceSifSendCmd`, `sceSifWriteBackDCache`, semaphores |
| `0x257F30` | MusicRpcPlayBgm | Cmd `0xE` with two params `(slot, id)`; backend for all BGM wrappers | `0x258EC8`, `0x258F30`–`0x258FD0` | `0x287EA8` |
| `0x257FD8` | MusicRpcCmd15 | Cmd `0xF` (stop/reset BGM; param 1 = hard stop) | `0x258EB0`, `0x258EC8`, `0x258FF0` | `0x287EA8` |
| `0x258EC8` | gs_music_play_bgm | Full BGM start: cmd `0xF`(0) → cmd `0xE`(0,id) → cmd `7` file select from `gs_music_file_table` | 28 scene/minigame tasks | `0x257FD8`, `0x257F30`, `0x257A28` |
| `0x257A28` | SendMusicPlayRpc | Cmd `7`: sends a music file record (name + 2 words) with slot/param | `0x258EC8` | `0x287EA8` |
| `0x258F30`–`0x258FD0` | PlayBgmSlot1…6 | `MusicRpcPlayBgm(slot, 0)` wrappers (scene BGM slots) | scene/minigame tasks | `0x257F30` |
| `0x258EB0` | BgmStop1 | `MusicRpcCmd15(1)` | `0x262678` | `0x257FD8` |
| `0x258E50` | PlayUiSound | Ubiquitous UI beep wrapper: `id \| 0x40400000` → `0x258308` | 125+ callers | `0x258308` |
| `0x258308` | SndRpcPlayOrStop | Masked id 0 → cmd `1` (stop all); else cmd `0xD` play with id payload | `0x258E50` | `0x287EA8` |
| `0x2575E0` | SoundRpcRegisterFiles | Cmd `0x32`: register two file names at slots `i*2`, `i*2+1` | `0x256AF8` | `0x287EA8` |
| `0x2574A0` | SoundRpcPlaySequence | Cmd `0x23` + cmd `0x22` (load/start sequence) | `0x218458` | `0x287EA8` |
| `0x257BA8` | SoundRpcGetStatus | Cmd `8` query; returns first dword of `0x175F880` | `0x208F50` | `0x287EA8` |
| `0x258578` | SoundInitVolumes | Reset sound, set channels 0/1 to `0x1CCC` (cmd `0x21`) | `0x205088`, `0x2052D0` | `0x2585B0`, `0x2585F0` |
| `0x2585B0` / `0x2585F0` | SoundRpcStopAll / SoundRpcSetVolume | Cmd `0x20` stop-all; cmd `0x21` channel volume | `0x258578` | `0x287EA8` |
| `0x258810` / `0x2588B8` | SoundRpcCmd25 / SoundRpcCmd26 | Cmd `0x25` open voice; cmd `0x26` fade/stop voice (used in 0/1 pairs) | `0x258FF0`, `0x259050` | `0x287EA8` |
| `0x258FF0` | SoundStopAllChannels | Teardown: cmd `0x25`(0/1), BGM cmd `0xF`(1), clear handle slots `0x335728` | scene exits | `0x258810`, `0x257FD8` |
| `0x259050` | SoundFadeOutChannels | Cmd `0x26` for channels 0 and 1 | `0x20F3B8`, `0x2105B8`, … | `0x2588B8` |
| `0x259070` | SoundStopVoiceByHandle | Stops voice by handle in `0x335728[slot]` (cmd `9`) | `0x217C68` | `0x257C68` |
| `0x2589A0` / `0x258A48` / `0x258AF0` / `0x258B30` | SoundRpcCmd31 / Cmd33PlaySe / Cmd34 / Cmd35 | Master volume (cmd `0x31`), play SE by index (cmd `0x33`), commit (cmd `0x34`), reset (cmd `0x35`) | `0x2137B0`, `0x2590B0`, `0x2105B8` | `0x287EA8` |
| `0x2590B0` | ApplySoundVolumes | Scaled master volume from amp `0x1780A3E`, optional SE `0x1780A3D` + commit | `0x210430`, `0x2105B8`, `0x2137B0` | `0x2589A0`, `0x258A48`, `0x258AF0` |
| `0x258080` / `0x258138` / `0x258D50` | SndRpcQueryCmd10 / SndRpcReadBlock1F / SndRpcReadStatBlock | Readbacks into `0x175F880` (0x40 / 0x40 / 0x300 bytes) | — | `0x287EA8` |

Remaining one-shot wrappers in the cluster (all `0x287EA8`): `0x257760` bind+init call,
`0x2577C0` cmd `1`, `0x257800`/`0x2578A8` cmds `4`/`5` (dialogue window/cursor addresses),
`0x257950` string-arg call, `0x257C68`/`0x257D18` cmd `9`, `0x257DD0` cmd `0xA`, `0x257E80` cmd
`0xB`, `0x258218`/`0x258BF0` cmd `0xC`, `0x258CA8` cmd `0xD`, `0x2586A0` cmd `0x22`, `0x258768`
cmd `0x24`, `0x258960`/`0x258B70`/`0x258BB0` cmds `0x30`/`0x36`/`0x37`, `0x2583F0` DGA volume
registers.

### Day/night & lighting

| Address | Name | Role | Callers | Callees |
|---|---|---|---|---|
| `0x225988` | updateTimeOfDayLight | Clock → day/dusk/night weights, fog far/colour, blended light matrices | `0x20F3B8`, `0x2105B8`, `0x26CA48` | `0x272988` (vec scale), `0x272940` (vec add) |
| `0x26CA48` | ApplyTimeOfDayLighting | Lighting task: sets clock from `DAT_002DCD28[step]`, spawns town setup + fade, per-frame `updateTimeOfDayLight` | `0x26CB60`, `0x26CCD0`, `0x26D1C0`, `0x26D2C0` | `0x210158`, `0x210430`, `0x204F58`, `0x21C6A0`, `0x225988`, `0x204C48` |
| `0x26D1C0` | AdvanceDayCycleStep | Step machine: waits for `DAT_002DCD50[step]`, kills/respawns `0x26CA48`, ends after step 9 | `0x26CB60`, `0x26D2C0` | `0x204DE8`, `0x204C48` |
| `0x26CB60` | StartAreaIntroDayCycle | Area `0xA` script: lighting task + BGM `0x0a1c0201`; tick 780 event, tick 844 restart (BGM `0x0a1a0201`) | `0x22AEB0`, `0x26CCD0` | `0x258F30`, `0x258FB0`, `0x258EC8`, `0x204C48` |
| `0x26D2C0` | StartDayCycleStepTask | Area `0xB` entry: BGM `0x0a1a0201`, lighting task at step 1, hands off to `0x26D1C0` | `0x22AEB0`, `0x26CCD0` | `0x258F30`, `0x258EC8`, `0x204C48` |
| `0x2114E0` | UpdateTimeOfDayCounter | Increments world `+0x14` clock, wraps past `0x34BC0`; bumps `0x335730` when flag `0x1000` set | `0x2156D0` | — |
| `0x225790` | UpdateSceneLightsAndFog | Per-scene fog/sky + light matrix upload from the tod blends | `0x208848`, `0x20F950`, `0x210430`, `0x2105B8`, `0x22AEB0`, `0x22BFD0`, `0x22C280`, `0x22CD38`, `0x246250`, `0x2455C0`, `0x216738` | `0x272E08`, `0x2729E0` |
| `0x226248` | setFixedLight | Fixed lighting preset from table `0x29EA10` (race mode, bypasses clock) | `0x20F3B8` | — |
| `0x2261A0` | updateLightDirection | Light direction relative to camera (block `+0x180`) | render scenes | — |
| `0x226480` / `0x226868` / `0x226940` | vifUploadSceneParams / vifUploadLightMatrix / vifUploadLightAt16 | Upload sceneParams → VU1 `0x13`, light matrix → VU1 `0x14`, light at `0x10` | render path | — |
| `0x2262C8` / `0x2262D8` | GetSceneFogParamsPtr / SetDrawSkyColours | Fog param pointer (`0x177F730`); sky-dome colour from `0x177F754` | `0x22AEB0`, `0x225608` | — |
| `0x20F3B8` / `0x2105B8` | UpdateRaceGameTask / UpdateTownExplorationTask | Per-frame modes that drive `updateTimeOfDayLight` (race also `setFixedLight`) | mode handlers | `0x225988`, `0x226248`, `0x225790` |

## Data structures / globals

### Sound

| Address | Meaning |
|---|---|
| `0x175E840` | `sceSifClientData` for the SNDMOD connection (bound to server id `0x1234567`) |
| `0x175E880` | Shared `0x40`-byte RPC request buffer (dword 0–1 = params, then payload/name) |
| `0x175F880` | RPC reply buffer for query cmds (`8`, `0x10`, `0x1F`, `0xFFF0` reads 0x40–0x300 bytes) |
| `0x335728` | Two voice-handle slots (cleared by `SoundStopAllChannels`, read by `SoundStopVoiceByHandle`) |
| `0x1780A3E` | Master amp value scaled into the cmd `0x31` master volume |
| `0x1780A3D` | Pending SE id played by `ApplySoundVolumes` [inferred] |
| `gs_music_file_table` | Music file records (12 bytes: name pointer + 2 words), indexed by BGM id bits 16–23 |
| `0x29AE20` / `0x29AE2C`, `0x29AE38` / `0x29AE44`, `0x29AE50` / `0x29AE5C` | Boot-time file-name pairs fed to `SoundRpcRegisterFiles` (slots 0/2/1) |
| `0x29ADF4`–`0x29AE7C` | Boot-time name-record words for cmds `6`/`7` (slots 0–3), incl. `PTR_DAT_0029AE08/14/68/74` |
| `0x335730` | Side counter incremented by `UpdateTimeOfDayCounter` when world flag `0x1000` set (consumed by timing/score code) [inferred] |

BGM id packing [inferred from `gs_music_play_bgm`]: bits 16–23 = `gs_music_file_table` index,
bits 8–15 = parameter sent with cmd `7`, low bits part of the id sent with cmd `0xE`
(e.g. `0x0a1c0201`, `0x0a1a0201`, `0x0a100201`).

### Day/night & lighting

| Address / field | Meaning |
|---|---|
| world `+0x14` | Clock: vblanks since midnight, wraps at 216,000 (`0x34BC0`) |
| world `+0x28` | Flags: `0x1000` enables the `0x335730` counter; `0x400` cleared / `0x10000` set at lighting-task start |
| world `+0x22`, `+0x54`, `+0x58`, `+0x5C` | Car-type byte (−1 triggers sector recompute), grid sector, position pair |
| `0x177F720` (`sceneParams`) | 3 qw to VU1 `0x13`: weights x=day (`0x177F720`), y=dusk (`0x177F724`), z=night (`0x177F728`), fog word `0x177F730` |
| `0x177F738` (`fogFarParam`) | Fog weight (1.0 night / 0.5 day) written from the clock |
| `0x177F73C` | Fog far distance (300 at night → 800 at day, minus the fog adjust arg) |
| `0x177F750` / `0x177F754` | Fog colour RGBA / sky colour RGBA (sky read by `SetDrawSkyColours`) |
| `0x177F620` / `0x177F660` (`lightMatrixBlend` / `lightMatrixBlend2`) | 4×4 qw weight-blends of the day/dusk/night preset rows |
| `0x29D500` / `0x29D540` / `0x29D580` | `todLightDay` / `todLightDusk` / `todLightNight` (4 qw each) |
| `0x29D5C0` / `0x29D600` / `0x29D640` | `todLight2Day` / `todLight2Dusk` / `todLight2Night` (second light set) |
| `0x29EA10` | Fixed-light preset table (16 B/entry: sky, fog colour, `fogFarParam`, fog far) — race mode |
| `0x332DA8`–`0x332DC8` | Per-phase weight scale/offset constants used by the clock ramps (values not yet read) |
| `0x2DCD28` | Day-cycle step start times (clock value per step) |
| `0x2DCD50` | Day-cycle step target ticks (short per step; `0x26D1C0` compares against them) |
| lighting task `+0x1C` / `+0x25` / `+0x28` | Step index arg / one-shot init flag / handle of spawned lighting task |
| stepper task `+8` / `+0x18` / `+0x27` | Tick counter / parent struct ptr / step byte (0…9) |

## Control flow

**Sound (boot):** `0x2003B8` → `InitSoundRpcDriver 0x256AF8` → `sceSifBindRpc 0x287CD8` (server
`0x1234567`) → cmd `0` handshake → cmds `5`×4, `4`×4 config → `0x2793D0` DGA/DMA2 packets →
`SoundRpcRegisterFiles 0x2575E0`×3 (cmd `0x32`) → cmds `6`/`7` file attach.

**Sound (per use):** scene/minigame task → `gs_music_play_bgm 0x258EC8` (or `PlayBgmSlotN` /
`PlayUiSound 0x258E50`) → thin opcode wrapper (`0x257F30`, `0x257FD8`, `0x258308`, …) → stage
`0x40` request at `0x175E880` → `sceSifCallRpc 0x287EA8` → `sceSifSendCmd` → IOP SNDMOD server.
Teardown path: `SoundStopAllChannels 0x258FF0` / `SoundFadeOutChannels 0x259050`. Options path:
`HandleAudioSettingsMenuInput 0x2137B0` → `ApplySoundVolumes 0x2590B0` → cmds `0x31`/`0x33`/`0x34`.

**Day/night (tick):** task `0x2156D0` → `UpdateTimeOfDayCounter 0x2114E0` advances world `+0x14`
(wrap `0x34BC0`). Per frame: `UpdateTownExplorationTask 0x2105B8` / `UpdateRaceGameTask 0x20F3B8`
(or lighting task `0x26CA48`) → `updateTimeOfDayLight 0x225988` → writes weights, fog, and the two
blended light matrices → `UpdateSceneLightsAndFog 0x225790` + VIF uploads (`0x226480`, `0x226868`,
`0x226940`) feed the renderer. Race mode calls `setFixedLight 0x226248` instead of the clock.

**Day/night (scripted steps):** area entry `0x22AEB0` → `DispatchAreaEntryTask 0x26CCD0` →
`0x26CB60` (area `0xA`) or `0x26D2C0` (area `0xB`) → spawn `ApplyTimeOfDayLighting 0x26CA48`
(step n: clock = `DAT_002DCD28[n]`, town setup, fade) → `AdvanceDayCycleStep 0x26D1C0` watches
`DAT_002DCD50[n]` (+`fadeInTask 0x2082D8` at target−0x40) → kill/respawn lighting task per step →
after step 9: flag parent `+0x24`, kill self.

## Open questions

- IOP-side SNDMOD semantics for most opcodes are unknown (the server is an IOP module, not in this
  ELF): only the EE-side request packing is documented. Cmd meanings marked above are `[inferred]`.
- The exact meaning and units of `updateTimeOfDayLight`'s first (fog-adjust) argument are unknown;
  the lighting task passes `DAT_0177ACE4` [inferred: a weather/scene fog offset; the ≤ −2.5 branch
  selects an alternate short-fog colour set].
- The rate at which `UpdateTimeOfDayCounter` runs (task `0x2156D0` tick rate) is unverified; the
  "one real hour per day" figure assumes 60 Hz [inferred].
- Constant values at `0x332DA8`–`0x332DC8` (phase ramp scales/offsets) have not been read out yet.
- Cmd `6` vs cmd `7` file-attach semantics at boot are not fully distinguished [inferred: paired
  BGM/SE container registration per slot].
- The `0x40`-byte request tail left from uninitialized stack in the music/SE one-shots is presumed
  ignored by the server [inferred].
- `0x335730`'s downstream consumer semantics [inferred], and whether `fadeInTask 0x2082D8` at
  step target −0x40 is a pure transition cue or also gates input [inferred].
