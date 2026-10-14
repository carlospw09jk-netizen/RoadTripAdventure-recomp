# Accuracy of the recompiled EE (R5900) code

Branch `re/cpu-accuracy`; the recompiler and runtime changes are on the fork's `cpu-accuracy`
branch. This page says what was compared with what, what was wrong, what changed and what is left.
Addresses are the USA release (SLUS-20398); there is no game code or Ghidra output here.

How the audit was done:

- **Function set:** an unseeded Ghidra analysis of the ELF (`RT_GHIDRA_NOSEED=1 python3
  scripts/ghidra/decompile.py --only 0x200008 --out build/decomp-unseeded`) against the recompiler's
  output, the dispatch table in `generated/register_functions.cpp`, and a scan of the ELF.
- **Instruction use:** a scan of the ELF's `.text` (data words in it inflate some counts a little).
- **Instruction semantics:** `tests/ee_semantics` (ctest `ee_semantics`). `ee_semantics_gen`
  translates instructions and small programs with the recompiler's own decoder and translators;
  `ee_semantics_test` runs that code in the EE's float mode and compares it with reference models
  written from the EE Core and VU manuals (integer soft float with round toward zero and
  flush-to-zero; byte-wise unaligned loads and stores; lane models of the MMI ops). 2.13 million
  checks; it passes on arm64 and on x86-64 (built with real SSE, run under Rosetta).
  `RT_EE_TEST_IEEE=1` leaves the thread in the host's default mode: 500,000 of the checks fail,
  which is the size of the old divergence.
- PCSX2 is installed (`/Applications/PCSX2.app`, see the `pcsx2-reference` note) but it only runs
  games, not single instructions, so it was not the oracle for these tests.

## 1. Function discovery and control flow

| | |
|---|---|
| Ghidra functions (unseeded) | 1,441 |
| recompiler functions (.text) | 1,304 discovered, 1,375 generated (stubs, entry points, resumable-entry owners) |
| Ghidra starts with no dispatch-table entry | 99 (before the fix below: 101) |
| of those: no caller, no data pointer | 97: unreferenced SDK code (`sceDevCons*`, libc corners); nothing can reach them |
| recompiler functions Ghidra does not start | 90 (entry points the analyzer gave from `lui/addiu` code pointers, stubs) |

- **Direct calls and jumps into code the analyzer never turned into a function (fixed).** Two direct
  `J`/`JAL` targets lay in no recompiled function: `0x2849D8` (a tail call from `0x2848B4`, the code
  after the `_calloc_r` stub) and `0x277D30` (jumped to from `0x278000`, the CD callback's
  neighbour). The control-flow analyzer dropped every external target that was not inside another
  recompiled function, so the runtime found nothing at the target: its missing-function path ends the
  guest thread. The analyzer now reports such targets (`orphanEntryPoints`) and the recompiler makes
  a function for each, bounded by the next known start, repeating for the new ones' own targets
  (`discoverAdditionalEntryPoints`). Two functions appeared (`entry_277d30`, `entry_2849d8`); every
  other generated file is unchanged by this fix. A scan of every `J`/`JAL` in `.text` now finds no
  target outside the table except jumps inside the same function and calls from stubbed code.
- **Computed jumps.** 91 `JR` (not `$ra`) and 78 `JALR` sites. 38 switch tables are listed in
  `config/roadtrip.toml`; 43 functions have a `JR`/`JALR` the recompiler cannot resolve and promotes
  every block start of (`Indirect fallback promotions: 86`, 14,126 entries): the target then goes
  through the dispatch table to the owner function, which re-enters by `switch (ctx->pc)`. A scan of
  the data for words that point at a function prologue (`addiu $sp, $sp, -N`) found 52, all in the
  table. So no static evidence of an unresolvable target remains.
- **Missing targets at run time are now counted.** `reportMissingFunction` logged only the first
  miss; it now counts every one (`PS2Runtime::missingFunctionCount()`) and logs each distinct target
  once (up to 256) as `[guest-branch:missing-target]`. The regression run's logs are the check.
- **Data in code, branch likely, delay slots.** Branch-likely (about 2,000 in the ELF) skips its
  delay slot when not taken and the condition is read before the delay slot runs; a `JR` reads its
  register before the delay slot changes it (both tested as programs).

## 2. Instruction semantics

