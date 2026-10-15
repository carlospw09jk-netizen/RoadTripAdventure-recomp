# Fully-Native Road Trip — Migration Design

**Status: DESIGN DIRECTION APPROVED (user, 2026-10-14).** Approved scope: a separate native target (SDL3/Vulkan/MoltenVK), offline asset conversion + save migration, and **no** shipped PS2Recomp/parallel-gs/EE/VU1/GS pipeline. This approves the *direction/scope* — **not** verified implementation (none exists yet) and **not** the still-open policies: which enhancements are retained (§12), parity corpus/tolerance/performance targets (§13 G5/G6), and offline-conversion delivery location (§16). No implementation started. Author: cloud coordinator (DeepSeek, OpenCode Go). All claims re-verified against **current on-disk files** (branch `re/native-complete`, worktree `1ec8213` + uncommitted edits), not HEAD history or stale docs.

Companion: `build/scratch/decomp-audit/native-migration-task-matrix.md`. Evidence base: `build/scratch/decomp-audit/decomp-audit-report.md` + the coupling map in §4.

---

## 0. Definitions

- **Decompiled** = authored, compilable, maintainable source reproducing a piece of the game's behavior. A Ghidra pseudocode export (`build/decomp/fn/*.c`) is a reading aid, **not** decompiled source. A PS2Recomp translation (`generated/*.cpp`) is machine-code translation that still executes guest registers/RAM, **not** authored source.
- **Native** = authored C/C++ on host data structures + authored GPU calls consuming the game's *assets* directly. **Excludes** a GS-compatible backend fed by recompiled EE code through GIF/VIF1 packets and VU1 microprogram semantics.
- **Standalone** = the shipped app contains only authored code and ordinary installed platform SDKs. **Library/API policy is RESOLVED (user, 2026-10-14):** Vulkan, MoltenVK, and SDL3 are explicitly permitted host dependencies; the prohibition is scoped to **PS2-recompilation-specific dependencies and guest runtime pipelines** — no PS2Recomp, no parallel-gs, no EE/VU1/GS execution, and no disguised vendoring thereof. Generic codec/upscaler libraries are not inherently PS2-specific: their role is audited and retention proposed where they carry no guest semantics (§10), not blanket-blocked. This resolves nothing about downloading/installing dependencies now — the current work is documents only.
- **Authored** = written by us, checked into `src/`, maintainable, not machine-generated and not copied verbatim from generated/disc-derived instruction streams.

## 1. Verified current architecture (file:line, on-disk)

| Layer | What it is | Where (on-disk) | Depends on |
|---|---|---|---|
| EE gameplay | instruction-level static recompile run against `uint8_t* rdram`, `R5900Context* ctx`, `PS2Runtime*` | `generated/*.cpp` (derived), produced at first launch by `src/game/GameBuilder.cpp:293-296` (`ps2recomp::PS2Recompiler::recompile()`) | `third_party/PS2Recomp` (`ps2xRecomp`, `ps2xAnalyzer`) |
| VU1 | recompiled (`ps2_vu1_recomp --split`, `GameBuilder.cpp:306`), HLE'd (`vu1hle::run`), or native-strip | `third_party/PS2Recomp/ps2xRuntime/src/lib/vu/` | PS2Recomp |
| GS | paraLLEl-GS (default "Accurate"), CPU GS, and authored hardware GS (`gs_hw_backend.cpp`) — all GS-compatible backends consuming register/GIF/VIF1 state | `third_party/PS2Recomp/ps2xRuntime/src/lib/gs/` | parallel-gs (submodule), PS2Recomp |
| IOP | emulated; SNDMOD etc. via native stand-ins; `procDriver` (0x74C8) still interpreted | `third_party/PS2Recomp/ps2xIOP/`, `src/lib/ps2_iop_host.cpp`; `CLAUDE.md:96` | PS2Recomp |
| Authored app shell | app entry, settings, UI, platform, textures, authored gameplay hooks (see §4; no count claimed — hooks are not a completeness basis) | `src/` (§4 coupling map) | PS2Recomp runtime |

