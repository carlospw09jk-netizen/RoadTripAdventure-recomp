# EE accuracy changes against PCSX2 (attract demo)

Question: does the build with the EE float-accuracy changes (branch `re/cpu-accuracy`, ac7a0a1: RZ
float mode, no FMA contraction, 64-bit branch tests, libm stubs dropped) match PCSX2 better than the
build without (`re/native-render` 591f741 / main)? Tools: `scripts/pcsx2_diff/`. No game data or
memory dumps are in the repository.

## Method

- Clock: the attract demo's own frame counter, frames run of the demo task 0x20A150 (task pool slot 2,
  +8 = 0x177A138). HUD TOTAL TIME stays 0 in the demo. The race loads at different speeds, so
  PCSX2's clock = the recomp's + 351 (constant; found by `align.py`).
- State: the 24 car objects (0x177ACE0, stride 0x270, 3744 words) once per clock, clock 53..3052 of the
  recomp (virtual time, headless `RT_RENDER=0`, `rt_attract_dump.py`) and the same frames from PCSX2 v2.2
  over PINE (`pine_dump.py`: live reads, a sample kept when the clock is the same before and after and
  two reads in a row agree; `pcsx2_run.sh`).
- PCSX2 is deterministic: two runs are bit-identical for 2,547 frames (the rest is the sampler). Its
  defaults (`PCSX2.ini`): FPU chop, FPUDiv nearest, VU0/VU1 chop, denormals zero, fpuFullMode off (full mode
  changed nothing in the first 850 frames).
- Builds compared: old = the main checkout's app, new = `re/cpu-accuracy`'s app, each building the game
  from its own bundled recompiler and headers. Variants of the new build (`mkvariants.sh`, `mkdivrn.sh`,
  `mkprobe.sh`) change one thing each.

## Result: new is closer, and one more change would make it much closer

Words (of 3744) that differ from PCSX2 (default settings) at recomp clock:

| clock | old | new | new + DIV.S/SQRT.S/RSQRT.S round to nearest |
|---|---|---|---|
| 53 (grid) | 221 | 246 | 22 |
| 100 | 479 | 272 | 64 |
| 200 | 579 | 295 | 64 |
| 400 | 1129 | 506 | 134 |
| 800 | 1650 | 1472 | 475 |
| 1200 | 1671 | 1644 | 1006 |
| 2000 | 1457 | 1447 | 1450 (chaotic from here: collisions) |

First clock at which some car's position differs from PCSX2 by more than 1e-6 / 0.001 / 0.1 / 1.0 /
10 units:

| | 1e-6 | 0.001 | 0.1 | 1.0 | 10 |
|---|---|---|---|---|---|
| old | 53 | 127 | 361 | 657 | 2237 |
| new | 53 | 401 | 587 | 818 | 1549 |
| new + nearest div/sqrt | 284 | 637 | 962 | 1293 | 2251 |

- New with PCSX2's FPUDiv set to chop gives the new build's grid state and car 0 at clock 700 bit for bit
  (1053.61572265625, 7.428073883056641, 1050.1700439453125); new with nearest DIV.S/SQRT.S gives PCSX2's
  default bit for bit at the same place. So the recomp and PCSX2 agree on every op except that one setting:
  **PCSX2's default rounds DIV.S, SQRT.S and RSQRT.S to nearest** and the new build rounds them toward zero.
  `ps2_fpu_div_s`, `ps2_fpu_sqrt_s`, `ps2_fpu_rsqrt_s` (`ps2_runtime_macros.h`) are where it is decided;
  `mkdivrn.sh` is a working patch (FPCR switched around the one instruction on aarch64).
- VU0 macro DIV/SQRT/RSQRT and CVT.S.W at nearest are worse (first diff at clock 96 and 58): they stay RZ.
- Not explained: the integer at car + 0x1BC (22 of 24 cars at the grid: one LSB of a fixed-point value),
  and from clock 284 the orientation matrix (car + 0x20..0x44) of cars 14/15 (near-zero elements,
  -2.1e-5 against -4.5e-11: a cancellation). They survive every variant here.

