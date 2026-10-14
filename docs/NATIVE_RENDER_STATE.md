# Native renderer: where things stand (2026-10-14)

Written when the overnight work stopped, because the weekly Claude usage was near its limit (86%). Nothing in this document is pushed or merged into `main`. Every branch named here is local; the fork (`third_party/PS2Recomp`) branches are local too.

Goal, from the user: stop emulating the VU1 and the GS for 3D (and later 2D). Decompile the game's renderers, draw its meshes, matrices and materials on the GPU, and keep the shipped app small by leaving the non-native renderers out of it ("no going back to old renders or non native paths"). Native must be accurate and much faster than the old path.

## 1. In one paragraph

All eight VU1 microprograms now have a native draw path on the hardware GS, verified pixel-exact against the old path (0 mismatches over about 106M live strips). Per-call records cut the VU1 worker from 23% to 3% of a core in town on the Mac. Total CPU on the Mac is now at or below the old path and the Thor's worker is 20 to 35% cheaper. It is not yet *much faster*: the GS thread is only level with the old path, and on the Thor with GSR 2 at 120 Hz frame generation still pauses (47 and 53 shown against 120 on the old path). So native with records is **off by default** and **Android native stays off**. The CPU side is better: readable generated code, 35 to 45% less game-thread CPU on the Thor, and PS2-exact float behaviour that is much closer to PCSX2. The goldens were regenerated for it.

## 2. Branch map (parent repo / fork submodule)

Merge order for the final result: `re/integration2`, then worker C's `re/native-cache3`, then `re/native-2d`. Those last two are not merged yet.

| Branch | Head | What it holds | State |
|---|---|---|---|
| `re/native-render` | 591f741 | Ghidra tooling (`scripts/ghidra/`), `config/game_symbols.toml`, `docs/native-render.md`, `tools/model_dump.cpp`, CLAUDE.md "use Ghidra and REA first" | base of everything |
| `re/vu1-hle` | 576c82e | `docs/vu1-microcode.md`, `vu1hle` reference C++ for the programs, `vu1_replay --hle` | done |
| `re/native-draw` | 8013650 | native draws for programs 2 and 5, `test_native_draw.py` | done |
| `re/native-clip` | 95468bb | clipped programs 3, 4 and 6 | done |
| `re/native-gen` | 72e85a5 | native draws with frame generation, motion, UI mask, widescreen, map layer | done |
| `re/native-merge` | 43f63ae | merge of the four above, one shared reference, ring of 32768, Gouraud fix, **native default on desktop, off on Android** | done |
| `re/cpu-codegen` | 4aacf50 | readable generated code (names from `game_symbols.toml`), inline cycle accounting, `pc` stored only where read | done, hashes identical to baseline |
| `re/cpu-accuracy-2` | d02d054 | EE FPU round toward zero, nearest div/sqrt/rsqrt, no FMA contraction, 64-bit branch tests, game's own libm, `tests/ee_semantics` | done, **goldens moved by design** |
| `re/integration` | 59ea667 | native-merge + cpu-codegen + cpu-accuracy-2, `RT_REFERENCE_RENDERERS` build option, **goldens regenerated** (71 tests) | done |
| `re/night-glow` | 9b963c1 | adventure-race bot fixes (opt-in `learn`), `docs/night-glow-bot.md` | done |
| `re/native-map` | 1bbe5ea | map classifier fix, named map code, `docs/native-map.md`, `test_map_audit.py` | done |
| `re/native-bugs` | c5ff7dc | #3, #31, #6, #20 fixes, River Raceway tolerance, `docs/native-bugs.md` | done |
| `re/native-enh` | 8d763fe | frame skip with native, queue clear on state load, native pipeline warm-up, `docs/native-enhancements.md` | done, **its new tests not confirmed** |
| **`re/integration2`** | **0e93c0f** (fork `integration2` 667b3aa) | night-glow + native-map + native-bugs + native-enh on top of integration | **the best combined state; this document is committed on it** |
| `re/native-records` | 08d56c4 (fork `native-records` 927a93e) | per-call records (`RT_NATIVE_RECORDS=1`), verify oracle, `docs/native-records.md` | done |
| `re/native-cache3` | c9ad5d1 (fork `native-cache2`) | worker C's GS-side work: runs, shared GPU objects, lean classifier; merges native-records and integration, **not integration2** | not merged into integration2 |
| `re/native-2d` | 4f2f0c2 | first native 2D draws, `docs/native-2d.md`, verify oracle | not merged |
| `re/native-2d-survey`, `re/native-tex-survey` | 42f1b74, 9f3a8f0 | surveys of the 2D draw paths and texture uploads | docs only |

