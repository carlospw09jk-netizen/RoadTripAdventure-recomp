# Bug notes on the native / hardware GS path

One entry per issue: what, cause, fix, test. Issues are never closed from here. The shipped app
has the hardware/native path only, so paraLLEl-GS-only differences are not chased.

## #3 Texture pack stops applying after leaving and re-entering a building

- Cause (from the code; the Thor's 4x pack hits it through eviction): `TextureTools::submit`
  refused a texture when its job queue held more than 512 entries (a burst at a scene load, or the
  resubmit of every texture after an eviction), but the backend had already put the texture's key
  in `m_submitted`, so it was never offered again until the next resubmit. Its pack image never
  arrived and the building drew without the pack.
- Fix: `submit` returns whether it took the job; the backend erases the key from `m_submitted`
  when it did not, so the next draw of that texture offers it again. `RT_TEXTURE_JOBS_MAX` lowers
  the queue limit (tests).
- Test: `test_texture_pack.py::test_texture_pack_survives_full_queue` (hardware GS only, limit 3;
  passes with the fix; not run without it).

## #4 River Raceway 2P road decal covers more ground (not root-caused)

Measured on the hardware GS with native draws on: `2p_River_Raceway_20s` differs from the golden in
0.95% of pixels (limit 3%) and a 32 px block at 42% (limit 30%), in blocky patches along the road
edge in player 2's half, where the hw picture's road polygon reaches further over the dirt.

- The draws involved (`RT_HWGS_BATCH_LOG`, last frame): two big strip batches with TEST 0x737fb
  (ATE GEQUAL 0x7F, AFAIL RGB_ONLY, ZTE, ZTST **GREATER**, Z written), 84 and 18 vertices,
  screen boxes 1024 px wide (x -192..832: the guard band of the clipped programs) reaching below
  the scissor, Z 0.016..0.78. They are in the log, so they go through the packet path (the fans of
  clipped native draws), not `native.vert`. Terrain draws around them are GEQUAL (0x537fb).
- Ruled out: integer rounding of Z (rounding `gl_FragCoord.z` to 24-bit integers in `draw.frag`:
  picture bit-identical), exact per-triangle integer plane evaluation in `native.vert` (no effect,
  the draws are not native), and a depth bias on ZTST GREATER draws: 1.5 and 30 steps change
  nothing, 200,000 steps makes it worse (5.1% off). So the decal and the terrain differ by more than
  float noise where they disagree, or the difference is not in Z at all.
- Dropping the 0x737fb batches (`RT_HWGS_DROP=t737fb`) changes the road much more than the
  difference against the golden, so the decal is drawn; the edge of the road polygon sits ~20 px
  further out in hw. Next steps: compare that polygon's vertex positions with the golden's (the
  clipped-fan vertices come from the `vu1hle` reference, `RT_NATIVE_DRAW_VERIFY=1`), and the
  scissor handling of a triangle whose box is far below the scissor. `RT_NATIVE_DRAW=0` should be
  run once to see whether the packet path gives the same picture (the issue predates native).

## #6 Frame skip level 1 trips on two late frames in 30

- Cause: `GS::updateFramePacingUnlocked` counted every frame starting two or more vblanks late,
  including the first 3D frame of a new scene, where the GS thread spends 25-35 ms setting up the
  area's textures and targets, and paused frame generation at 2 of 30.
- Change: a scene's first 3D frame after 2D ones is not counted, and level 1 needs 3 of the last 30.
  Not measured on the Thor (off limits here): sustained overload still reaches level 1 within
  three late frames.
- Test: none (needs real-time load on a device).

## #20 Windows Intel HD630 crash at the Conspiracy screen

The issue body is unrelated text; the owner's comment says the GPU lacks the Vulkan features the
renderer needs. No Windows binary is available here, so no REA analysis was done.
Change: `failVulkan` in the presenter (`gs_pgs_presenter.cpp`) now reports a readable message (log
and message box) when instance, device or swapchain creation fails: the devices the Vulkan loader
offers, their Vulkan version, and whether they have the swapchain and push-descriptor extensions.
No fallback renderer.

## #2 Second screen glitches in the Body Shop

Write-up only. Thor-only (second display), and the issue says it does not reproduce from a state
loaded into the Body Shop, so the suspect is entering it: the Map to Notebook switch with the map
layer being released (`wantMap` drops, `mapFramesSeen` stops, three frames without a map clear the
snapshot). Needs a screen capture of the lower display during entry.

## Not done in this pass

- #31 stamp book letters at 16:9 and #10 GSFE chunk after a load: not started. For #31 the caption
  is the string `#Gave#a big peach#to James!` (ELF file offset 0x12fed5, lines split at `#`), the
  stamp book is built by 0x22C648; the likely cause is the letters being anchored as separate
  elements by `WideLayout`.
