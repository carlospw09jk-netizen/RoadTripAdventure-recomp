# Enhancements on the native draw path (re/native-enh, 14 Oct 2026)

Rule: native is the only render path. This is the audit of every enhancement that touches 3D drawing or
its data, against `re/native-merge` (fork `native-merge` 132b94b). Status: **works** = by construction
or by an existing test, **fixed** = a gap found here and fixed in the fork, **gap** = open, **untested**
= no test could be run yet (the GPU lock was held by other jobs for the whole session: every runtime
check below marked "to run" is written but not yet executed; run
`python3 scripts/regress.py -k test_native_enhancements` with `RT_GS_BACKEND=hw` when the lock is free).

## Fixes made (fork commits on `native-enh`)

1. **Frame skip dropped the classifier's view of native 3D** (`GS::processNativeMarker`: `if (m_skipDraw) return;`
   came before the widescreen classifier and the frame-buffer skipped mark that the packet path runs in
   `processGIFPacket`/`vertexKick`). In a skipped frame whose 3D is all native strips the frame's first draw
   the classifier saw was the HUD (verdict votes 2D, no HUD start, no 3D numbering, no depth buffer note),
   and the frame buffer wasn't marked skipped. Now a skipped native strip does the same bookkeeping (skipPacket /
   `nativeReference` + `classifyWideUnlocked`, the fbp mark, `m_lastZbuf3D`) and draws nothing.
2. **A state load left the native draw queue's slots pending** (`NativeDrawQueue::clear()` had no caller). Draws
   whose markers a load dropped would count as pending for good; a few loads in, `ring_full` would send
   every strip back to the VU1 run. `GS::serializeState` now clears the queue on load.
3. **native.vert pipelines were never warmed up** (`PipeKey::native` was "never noted"): the 256 built-in states
   and the cache file are draw.vert states, so the first strip drawn with each 3D state compiled a pipeline in
   the middle of a frame (8-25 ms on Adreno). The warm-up now builds a native twin of every state with a
   depth buffer, native states are noted in `hw_pipeline_states.txt` (14th field; 13-field files still parse).

## Checklist

