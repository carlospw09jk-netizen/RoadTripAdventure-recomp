# Night Glow A/B: bot problem, not a physics difference

Question: after the accuracy changes (EE float mode, libm, branch tests), `test_adventure_race[Sandpolis-Night_Glow_Raceway_A/B]`
retired ("the bot was stuck at (592, 744)"). Is that the bot, or a real physics/game difference that would stop a
person finishing Night Glow? Answer: the bot. Evidence (macOS, paraLLEl-GS, lockstep; the old build is
`build/scratch/baseline-app`, the new one the integration build, each with its own copy of the data directory so
their game libraries do not overwrite each other).

## What happens

- The trail the bot learns from the AI leader is the same course in both builds (1023 vs 1000 points, nearest
  points within 1-8 units over the whole lap; start frame differs only by rounding). Same course, same line.
- At the first right-hander (line points 126-132, about (571, 644) -> (580, 653), a 70 degree turn), the new run
  arrives about 6 units west of the line and clips the corner of the brick building on its inside (x about 575.5,
  z >= 655). The car is deflected into the straight side street (x = 575, heading +z) beside an iron fence, a
  dead end running to the glass tunnel at about (685, 744). The bot was never at (592, 744) by choice: that is
  where it ends up wedged after crawling 90 units down the wrong street. Screenshots at s08: the old car turns
  east along the fence; the new one is past the corner with the brick wall at its right.
- In the side street the car is not pinned: probe (`tests/regression/test_night_glow_escape.py`,
  state saved at (575, 668), restored for each try) shows holding gas straight accelerates along the wall to
  10304, gas + left turns freely, and holding reverse (`circle`) for 200 vblanks backs the car straight out
  from z 672 to z 656 at up to 6600. So a person drives out; the geometry does not trap the car.
- The bot's detector never saw it: it counts "stuck" only when the car moves less than 1 unit in 45 vblanks, but
  the car crawls along the fence at 0.11 units/vblank, and `backtrack` is off for Night Glow A. Its own recoveries
  (60-240 vblanks of reversing) were too short to get back around the brick corner.

## Is the physics different? Not in any way that matters

Same input in both builds (`test_accel`: full gas from the start, then full left, straight, brake; Night Glow A,
lockstep, macOS): speed after 440 vblanks 17846 (new) vs 18418 (old), position 3 units apart after 400
units; yaw after 200 vblanks of full left differs by 0.07 rad; braking distance within 2%. Both builds hit
the same obstacle at (586, 487) on the straight. That is the size of change float rounding makes, in the new
build slightly slower (round-toward-zero loses a little on every accumulation, which is what the EE does).
**Attribution: the EE float mode alone.** The same test on the new build with `RT_EE_FLOAT_MODE=0` reproduces the
old build's numbers to the last digit shown (spd 18018 / 12030 / 9984 / 9625 at vblanks 420 / 580 / 700 / 940,
positions within 0.01), so libm, branch tests and fp contraction move nothing in this scenario. (With the mode
off the bot still fails on Night Glow A, at (579, 430), so the bot's line is fragile to any perturbation.)
No game routine needs tracing: nothing in contact words, wall scrapes or air flags looked wrong
(contacts 0x13000 = glass tunnel panels, 0 = asphalt, brief -1 on one wheel at walls, nothing airborne).

## Fix (bot only; no runtime change, no goldens touched)

`tests/regression/rtharness/driver.py`:
- **Lost detection**: more than `lost_distance` (25) from the line and not getting closer for `lost_vblanks`
  (300) starts the existing backtrack (reverse along our own breadcrumbs to the line), as for a wrong branch.
- **Learned slow zones**: wherever the bot gets stuck, is lost or backs out, the stretch of line just before
  that point (45 points, widened by 20 each time, at most 150) is taken at 0.7 (then 0.75x, floor 0.45) of the
  leader's speed on later laps and retries.
- `[bot] careful` in `config/game_state.toml` (Night Glow A and B): corner at 0.8 of the leader's speed and a
  4-unit minimum look-ahead, since the first try matters (B cannot get out of the side street once in).

Shortcuts (COVERAGE.md): none beyond the existing bot; the races are driven for real. The Night Glow bot
settings are tuned to this corner.

Result: all 26 `test_adventure_races` pass on the integration build (pgs, goldens as committed; 12.7 min).
The first version of the lost detection (12 units) broke Snow Mountain B, which passed before; 25 units does not.
Not run: the other users of `Driver` (WGP, President, Quick Race, 2 Player), which the lost detection could in
principle touch (only when more than 25 units from the line for 5 s with no gain).

Probes (diagnostic, skipped unless `RT_NG_PROBE=1`): `test_night_glow_probe.py` (log the car's state and
contact words around trouble; `RT_NG_RACE`, `RT_NG_CFG` for DriverConfig overrides),
`test_night_glow_escape.py` (`test_escape`, `test_shots`, `test_accel`).