The game's use (opcode counts in `.text`): SPECIAL integer ops, `LDL/LDR/SDL/SDR` 762 each,
`LWL/LWR/SWL/SWR` about 25 each, `LQ/SQ` about 75 each, `BEQL/BNEL` 2,000, FPU `ADD/SUB/MUL.S`
about 1,400 (no `MADD.S`/`MSUB.S`/`MULA.S`/`SQRT.S`-accumulator forms at all; `DIV.S` 134, `SQRT.S` 57,
`CVT.S.W` 221, `CVT.W.S` 119, `C.EQ/C.LT/C.LE`; **no `CFC1`/`CTC1`**), `MULT1`/`DIV1` and
`MFLO1`/`MFHI1`, about 25 MMI ops (`PSUBW/B`, `PSUBSW`, `PEXTLW/UW`, `PCPYLD/UD`, `PAND`, `PXOR`,
`POR`, `PNOR`, `PCPYH`, `PPACW`), about 400 VU0 macro instructions (the `sceVu0*` matrix and vector
routines: `VMULA`, `VMADDA`, `VMADD`, `VSUB`, `VMULx`, `VOPMSUB`, `VOPMULA`, `VDIV`, `VSQRT`,
`VITOF`, `VFTOI`, `VMOVE`, `VMR32`, `VLQI`, `VSQI`), `MFC0` Status (11) and TagLo (4) as the only COP0
reads, `CFC2`/`CTC2` for the status flags, `FBRST` and `VPU_STAT`, and no `VCALLMS`, `BC2`, `CFC1`.

| Op | Expected (manual) | We did | Fixed |
|---|---|---|---|
| `ADD/SUB/MUL.S`, `CVT.S.W` (`DIV/SQRT/RSQRT.S`: next row) | round toward zero; denormal operand or result is zero; overflow gives +-MAX | IEEE round to nearest, denormals kept, overflow to infinity | yes: the executor thread runs in round-toward-zero, flush-to-zero mode (FPCR RZ+FZ, MXCSR RC+FTZ+DAZ), set in `EeScheduler::run`; no cost per instruction |
| `DIV.S`, `SQRT.S`, `RSQRT.S` | PCSX2 (default `FPUDiv.Roundmode`) rounds these to **nearest** and matches the demo bit for bit (`docs/pcsx2-accuracy-check.md`) | toward zero | yes: `ps2_fpu_round_nearest` (`ps2_runtime_macros.h`): the operation in double (toward zero), half a float ulp added to its encoding, then the truncating float conversion: no mode switch. VU0 macro `VDIV/VSQRT/VRSQRT` and `CVT.S.W` stay toward zero (nearest was worse there) |
| VU0 macro `VADD/VSUB/VMUL/VMADD*/VMULA*/VOPMSUB/VDIV/VSQRT/VRSQRT` | same FMAC rules | plain SSE / NEON in the host's mode | yes: same mode; the VU1 path already had it |
| `MADD.S`, `VMADD` | the product rounds, then the sum rounds | clang's default `-ffp-contract=on` fused `acc + a * b` into an FMA | yes: `#pragma clang fp contract(off)` in `ps2_runtime_macros.h` (the game does not use `MADD.S`; macro mode is intrinsics) |
| `BLEZ/BGTZ/BLTZ/BGEZ` (+likely, +AL) | test the whole 64-bit register | tested bit 31 (`GPR_S32`): 692 branches in 249 functions | yes: `GPR_S64`. It matters where a register holds a 64-bit value with a clear upper word (the soft-double `dp*` routines, `long long` code) |
| `LQ`/`SQ` | the low four address bits are ignored | used the address as given | yes |
| `VFTOI0/4/12/15` | float * 2^n truncated, saturated (+0x7FFFFFFF / 0x80000000) | SSE's CVTTPS2DQ: 0x80000000 for a large positive too | yes: `ps2_vu_ftoi` |
| `PEXEH` (macro and translator), `PEXCH`, `PEXEW`, `PEXCW`, `PROT3W`, `PREVH` | h0<->h2/h4<->h6; h1<->h2/h5<->h6; w0<->w2; w1<->w2; (w1,w2,w0,w3); halfwords of each 64-bit half reversed | adjacent swaps, a wrong word rotation, a 128-bit reversal | yes (the game does not use them; found by the tests) |
| VU0 MAC overflow flag | O set when the result overflowed | tested for exponent 0xFF, which RZ never produces | yes: also set for a +-MAX result |
| `LWL/LWR/LDL/LDR/SWL/SWR/SDL/SDR` | byte moves as the manual | correct | tested, no change |
| `MULT/MULTU/DIV/DIVU` and the `1` forms, `MULT rd`, `MADD`, `MADDU`, `MFHI1` ... | EE results incl. `INT_MIN / -1` and divide by zero | correct | tested, no change |
| `SLT/SLTU/DSRA32/DSLL32/DSRL32/SRA/SRL/SLLV/MOVN/MOVZ`, `MFC1/MTC1` sign extension | | correct | tested, no change |
| `C.EQ/C.LT/C.LE.S` (function codes 0x32/0x34/0x36) | FCR31 bit 23 | correct | tested, no change |
| `CVT.W.S`, `DIV.S` by zero, `RSQRT.S`, `SQRT.S` of a negative | saturation; +-MAX; |x| | correct | tested, no change |
| MMI set the game uses, `PADD*/PSUB*/PMAXW`, `PEXTL*/PEXTUW`, `PPAC*`, `PCPY*`, logic | lane models | correct | tested, no change |
| `CTC1` to anything but FCR31 | there is no other writable FCR on the EE | ignored | no change needed (the game has no `CTC1`/`CFC1`) |
| COP0 registers never implemented | | read as 0 | no change: the game reads only Status and TagLo |
| `ADDI`/`ADD`/`DADD` overflow | exception | `SignalException` | not exercised by the game (32 `ADDI`, never overflowing) |
| operands that are Inf or NaN bit patterns | the EE reads exponent 255 as a large number | IEEE inf/nan | open: the game would have to load such data into a float register |