## Verdicts per change

- **Rounding mode (RZ + flush-to-zero)**: right, and the whole effect: with `RT_EE_FLOAT_MODE=0` the new
  build equals the old one: every car position bit for bit at all sampled marks, and only a few `+0x1BC`
  integers differ (first at clock 74, 3 words). Needs the nearest exception above.
- **FMA contraction off**: no effect: variant frames identical (3000/3000) and the game library has
  the same size. Harmless.
- **64-bit branch tests**: no effect in the demo (variant built with the old recompiler: 3000/3000
  frames identical). Correct by the manual; not exercised here.
- **libm stubs dropped**: demo identical for 370 clocks, then one LSB in one word (car 21 + 0x1BC): no
  material effect. The four `cos` callers: 0x2443D0, 0x244538, 0x244800 run (Quick Race and 2 Player course
  carousels, `drawCarouselScene` 0x2455C0, the first calls logged by `mkprobe.sh`); 0x2446A8 (parent
  FUN_00246250) did not run in the title/demo, Quick Race, 2 Player (right-away and saved cars), or the
  Adventure factory and town. They call `cos` with a float in $f12 and read the result from $f0, which
  the game's own `cos` never writes: after it $f0 is 0.0 (probe: f12 = 0.70, 1.39, 2.10 rad, f0 = 0), so
  the value they use is 0, where the old host stub put cosf($f12) there. Equal to the hardware by
  construction (same code, same registers); not compared with PCSX2 (the carousel is out of reach of
  PINE; there is no input).

## The two scripted-driving failures

`test_stamp_86_angels_wings` and `test_ski_jump_lands` pass on the old build and with
`RT_EE_FLOAT_MODE=0`, and fail on the new build and on every variant that keeps RZ (libm, branches, FMA,
nearest div): the rounding mode causes them, nothing else. Both are trajectory-sensitive:

- ski jump: it lands (91 m, old 101 m, still inside the test's 90..140). The text check fails because the
  harness polls every 60 vblanks and first looks after "Smooth landing. Cong" has typed; polling every 5
  vblanks passes on both builds. A harness phase problem, but the 10% shorter jump is real and comes
  from RZ (the same with nearest div).
- Dust: the bot does not meet him within its 180 s with the new trajectories.

These need a robust harness, not the old trajectory. Whether 91 m or 101 m is the hardware's cannot be
told from the demo; it needs input in PCSX2.

## Result of the change (branch `re/cpu-accuracy-2`)

`DIV.S`, `SQRT.S` and `RSQRT.S` now round to nearest in the runtime itself (`ps2_fpu_round_nearest`,
`ps2_runtime_macros.h`); ADD/SUB/MUL/CVT and the VU0 macro DIV/SQRT stay toward zero. Same tools, a fresh
PCSX2 dump (3,100 frames) against the new build's (3,000 frames), clock shift 351 (`align.py`: mean car 0
position error 1.2e-7 at 351):

| clock | words differing (of 3744) | before (RZ everywhere) | variant `mkdivrn.sh` |
|---|---|---|---|
| 53 | 22 | 246 | 22 |
| 100 | 64 | 272 | 64 |
| 200 | 64 | 295 | 64 |
| 400 | 134 | 506 | 134 |
| 800 | 475 | 1472 | 475 |
| 1200 | 1006 | 1644 | 1006 |
| 2000 | 1450 | 1447 | 1450 |

First clock at which a car's position differs from PCSX2 by more than 1e-6 / 0.001 / 0.1 / 1.0 / 10:
284 / 637 / 962 / 1293 / 2251 (the variant's row exactly). The 22 words at the grid are the unexplained
`car + 0x1BC` integers. Cost: a double divide or square root plus one integer add and a conversion; a
micro-benchmark of 1e8 divides measured 0.77 ns against 0.78 ns for the float divide. `tests/ee_semantics`
checks the three against nearest-rounding reference models (2,130,002 checks, 0 failed on arm64 and on x86-64
under Rosetta; taking the half-ulp add out makes 25,384 of them fail).