The fork repo has matching local branches. Rule when pushing: push the fork's branch to `silentsudin/PS2Recomp` (remote `origin`) **before** the parent branch, because the parent's submodule pointer must name a pushed fork commit.

## 3. What is verified

- **Native draw exactness:** `vu1_replay --native` over all 9 captures: 0 mismatches. `RT_NATIVE_DRAW_VERIFY=1`: 0 mismatching runs over about 106M strips (a Quick Race plus four courses), no fallbacks, `ring_full` 0.
- **Records:** `RT_NATIVE_RECORDS_VERIFY=1`: 0 constant and 0 side-effect mismatches over about 5M strips in town and a similar number in the Quick Race and course 0. Pictures from records against per-strip native differ by 0.000% of pixels. The carousel was not measured.
- **Mac CPU, 60 Hz, hardware GS, busy % (town / race):**

  | | VU1 worker | GS thread | Process CPU |
  |---|---|---|---|
  | Per-strip native, at the start | 23 / 12 | 44.5 / 5.5 | 74-77 / 38-41 |
  | Native with records | 12 / 8 | 17 / 3 | 42 / 39 |
  | Old VU1 path | 21 / 14.5 | 16 / 7 | 51 / 43.5 |

- **Thor (dev app, 30 s medians; worker and GS in GHz):**

  | Scene | Config | Path | Game / shown | Worker | GS | GPU % |
  |---|---|---|---|---|---|---|
  | Fuji | 60 Hz | old | 60 / 60 | 1.25 | 0.83 | 44 |
  | Fuji | 60 Hz | records | 60 / 60 | 0.98 | 0.86 | 51 |
  | Fuji | 120 FSR 1 | old | 59.9 / 120 | 1.22 | 1.00 | 80 |
  | Fuji | 120 FSR 1 | records | 59.7 / 119 | 1.01 | 1.08 | 73 |
  | Fuji | 120 GSR 2 | old | 59.8 / 120 | 1.17 | 1.06 | 82 |
  | Fuji | 120 GSR 2 | records | 59.8 / 47 | 0.86 | 1.24 | 65 |
  | Peach | 60 Hz | old | 60 / 60 | 1.56 | 1.15 | 39 |
  | Peach | 60 Hz | records | 60 / 60 | 1.14 | 1.30 | 50 |
  | Peach | 120 FSR 1 | old | 59.9 / 120 | 1.58 | 1.32 | 82 |
  | Peach | 120 FSR 1 | records | 59.9 / 120 | 1.15 | 1.27 | 82 |
  | Peach | 120 GSR 2 | old | 60 / 120 | 1.53 | 1.32 | 85 |
  | Peach | 120 GSR 2 | records | 55 / 55 | 1.01 | 1.12 | 64 |

  After the lean-classifier change the GSR 2 120 Hz rows measured Fuji 59.8 / 47.5 and Peach 53.6 / 53. Simpleperf at GSR 2: GS thread `record()` 39% (`prepareNative` table fill 11.6% self, memcpy 9%), record expansion 40%, classifier 4.4%.