Libm and the host stubs (`config/roadtrip.base.toml`, `drop_stubs`): the stubs for `cos`, `floor`,
`fabs`, `__ieee754_rem_pio2f`, `__kernel_cosf` and `__kernel_sinf` are gone; the game's own newlib
code runs. The first three take a soft-float `double` in a GPR, but the stubs read `$f12` as a float
(`cos` has four callers, `0x2443D0`, `0x244538`, `0x2446A8`, `0x244800`, which call it with a float
the way the game's own prototype says: what the hardware did with that mismatch is now what the
recompiled code does). The other three computed `sinf(x + y)` with the host's `sinf` and a
single-precision `x - n * pi/2`: the game's `sinf`/`cosf` (recompiled, which call them) now match the
hardware's maths. A scratch probe of the recompiled `sinf` against the exact value: worst error 1 ulp
for |x| < 2 pi, 2 ulp (sin) and 8 ulp (cos) to |x| < 100 in the EE's round-toward-zero mode, 1 and
2 ulp in IEEE mode; the host's `sinf` is within 1 ulp but is not the game's function.
Other numeric stubs (`sqrt`, `pow`, `exp`, `atan2` and friends in `LibC.cpp`) are only bound if the
game has a function of that name; the game has none (its libm is `sinf cosf atanf atan2f sqrtf fabsf
floorf` and the double-precision `cos`, `floor`, `fabs`, plus the soft-float routines).

Unimplemented SDK stubs (`TODO_NAMED`: `sceSdTransToIOP`, `sceDmaRecv*`, `sceDmaWatch/Pause`,
`sceTty*`, `sceDeci2*`, the `sceSSyn*`/`sceSynthesizer*` family and others) threw an exception that
ended the game thread. They now log (up to the existing per-name limit) and return 0.
`RT_STRICT_STUBS=1` restores the exception.

## 3. What changed, and what moves

- Everything the EE computes in floating point, and VU0 macro mode, is now round-toward-zero and
  flush-to-zero: object and camera matrices, physics, interpolation and AI all move by their last
  bit or two, so **every golden state hash that includes game state changes**; pictures change by
  sub-pixel amounts (the suite's goldens compare images by hash first, then by tolerance under the
  hardware GS). The measured list follows.
- `RT_EE_FLOAT_MODE=0` turns the mode off for A/B comparisons. The `[perf]` cost is nil: the mode is
  set once, and RZ and FZ cost nothing in the FPU.
- The game's `sinf`/`cosf` run natively; rotation matrices differ from the host's by up to an ulp.

### Regression suite with these changes (paraLLEl-GS, `scripts/regress.py -n 4`)

75 failed, 63 passed, 2 skipped, 3 xfailed. Not one is a crash or a missing-target log
(`[guest-branch:missing-target]` appears nowhere). Nearly all are golden pictures that moved:
`test_adventure_races` (25 courses), `test_quick_race_course` (9), `test_towns` (all 10),
`test_two_player` (12), `test_world_grand_prix` (7), `test_smoke` (quick race, adventure continue),
`test_sections` (peach town drive), `test_president`, `test_factory`, `test_memcard` (3),
`test_menus` (attract demo), `test_progressive_fields` (2): game state differs by float rounding, so
cars and cameras sit a little elsewhere (most differ in under 1% of pixels). Two are scripted-driving
scenarios that depend on the exact trajectory: `test_stamp_86_angels_wings` (the bot no longer meets
Dust at the scripted moment) and `test_ski_jump_lands` (the result text is read a frame early:
`ratulations!`). Left for a decision, goldens not updated. Not run: the hardware-GS suite.

## 4. Open items

- Inf/NaN operands (exponent 255) are not read as numbers. Cheap to add as a clamp in `FPU_ADD_S`
  and friends if the game is ever seen to need it (nothing in the suite does).
- The VU0 macro flags' sticky and `D`/`I` bits, `VRNEXT` (a simple LFSR, not the VU's), `BC2`.
- FCR31 flag bits (O/U, SO/SU) for add/sub/mul are not set.
- 97 unreferenced Ghidra functions (SDK corners) are not recompiled; if a state file or a mod ever
  calls one, the missing-function counter says so.
- Possible later check with PCSX2: run a short deterministic stretch in both and diff RAM, for the
  libm change (the four `cos` callers) in particular.