Native-render state (on-disk): the **strip programs (entries 2–6)** have a native draw path on the hardware GS; **entries 0/1/7 are setup programs** (`docs/vu1-microcode.md:14-26`), not strips. Native 2D (`gs_native_2d.{h,cpp}`, `docs/native-2d-draws.md`) interprets 2D register packets but still looks up textures "from the GS-memory image through tex0" (`docs/native-2d-draws.md:27-29`) and forwards Context-2 draws/uploads as verbatim GIF (`:42-51`). **None of this removes the EE/VU1/GS pipeline** — every "native" path is driven by recompiled EE code emitting VIF1/GIF.

## 2. Acceptance criteria

1. Standalone, faithful Road Trip on macOS (Apple Silicon) and AYN Thor (Android), built only from the user's disc (no game data shipped).
2. **100% authored/native CPU gameplay**: authored C++ game loop + physics/AI/world/menu logic on host data structures; **no EE instruction-level recompile, no guest register/RAM execution, no function-address dispatch**.
3. **Direct host GPU rendering** of game assets; **no GS register/GIF/DMA/VIF1 packet emulation, no VU1 interpretation/recompilation/HLE, no GS-compatible backend, no VRAM/GS-local-memory/swizzle/tex0 execution** in the shipped app.
4. **No PS2Recomp, no parallel-gs, no EE/VU1/GS execution, and no disguised vendoring thereof.** Vulkan/MoltenVK/SDL3 are permitted host libraries; generic codec/upscaler libraries are audited for guest semantics, not blanket-blocked (§0, §17).
5. Visual and gameplay behavior preserved (parity), defined in §12.

## 3. What "completeness" is and is not

- Function/pseudocode counts and sampled pixel-exactness are **not** a gameplay-source denominator. The machine-checked M0.1 figures are **1387 generated starts / 1379 seeds / 1542 exports / 155 no-generated-file** — not "1379 recompiled functions" and not any known game-logic count. The game/library/infrastructure split is **not yet classified** (M0.1, §18); gameplay scope remains unknown.
- A pseudocode export is not compilable authored source; a generated MIPS translation is not native gameplay.

## 4. Per-file coupling (corrected — grep absence is not purity)

Coupling is assessed by source inspection of includes + direct uses of guest/runtime symbols (`runtime/*` headers, `ps2_*`, `ps2x::`, `gs_*`, `PS2Runtime`, `ps2_test`, `ps2_save_state`, `ps2AudioOut*`, `PgsControl`, `ps2_gs_memory`, `Ps2GuestCall`, `GPR_*`, `rdram`) **plus transitive dependencies**. "pure?" below means *not flagged by this inspection* — it is **not** a claim of clean reuse; each still needs an audit before it is treated as reusable.

**Directly guest/runtime-coupled (confirmed on-disk):**
- Gameplay: `game/Driving.{h,cpp}` (`playerControl` reads/writes `rdram`, `ctx`; `Driving.cpp:213-262`), `GameOptions.*`, `GameStats.*` (`readStats(runtime)`), `overrides.cpp`, `game_shim.cpp`, `GameBuilder.cpp`, `sdk_probe.cpp` (`#include "ps2_runtime.h"`).
- UI: `ui/PauseMenu.cpp`, `ui/MapRouter.*`, `ui/MapView.*`, `ui/SecondScreen.*` (`PS2Runtime &`, `runtime.presenter()` at `SecondScreen.cpp:323,404,519,558,656,661`), `ui/ButtonGlyphs.cpp` (`runtime/gs/gs_pgs_backend.h`, `ps2x::gs::PgsControl` at `:8,22,164`).
- Platform: `platform/Host.{h,cpp}`, `platform/Lifecycle.cpp` (`runtime/ps2_audio_suspend.h`, `runtime/ps2_test_harness.h`; `ps2_test::paused()/setPaused`, `ps2AudioOutSuspend` at `:6-7,28-43`), `platform/input/Input.cpp`.
- Settings: `settings/Apply.{h,cpp}` (presenter-driven).
- States: `states/StateSlots.*` (`ps2_save_state`, `.rtstate`).
- Textures: `textures/Extract.cpp` (`runtime/gs/ps2_gs_memory.h` at `:4`), `textures/PackPrep.cpp` (`runtime/gs/gs_texture_pack_cache.h`; `ps2x::gs::packcache` at `:3,19,47`).
- Debug: `debug/{FpsOverlay,PerfStats,RamDump,ThreadDump}.*`.
- Entry: `main.cpp`.