- **CPU code (Thor, GameThread GHz-eq, 60 Hz):** Fuji 0.62 / 0.58 to 0.34 / 0.33; Peach 0.37 to 0.24. Mac Quick Race 6000 vblanks, rendering off: wall 8.1 s to about 6.0 s, CPU 11.2 s to about 8.8 s. State hashes (`RT_TIME=virtual`) identical to baseline on codegen alone and on codegen + accuracy.
- **Accuracy against PCSX2 (attract demo, 24 cars, words differing out of 3744):** old build 221 at demo clock 53, accuracy build 246, accuracy + nearest div/sqrt 22; at clock 200: 579, 295, 64; at 400: 1129, 506, 134. Past clock 1200 everything is chaotic. 22 words at car+0x1BC (a fixed-point integer, off by one in the lowest bit) are unexplained. See `docs/pcsx2-accuracy-check.md` and `docs/recompiler-accuracy.md`.
- **Suites on `re/integration` after regenerating goldens (59ea667):** paraLLEl-GS 3 failed / 170 passed; hardware GS 4 failed / 169 passed. Remaining failures: `test_field_area_map_is_routed` (fixed on `re/native-map`), Night Glow A/B (bot, fixed on `re/night-glow`), and on hardware GS `2p_River_Raceway_20s` (tolerance widened on `re/native-bugs`).
- **Worker C's gate on `re/native-cache3` (dev tree, records on):** paraLLEl-GS 3 failed of 190, hardware GS 4 failed of 190 (Night Glow A/B, River Raceway, and `test_native_map_layer` at 6.03 against a limit of 6.0, borderline and flaky).
- **`re/integration2`:** build clean, `sub_00` 0, 9 of 9 unit tests pass; hardware GS targeted set 60 passed / 5 failed; paraLLEl-GS widescreen and save-state tests 10 of 10.
- **Size (Mac):** `RoadTrip` binary 14.8 MB with `RT_REFERENCE_RENDERERS=OFF` against 15.9 MB ON; bundle 25.7 against 26.7 MB. Only about 1 MB saved so far, because the CPU GS, the hardware GS and the presenter still link.

## 4. What is NOT verified, or known broken

1. **Five new tests fail on `re/integration2` under the hardware GS** (`tests/regression/test_native_enhancements.py`): `test_motion_on_every_program[quick_race|town|course_2|course_3]` report 0 draws carrying motion (0 of 6281 for program 0 in quick race; the counter is `motionDraws` in `gs_frontend.cpp`), and `test_frame_skip_with_native_draws` skips 0 frames. The enhancements worker marked them "to run" and never ran them. It is unknown whether the merge, the tests or the code is at fault, and unknown whether they also fail on `re/native-enh` alone. **If motion really does not flow for native draws, the upscalers' inputs (TAA, GSR 2, ASR, MetalFX) are wrong.** Check this first.
2. **GSR 2 at 120 Hz on the Thor** loses frame generation with native records. Records are off by default and Android native stays off until this holds 120 shown.
3. **The "flashing" on the Thor with native on** is inferred, not captured: the `[late]` log points at frame skip pausing generated frames (late frames of 25 to 30 ms). The present dumps captured only real frames, which look correct.
4. **#4 River Raceway decal is not fixed.** Depth precision and bias were ruled out; the decal draws are clipped-program fans on the packet path; the same difference shows with `RT_NATIVE_DRAW=0`. The software GS and the hardware GS agree with each other and differ from paraLLEl-GS, so it is a *suspected* paraLLEl-GS divergence. The test limit (worst 32 px block) was widened from 30% to 50% for that one frame. Which picture matches a real PS2 is unknown; it needs PCSX2 on that stage.
5. **#6 and #20** are changed but not measured: #6 (frame skip level 1 now needs 3 late frames of 30 and the first frame of a new scene is not counted late) and #20 (`failVulkan` shows a readable message with the GPU's Vulkan versions and features instead of crashing; never forced to fail). **#10** does not reproduce on the Mac (6 of 6 save-state tests pass); the report is from the Thor and needs `RT_STATE_HASH_FULL=1` there. **#2** is a write-up only.
6. **The map:** race maps (courses 0, 2, 4, 6), attract demo, Q's Factory and 16:9 placement were audited. Not exercised: 120 Hz generated frames, the second-screen map page, the highway map, and the map after a race or building (unit-tested only). The classifier fix: a loading screen's "2D" verdict could stay stuck on a 4:3 window, hiding the Enhanced map for the rest of the session.
7. **The `RT_NATIVE_RECORDS` default is off; records were not run against the full suites together with `re/integration2`.** `re/native-cache3` and `re/native-2d` are not merged into `re/integration2`.
8. **Never executed:** Windows and Android builds with `RT_REFERENCE_RENDERERS=OFF`; the pgs suite on an OFF build (it cannot run; dev trees need `-DRT_REFERENCE_RENDERERS=ON`).
9. Release-guard (the agent) was never run on the final heads; each worker applied its checklist by hand. Run it before any push.
10. Clipped fans (the Sutherland-Hodgman output of programs 3/4/6) and the HUD still use the packet path. Sky (program 6) stays per-strip in the records path.

