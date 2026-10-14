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

## Visual review of the biggest moves (golden vs actual, no goldens updated)

Caveat: `build/regression/failures` holds the last run's pictures (hw ran last), so for tests that failed in both runs the "actual" is the hw picture.

| test | verdict | evidence |
|---|---|---|
| Night Glow A (42% pgs / 9% hw) | legit | same street, HUD identical (00'18"99, 24th/24, map); camera and lamp posts shifted a little |
| Temple Raceway A (35.5%) | legit | same scene, time 00'18"89 identical, car slightly rotated: camera heading differs, all geometry and sky present |
| Ninja Temple C (11%) | legit | same corridor, time/rank/map identical, small lateral offset and different dash-line phase |
| President race start (9.2%) | legit | same corner and car, time 00'19"61 identical, speed 51 vs 53 mph, gear/camera phase |
| Attract demo 45 s (1.8%) | legit | same cars and framing, sub-pixel/animation phase; nothing missing |
| 2P custom carousel (24.8%) | legit | same menu cards, rotation phase of the carousel differs by a few pixels |
| Cloud Hill town drive (hw 80%) | legit, with doubt | both show the rainbow town, HUD, speedo, car; camera pitch differs (horizon higher in the new one) and the old picture has the car semi-transparent and an empty minimap. The mismatch is the state the car is in at the sampling time. Worth one look at a second frame, nothing missing/garbage |
| White Mountain town drive (hw 37.6%) | legit | same snow town, minimap, speedo (42 mph both); camera pitch differs (horizon lower) |

No NaN/black frames, missing objects, wrong colours, garbage geometry, or wrong text in any of the eight.

## `test_field_area_map_is_routed` alone (hw, this build)

Run twice alone: first run passed, second failed (same assertion: dest 'overlay', `overlay: False`). It is flaky (a timing race in the map router/overlay latch), not a deterministic merge failure.

## Goldens regenerated (user-approved)

`regress.py --update-goldens` with the pgs backend on the 73 tests that failed as golden or sound-hash mismatches in the runs above (the pgs run's list, 71 updated; pixel-diff percentages are in the tables above). Only `tests/regression/goldens.json` changed: 193 entries rewritten, none added or removed; pictures and checkpoints stay in `build/regression`.

Not regenerated: `test_field_area_map_is_routed` (flaky), `test_native_takes_part[frame_gen-course_2]` (passed in the rerun), and `test_adventure_race[Sandpolis-Night_Glow_Raceway_A/B]`. Those two now fail before reaching their golden: "the bot was stuck at (592, 744) (line point 161/1017, 19 recoveries) and retired" (B: 172/1022, 26 recoveries), deterministically. The adventure race bot cannot finish Night Glow with the exact-float physics. Their goldens still hold the old pictures. Needs a look at the bot's line (or at the course) before they can be regenerated.

Counts, before -> after regeneration:

| suite | before | after |
|---|---|---|
| pgs | 75 failed / 98 passed | 3 failed / 170 passed (Night Glow A/B bot retired; field-area map flaky) |
| hw | 44 failed / 129 passed | 4 failed / 169 passed (the same three, plus `2p_River_Raceway_20s`: 0.71% off by >48, worst block 37% vs the 30% limit, the known River tolerance case fixed on re/native-bugs) |