**Transitively coupled (revealed by include closure):** `ui/PerfOverlay.cpp` (`debug/PerfStats.h`), `ui/Theme.cpp` (`platform/Host.h`), `debug/FrameDump.cpp` (`platform/Host.h`), `platform/input/Controllers.cpp` (`platform/Host.h`), `ui/MapRoute.cpp` (`ui/MapRouter.h` → coupled `MapRouter`).

**Verified standalone (full include closure checked; only these are claimed reusable today):** `game/DriveFeel.{h,cpp}` (std only), `rom/{IsoReader,RomInstaller,Sha1}.{h,cpp}` (libchdr/SDL3 host libs only), `settings/{Settings,Capabilities}.{h,cpp}` (toml11 + `platform/Paths.h`), `platform/{Input.h,ProgressEta.h,TaskProgress.h}` (std only). Headers-only files whose implementation is coupled (`PauseMenu.h`, `PerfOverlay.h`, `ButtonGlyphs.h`, `Lifecycle.h`, `Controllers.h`, `Extract.h`, `PackPrep.h`) are **not** reusable.

**All other files: UNKNOWN pending full include/dependency closure** — not claimed reusable; each needs a closure audit before any extraction work.

**Extraction work:** every coupled file gets a per-file extraction task (document its guest dependency → define a host interface → port the logic). Pure files still need the surrounding shell re-plumbed to the authored core.

## 5. Route comparison (corrected — no hook-count rationale)

Both routes require the same authored gameplay (that is the work regardless). They differ in **continuity, reversibility, and per-slice evidence gating**:

| | (A) In-place subsystem/data-boundary migration | (B) Separate native application + offline asset extraction |
|---|---|---|
| Continuity | Higher initially: reuses shell/runtime scaffolding; evidence captured from the live system each step | Lower initially: new tree, asset pipeline first |
| Reversibility | Poor: each step deepens the hybrid; the runtime stays until the last step | High: independent; oracle preserved; pausable per slice |
| Per-slice gating | Hard: a partially-migrated hybrid still links PS2Recomp+parallel-gs and keeps guest coupling alive, so proving a slice is guest-free is difficult | Provable by construction: each slice is guest-free or it fails to link/run |

**Recommendation: (B), but (A) remains viable/deferred — not logically impossible.** Both routes are re-decidable. **Checkpoint/kill conditions** (after M1, then per milestone): if authored rendering to parity stalls against the oracle for three consecutive milestones, or if the M0.1 classification shows the gameplay-derivation set is untenable, re-open (A) with a *data-boundary* migration plan (replace whole EE subsystems and the guest-RAM→host-struct boundary, not individual function hooks). Route choice is not a one-way door.

## 6. Target architecture (host-native)