## 5. Decisions already made by the user

- Native is the only path going forward. Do not add new old-path code. The shipped app drops paraLLEl-GS, the CPU GS and the VU1-to-GS packet path once native covers 2D and textures (dev builds may keep them as a test oracle behind `RT_REFERENCE_RENDERERS`, default OFF). The removal list is at the end of `docs/native-render.md`.
- Accuracy target: exact, checked against PCSX2. Goldens were regenerated for the accuracy changes (user approved; 71 tests, `goldens.json` only, pictures never committed).
- Feature requests (#24 to #30) go on top of the native path, later.
- Thor tests only in the dev app (`io.github.silentsudin.roadtrip.dev`); back up and restore saves and settings; card slot 2 is the user's own save; never sleep the Thor or touch power settings.

## 6. Plan from here

1. **Check the five failing tests** (section 4 item 1), then decide whether motion is broken for native draws. This is the first thing to do.
2. **Merge** `re/native-cache3`, then `re/native-2d` into `re/integration2`; rerun the hardware-GS and paraLLEl-GS suites once each on a dev build (`-DRT_REFERENCE_RENDERERS=ON`).
3. **Close the GSR 2 gap on the Thor**: build the GPU table and objects in cached memory and write once (the `prepareNative` and memcpy cost), move record expansion fully off the GS thread, then a per-object GPU path to cut the 5000-plus draws a frame. Make records the default on desktop only after the GSR 2 Thor run holds 120 shown and the hardware-GS suite is clean.
4. **Phase 5**: native 2D (`docs/native-2d.md`: all 2D goes through `vifDirect` 0x226F70 and `sendTexturePackets` 0x207B50; about 98% of 2D vertices are atlas sprites), then direct textures (`docs/native-textures.md`: one upload funnel; the cases that need real GS-memory behaviour are listed there), then delete the old renderers from the shipped app and measure the size.
5. **PRs**, in this order, each fork branch pushed before its parent: (a) tooling, symbols and VU1 reference (`re/native-render`, `re/vu1-hle`); (b) CPU codegen + accuracy with the golden regeneration (`re/integration`'s CPU parts); (c) the native renderer and records; (d) bug and map fixes. Run release-guard first; the repository is public, so no game data or derived dumps (`build/` and `generated/` stay out).
6. **Thor-only items** when it is plugged in: #10, #6, #20, the second-screen map, the Body Shop glitch (#2), generated-frame flashing, and the Thor numbers after each speed step.
7. **Later**: SPU2 accuracy, the hardware GS blend gaps (alpha above 0x80, PABE/FBA/FBMSK, DTHE/COLCLAMP), recalibrate the Android compile-time weights in `GameBuilder` for the smaller generated code, and the feature requests.

## 7. Working notes for whoever continues

- **Usage guard:** `scripts/agent_usage_guard.py` polls `claude -p "/usage"` every minute, writes `build/scratch/usage.json`, and raises `build/scratch/agent-pause.flag` at 85% of the 5-hour window (or 92% weekly). A flag alone does not stop an agent mid-step: the main session must stop the workers itself (TaskStop) when the flag appears and resume them after the window resets. It is untracked and not committed. At 0% used the usage text has no reset time; the script handles that.
- **GPU lock:** one GPU job at a time on the Mac, `rt-gpu.lock` (two paths were in use: `build/scratch/rt-gpu.lock` and `/private/tmp/claude-501/rt-gpu.lock`). Never remove one you did not create; never use broad kills like `pkill -f pytest`.
- **Dev tree:** configure with `-DRT_REFERENCE_RENDERERS=ON` (separate build dir) for the regression suites; the app also needs the `rt_bundle` target built or it shows a modal "Setup failed" dialog; tests that run in a worktree need `--base-data` to a private data directory and copied goldens (`build/regression/goldens`, never committed).
- **Reverse engineering:** Ghidra first (`scripts/ghidra/decompile.py`, `fn.py`; the export lives in `build/decomp`, derived data, gitignored); REA only for host-side binaries because it refuses the R5900 ELF. Names and addresses go in `config/game_symbols.toml`.
- **Fork submodule:** each worktree's `third_party/PS2Recomp` is a local clone with its own local branches; fork heads are listed in the branch map, and any push needs the fork branch pushed first.