| Feature | Status under native | Test |
|---|---|---|
| Frame generation: shadow frames redraw native draws with t, matrices from MotionTracker | works (re/native-gen: matrices are the object's own; 200 px rule kept) | `test_native_draw::test_native_takes_part[frame_gen]` |
| Per-vertex motion for entries 2-6 (3 billboards, 4 terrain, 5 offset strips, 6 sky: `motionOn` needs the marker's context) | works; coverage per program asserted | `test_native_enhancements::test_motion_on_every_program` (to run) |
| Clipped fans (polygons the clipper cut) | works: packets through the packet path before the strip, their own `moveVertices` (small, screen edge). **Residual use of the VU1->GS packet path**: fans, HUD classifier reference (`nativeReference`), UI-mask/map triangles on the CPU | `test_native_draw` (declines 0) |
| TAA / GSR 2 / ASR / MetalFX / FSR3-DLSS-XeSS inputs: depth (raw GS Z24 copy at the HUD start) | works: native.vert writes the same D32F Z (`z = zi * zUnit`) into the same depth image; the copy is path-independent. Frame-skipped frames now still number the 3D (fix 1) | indirectly `test_native_matches_old_path` (depth test decides pixels); no direct readback test (**gap**: no socket command exposes `PgsShared::depth/motion`) |
| Per-pixel motion attachment | works: native.vert `vMotion`; mean 0.047 vs packet path, 0.1% of pixels > 24 levels (re/native-gen) | `test_native_takes_part[motion]` |
| Sky / billboard motion | sky: SetupA object (entry 7) matrix, billboards: entry 3 vertex runs, both get contexts like any run | `test_motion_on_every_program` (to run) |
| UI mask, WideLayout (widescreen, HUD edges, 2D verdict) | works: classifier verdict from the draw's data, HUD placement in the shader; skipped frames fixed (fix 1) | `test_native_widescreen_hud`, `test_widescreen` under hw (to run), `test_frame_skip_with_native_draws` (to run) |
| Map layer, Map = Enhanced, second screen | works (map draws are native draws diverted to the layer; CPU triangles for the box) | `test_native_map_layer`, `test_map_overlay` (hw) |
| Texture dumps / HD packs / hash names / REGION_CLAMP upload rule / recolour / ASTC-BC7 cache | works by construction: the native batch's state goes through `openFor` -> `resolveTexture` -> `HwTexturePacks` exactly as a packet batch, `F_REPLACED` in draw.frag (shared by native.vert pipelines) | `test_texture_pack`, `test_texture_pack_hd`, `test_texture_pack_cache` with `RT_GS_BACKEND=hw` (to run; the cars and terrain are native, so a pack that shows in the race shows on native draws) |
| Glyph repaint (button glyphs) | n/a to native: the font atlas draws are 2D sprites, never strips (entry 5 minimap aside) | `RT_GLYPH_DEBUG` manual |
| Progressive fields, interlace flicker | works: native.vert uses the same `posScale` (vertical scale doubled) as draw.vert; flicker is a post pass | `test_progressive_fields` under hw (to run) |
| Save states (HWG1: targets, local memory) | works for the render side (queue recorded first); **fixed** queue leak on load (fix 2) | `test_save_load_with_native_draws`, `test_cross_process_with_native_draws` (to run; the old `test_save_states` pins pgs and never touched native) |
| Frame skip | **fixed** (fix 1) | `test_frame_skip_with_native_draws` (to run; real time, `RT_GS_TEST_LOAD_US`) |
| Pause / resume | works: independent of the draw path | `test_pause_resume` |
| Performance overlay, RT_PRESENT_DUMP, RT_SHOW_DEPTH/MOTION/UI | presenter-side, path independent. `RT_SHOW_*` need a window | manual |
| Pipeline warm-up | **fixed** (fix 3) | `[hwgs] pipelines:` log line shows ~2x states compiled |
| Debug hooks (`RT_GS_BATCH_LOG`, `RT_GS_STATE_SURVEY`, `m_debugHistoryPaused`) | by design decline native (draws become packets): survey/batch logs show the packet path, not the shader's | none |
| Android | **gap**: native is off by default (one draw per strip is GPU bound on the Thor); until the batching of strips lands Android still runs the VU1 packet path for 3D | worker C |

## Features that depend on the non-native renderers

What would be lost or must be rebuilt if paraLLEl-GS, the CPU GS and the packet path were removed:

- **The hardware GS itself embeds `GSCpuBackend`** (`m_cpu` in `gs_hw_backend.cpp`): GS local memory, host-to-local
  transfers, CLUT state, readbacks (`ReadVram`/`SnapshotVram`) and the transfer part of save states. Only the CPU
  *rasteriser* can go; `gs_cpu_backend.cpp`'s memory half (or `ps2_gs_memory`) must stay.
- **The presenter is paraLLEl-GS's Granite device** (`createPgsPresenter`, `PgsShared`, `PgsControl` are shared names).
  The GS emulation (`gs_pgs_backend.cpp`, parallel-gs `gs/`) can be dropped while Granite stays; `PS2X_ENABLE_PGS`
  builds both, so the CMake needs splitting.
- **Options -> Renderer row** ("Accurate (paraLLEl-GS)"), `[display] renderer`, `RT_GS_BACKEND=pgs|cpu`, the raylib
  presenter fallback (machines without Vulkan: `RT_PRESENTER=raylib`, default with the CPU GS) and the "Mipmaps"
  row (hidden on hw already) go. Without Vulkan the app cannot run at all.
- **Supersampling above the render scale**: hw gives 1x/2x/4x render scale (sqrt of the setting), no 8x/16x
  anti-aliasing; paraLLEl-GS's sample-exact AA is lost (FXAA/SMAA/temporal remain).
- **Pixel accuracy**: hw is not bit-exact (alpha/Z rounding, the 2D snap rules, `2p_River_Raceway_20s` decal layer);
  the pgs pictures are the oracle for the goldens (`rtharness/goldens.py`) and stay dev-only.
- **Regression harness default** is `RT_GS_BACKEND=pgs`: every test that does not set `hw` (smoke, quick race,
  towns, stamps, saves, menus ...) exercises no native draw. Flip the default to hw (goldens then compare by
  `hw_verdict` against pgs-made PNGs) once the oracle PNGs are kept in a fixed place; `--update-goldens` needs pgs.
- **Old save states** made under pgs load into hw with empty render targets (one frame differs).
- **Frame generation** on pgs used shadow instances; the hw shadows (`recordShadows`) replace it (already default).
- Features that only exist on hw already: Map = Enhanced, second-screen map, UI-mask scissor, texture-pack cache.

## Not yet done / next

- Run the "to run" tests (GPU lock), then flip the harness default to hw.
- A socket command (`temporal`) exposing depth/motion buffer checksums, to test the upscaler inputs without a window.
- Residual packet-path uses listed above; Android enablement (worker C).