- **Data model:** authored C++ structs for cars, world (FLD/COURSE/ACTION), tasks/menus, notebook/progress. `config/game_state.toml` RAM offsets become struct fields; no `rdram` byte array.
- **Game loop:** authored fixed/decoupled tick; vblank cadence expressed as host timing, not guest cycle accounting.
- **Render boundary:** the loop produces authored draw lists (models + authored camera/object transforms + fog + time-of-day colour weights + materials + HUD). VU1 microprogram math (`docs/vu1-microcode.md`; reference `third_party/PS2Recomp/ps2xRuntime/src/lib/gs/hw/native.vert`) is ported **once** into authored shaders, not kept as a microprogram interpreter. No GIF/VIF1/GS-register stream crosses the boundary.
- **Asset pipeline:** disc files → **offline converter** → authored **versioned `HostAssetContainer`** (typed image/mesh records with version + type tags) → authored loader → host resources. The shipped loaders accept **only** `HostAssetContainer` records (no disc/PS2 byte inputs, no `Tex0`/swizzle/palette/GIF/GS types); the converter decodes PS2 packed upload chains/palettes/swizzles offline and is **never linked into or included by the shipped runtime** (§10, G2). `tools/model_dump`/`texture_extract` are offline prototypes only.

## 7. Subsystem matrix

| Subsystem | Final owner | Grounding (on-disk) | Status today |
|---|---|---|---|
| Car physics/feel | authored | `DriveFeel.cpp` (pure seed); `Driving.cpp` (coupled, port) | partial pure seed |
| AI / traffic / race logic | authored (derive) | `game_symbols.toml` fn notes; `build/decomp/fn/*.c` (reading aid) | unstarted |
| World/field/course streaming + libcdvd | authored loader | `docs/native-render.md` FLD/COURSE/ACTION; `rom/IsoReader` | disc reader exists; streaming logic unstarted |
| Menus/tasks/options | authored | `overrides.cpp` menu hooks; `game_state.toml [options]` | coupled hooks only |
| Input + libpad | authored | `platform/input/*` (audit pending) | mostly candidate-pure |
| Save + libmc | authored | `game_state.toml:1-6` (original save layout) | **§8 — migration in scope** |
| Audio (VAG + TSQ/TVB + SNDMOD) | authored | `audio_extract.cpp` (VAG only); `CLAUDE.md:67,96` | **§9 — sequenced music unresolved** |
| Disc/assets | authored | `rom/*` | candidate-pure |
| Rendering (3D+2D+textures) | authored | `native.vert`, `docs/native-render.md`, `native-textures.md` | reference only (§10) |
| VU0 | **authored matrix/math**, not emulated | EE FPU/macro notes; `ps2_ee_fpu_mode.h` (reference) | folded into EE today; must be re-derived as authored math |
| IOP | **eliminated only after authored replacement** | `ps2xIOP` (reference); `procDriver` interpreted (`CLAUDE.md:96`) | §9 audio + §7 IO |
| VU1 | **eliminated** (math ported to authored shaders/CPU) | `docs/vu1-microcode.md` | reference only |
| GS | **eliminated** (authored renderer) | reference backends | reference only |

**Guest-coupled libraries NOT presumed native:** libcdvd, libmc, libpad, SIF/RPC, libdma/libgraph each need an authored replacement + evidence gate. Do not label "IOP/VU0 eliminated" before an authored replacement exists and passes its gate.

## 8. Save handling (three distinct things; migration feature IN SCOPE)

1. **Original save — parse/migrate (in scope).** Memory-card file `BASLUS-20398/BASLUS-20398` (**13,384 bytes**), written verbatim from the EE RAM progress block; `config/game_state.toml:1-6` documents that the same offsets read a save file or live RAM. **Direction is an assumed proposal (original→native import), not yet user-approved beyond "migration feature is fine".** The native app must parse + validate (schema/version/ROM identity) and migrate to the native save, reproducing every mapped notebook/progress field. **Retain original inputs untouched**; report unsupported fields.
2. **`.rtstate` — offline migration input only.** PS2Recomp guest snapshots (`src/states/StateSlots.cpp`, `runtime/ps2_save_state.h`) export **verified native persistent progress** (progress/inventory/notebook/settings) into the native save — never a shipped EE/VU1/GS runtime, never guest-snapshot restore.
3. **No arbitrary mid-frame snapshot equivalence.** Inventory persistent vs transient (physics/render/scheduler) state; restore only at a **documented safe native checkpoint**.

