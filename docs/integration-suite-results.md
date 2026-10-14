# Integration suite results (re/integration, 580638f fork / 67051da parent + docs)

Goldens were not updated. Run with the pgs backend (the harness default) and with `RT_GS_BACKEND=hw`, `-n 4`, macOS.
Build: `-DRT_REFERENCE_RENDERERS=ON`.

## pgs: 75 failed, 98 passed, 2 skipped, 3 xfailed (18.5 min)

Golden mismatches (73) are the cpu-accuracy-2 moves (PS2-exact float rounding changes the guest's state), same families as that branch's run (73 failed there):

- Large: Sandpolis Night Glow A/B 42.0%, Temple Raceway A/B (Fuji City) 35.5%, 2P saved-car carousels 24.8% / 21.3%, Ninja Temple C 11.1%, President race start 9.2%, attract demo 1.83%.
- Small: the other races 0.02-0.31% (Mushroom River C 0.31%, Lava Run A/B 0.19%, Lagoon A/B 0.18%, Slick Track 0.10-0.12%, Sunny Beach 0.10-0.11%, Peach 0.03-0.06%, ...), progressive quick race 0.22%, 2P starts 0.01-0.28%, quick-race starts 0.06%, factory/card/memcard frames 0.00% (max diff 154: a few pixels), wgp factory frames 0.00-0.01%.

Not golden mismatches (2), both from tests that came with native-merge, not in the accuracy branch's suite:

- `test_map_overlay.py::test_field_area_map_is_routed`: also fails under hw (see below). Not verified whether it also fails on native-merge alone.
- `test_native_draw.py::test_native_takes_part[frame_gen-course_2]`: "the old path declined nothing here". That test is about the hardware GS's native draw; under the pinned pgs backend there is nothing to decline. Expected under pgs, passes under hw.

## hw: 44 failed, 129 passed, 2 skipped, 3 xfailed (20.5 min)

hw compares to the old paraLLEl-GS golden PNGs (3x3 tolerance, limit 3% of pixels / 30% of a 32 px block) and sound hashes. Every failure is in a scene whose game state moved with the accuracy changes:

- Frames: Night Glow A/B 9.1%, Temple A/B 4.5%, wgp_3 9.0%, wgp_2 4.4%, Lagoon records 3.3%, 010_peach_town_drive 3.3%, Ninja Temple C 2.96% (block 92%), President 1.1% (block 34%), town driven frames (the car ends elsewhere): Cloud Hill 80%, White Mountain 37.6%, Mushroom Road 18.9%, Sandpolis 16.9%, Papaya 3.1%, Chestnut 2.6%.
- Sound hash mismatches (rms 7,600-20,300) in the 2P events, attract demo, quick races, Miner 49er A/C, towns Fuji/My City/Peach, wgp 1/4-7.
- `test_field_area_map_is_routed` (as under pgs): dest 'overlay', `overlay: False`.
- The Known hw failure in CLAUDE.md (`2p_River_Raceway_20s`) is within this list (2P River).

Nothing in the list points at a codegen problem: codegen+accuracy state hashes equal accuracy alone (`ee_bench.py --diff`: identical, 1800 vblanks).

## Merge fixes found by the suites and ctest

- 128-bit direct RAM path (codegen) now aligns down (accuracy's `ee_semantics`: 7,468 failures before).