**Explicitly undecided (not required silently):** native→original **export** (writing back to a PS2 memory-card file) is **not** assumed. Host save/load **timings** are **blocked** (macOS live tests prohibited); Thor verification is subject to the separately scheduled oracle approval (§11). Exact `.rtstate` fidelity and safe-checkpoint details are M0.4 evidence tasks, not approved implementation specifics.

## 9. Audio (sequenced music is in-scope, not optional)

- `tools/audio_extract.cpp` decodes **VAG streams only**; "the sequenced music (TSQ sequences, TVB sample banks) is not decoded here" (`audio_extract.cpp:5`).
- Sequenced music/effects are **original content**, therefore in-scope. Required RE: TSQ sequence format, TVB sample-bank format, SNDMOD sequencer/mixer/reverb, and the IOP `procDriver` tick (still interpreted, `CLAUDE.md:96`).
- Add an authored **sequencer/mixer/reverb** replacement + an **audio-parity gate** (behavioral). No "IOP eliminated" claim until this lands.

## 10. Textures (offline converter vs shipped loader — separated)

- `tools/texture_extract.cpp` replays each file's GS upload packets (BITBLTBUF..TRXDIR + IMAGE) into a GS-memory image via PS2Recomp's `ps2_gs_memory.h` (`CLAUDE.md:68`). That is an **offline, oracle-assisted decode** — development reference, **not** a shipped loader.
- `docs/native-render.md:160-161`: FLD/COURSE/ACTION/CAR containers **embed the textures' GS upload chain** (CNT tags to END). So even model files carry GS packet data that must be decoded **offline**, never at runtime.
- **Shipped runtime:** an explicit **offline-converted, versioned host asset format** (with recorded provenance) decoded by an **authored runtime loader**. The final runtime contains **no GS-local-memory, swizzle, or tex0 execution**; PS2 packed/GS uploads, palettes, and swizzles are decoded **only** in the offline converter.
- Track without assumption: palettes (banks/CSA), palette animations/recolours, mip chains, uploads/transfers, per-asset coverage; the edge cases `docs/native-textures.md` lists (render-target reads, blend gaps DTHE/COLCLAMP/PABE/FBA/FBMSK).
- **Codec/upscaler libraries (ASTC/BC7 encoders, FSR/GSR/ASR/DLSS/XeSS) are not inherently PS2-specific.** Each is audited for guest semantics: where a library only compresses/decompresses textures or scales frames with no PS2 execution semantics, retention is proposed; any that encode PS2-specific behavior are removed. This is a per-library audit task, not a blanket block, and authorizes no downloads now.

## 11. Bootstrap + oracle identity + oracle schedule (explicit)

- The current app is the **behavioral/visual oracle only**. Identity pinned by **commit + dirty diff + artifact hashes** (not commit alone).
- **No automatic commits or deletions.** A **read-only preservation manifest** precedes any dependency-removal work, generated from `git status --porcelain` (top level + every nested submodule), full `git diff`, source checksums, and identity pins — covering **all** tracked/untracked changes and dirty nested submodules, with no hardcoded/incomplete enumeration. An explicit authorization gate precedes any delete/archive; the dirty submodule is archived/pinned by commit + diff + checksum.
- **Oracle schedule (no implicit authority):** reading existing recordings/artifacts is read-only authorized. **No new macOS oracle/game/test runs are authorized at all.** New Thor oracle runs go **only through the dedicated dev owner** (`io.github.silentsudin.roadtrip.dev`, `97a75c3e`) under specifically scheduled approval; never the release app; never touch power settings. Any missing trace/recording is **BLOCKED** until approved.

## 12. Parity baseline (original vs enhancements)

- **Original parity**: the PS2 game's gameplay/content/audio semantics — the migration baseline.
- **Enhancements**: widescreen, HD packs, analog triggers, 120/240 Hz generation, second screen — added by this project, **not** automatically mandated. **User decision required** on which are retained/required vs dropped; do not drop silently and do not declare all implicitly required.
- **Original bugs vs selected fixes** distinguished; parity means "matches PS2 for original semantics", with deliberate divergence listed.

## 13. Completion gates (`nm sub_00` is only a smoke test)

- **G1 repo elimination:** no `third_party/PS2Recomp`, no `parallel-gs`, no submodule/FetchContent. Verified by dependency graph + compile/link manifests + artifact dependency inspection — not a symbol grep.
- **G2 no guest execution:** source + data-flow inspection proving no `R5900Context`/`rdram`/`Ps2GuestCall`/function-address dispatch/COP0/EE-FPU mode/VIF1/GIF/VU-selector/GS-register/VRAM references reach the shipped app. The final runtime contains **no PS2 swizzle/palette/GIF/GS decode code at all** (that lives only in the offline converter). **Compile/link separation proof**: the converter is a separate tool target with no header/include edge into shipped loaders and is never a link dependency of the runtime. `nm … | grep -c sub_00 = 0` is a smoke test, **not proof**.
- **G3 asset coverage:** every disc asset the game loads is read by an authored loader (per-asset coverage; runtime use proven from retained traces, not a static file list).
- **G4 behavior parity**, **G5 visual parity**, **audio parity** (§9): against the pinned oracle, frozen corpus + goldens, original parity (§12).
- **G6 performance:** 60 **native** game fps sustained (distinct from 120/240 **generated shown** fps). Platform settings, warmup, thermal state, and sample duration are **defined and approved**, not invented. macOS live tests remain **PROHIBITED**; G6 evidence is blocked until approved.
- **G7 reproducible cold build:** a clean build that never fetches a banned repo (PS2Recomp/parallel-gs or any guest-pipeline code), run **only when separately authorized**.

## 14. Estimates

Withdrawn. No week/month/team-size figures are asserted. Timeline and effort are **unknown until M0 inventories** (function classification, asset coverage, audio-format RE) are complete.

## 15. VU1 scope (corrected)

- Eight programs: **entries 2–6 are strip programs** (2 `LitStrip`, 3 `ObjectStrip`, 4 `WorldStrip`, 5 `OffsetStrip`, 6 `SkyStrip`); **entries 0 (`Init`), 1 (`SetupLit`), 7 (`SetupClipped`) are setup programs** (`docs/vu1-microcode.md:14-26`).
- Current authored "native draw" covers the strip programs on the hardware-GS path, **but**: records off by default; Android native off. Precise current routing (verified on-disk, not the stale `docs/NATIVE_RENDER_STATE.md`):
  - **Clipped polygon fans** (`NativeDraw::fans`, programs 3/4/6) go through the ordinary packet path (`docs/native-render.md:392,406`).
  - **2D (menus/HUD)** is PATH2/PATH3 packets, not model calls (`docs/native-render.md:234`).
  - **Native-strip HUD placement** uses the CPU classifier (`nativeReference`) only for HUD draws (minimap/course map/markers); 3D never takes that CPU path (`docs/native-render.md:343-352`).
  - "106M vs 149M" strip counts are **not** contemporary completeness evidence.
- Final app: VU1 eliminated as an execution unit; setups 0/1/7 (camera/object/light/fog/time-of-day) become authored host code feeding authored shaders.

## 16. Milestones (policy decisions mapped to the milestones they block)

- **M0 — Evidence (read-only, cloud only; plannable now):** M0.1 function classification; M0.2 **static potential** asset inventory (runtime use unknown until a retained trace proves it); **M0.3a** static inventory of retained goldens/corpus + original-vs-enhancement split; M0.4 save + audio format RE. **M0 proceeds with existing on-disk artifacts; no new oracle runs, no Mac compile/run.** No local model.
- **M1 — Geometry/texture deterministic fixture (first slice).** Authored **offline converter** → authored host asset format; authored loader + direct-GPU draw (**Vulkan/MoltenVK**) of one pinned model/atlas verified against **independently specified authored expected buffers** (geometry/texture checks), not an oracle frame golden. API scope is resolved (§17); rendering-independent loader logic still needs **design approval** before implementation.
- **M2 — RE prerequisites for scene parity:** authored camera/object transforms, fog, time-of-day colour weights (setup programs 0/1/7) with evidence.
- **M3 — Authored render loop to scene parity** (depends M2 + M0.3). **Blocked on the enhancements decision (§12)** for which visuals are in/out of scope.
- **M4 — Authored gameplay vertical** (car physics from `DriveFeel`, then AI/world/tasks; depends M0.1).
- **M5 — Subsystem coverage** (audio §9, save §8, input, menus, disc/libcdvd, libmc, libpad).
- **M6 — Dependency deletion + gates G1–G7** (gated on §11 manifest + authorization).

**First slice = M1 fixture.** **Conversion execution location/delivery is an unresolved milestone decision** (on-device first-launch vs authoring pipeline vs both). It **blocks M6 shipping only, not the M1 fixture**; no claim that first-launch conversion is acceptable, and **no bundled/distributed game assets**. Owned disc inputs stay local to the user; the device standalone runtime is kept separate from any authoring pipeline.

## 17. Unresolved decisions (milestone-gated, minimal)

**Library/API policy — RESOLVED (user):** Vulkan + MoltenVK + SDL3 are permitted host dependencies. The "no external repo pulls" prohibition is scoped to **PS2-recompilation-specific dependencies and guest runtime pipelines** — no PS2Recomp/parallel-gs/EE-VU1-GS execution or disguised vendoring. Primary GPU path is **Vulkan (MoltenVK on macOS)** with an **SDL3 host shell** (matching the existing renderer style); no Metal-vs-Vulkan or SDL3 question remains. Generic codec/upscaler libraries are audited for guest semantics and retained where none (§10); that is an audit task, not a blanket allowance, and it authorizes no downloads now.

Remaining decisions (only where they gate a milestone):

- **BLOCKS M3 (visuals/enhancements):** which enhancements (§12) are retained/required.
- **BLOCKS G5/G6 + M0.3b (tolerance/performance + corpus freezing):** tolerance policy, performance targets, and platform-specific render/run authorization. **No macOS run/compile is authorized now and none is requested of the user now**; these are blocked until a future platform-specific approval. Thor-only runtime acceptance goes through the dedicated dev owner under a later scheduled approval.
- **M1 conversion delivery:** offline-conversion location (§16) — a milestone decision, not a user question here.

Builder defaults (overridable by reviewer/user): `src/native/` subtree and `rt-native` target **name**; authored versioned host asset container with recorded provenance; route B with §5 checkpoint/kill conditions.

## 18. Evidence-collection tasks (precede any completeness claim)

- M0.1 function classification (game vs SDK/newlib vs infra) — the missing gameplay denominator.
- M0.2 **static potential** asset inventory (per-asset; runtime use must be proven from retained traces, not assumed).
- M0.3a static inventory of retained goldens/corpus + original-vs-enhancement split (§12) — plannable now.
- M0.3b corpus freezing + tolerance policy — **BLOCKED** on G5/G6 approval (§13).
- M0.4 audio-format RE (TSQ/TVB/SNDMOD, `procDriver`) + save-format RE (`BASLUS-20398` schema/version/ROM-identity, `.rtstate` persistent-vs-transient inventory, safe native checkpoint) — feasibility/evidence only.

**Do not invent function/asset semantics** in any future task; a pseudocode export is a reading aid and a generated MIPS translation is not native gameplay.
