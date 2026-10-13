# Road Trip's VU1 microcode

What the game's VU1 microprograms do, in our words, and how our readable reference
implementations (`vu1_hle`, in the PS2Recomp fork) reproduce them. Plan:
Phase 1.3–1.4 of the native renderer (`re/native-render`).

The microcode is the game's: this page describes it and quotes a few instructions, never the
listing. Captures (`*.vu1cap`) hold game data and stay under `build/`.

## The image

- One 16 KiB microcode image for the whole game. FNV-1a 64 over the image (`Vu1Native::imageHash`):
  `0xe2868a5859386847`. About 0x1DF8 bytes are code.
- A jump table at 0x00–0x78: eight slots of two pairs, each a `B` to the program. The EE MSCALs
  the slot (0x00, 0x10, … 0x70); "entry N" below is the slot at N × 16.

| Entry | Code | Name (ours) | Role |
|---|---|---|---|
| 0 | 0x1268 | Init | Loads the clipped pipeline's matrices and parameters; clears both batch headers |
| 1 | 0x1438 | SetupLit | Object setup for entries 2 and 5: world→screen and world→clip from camera × world, lights into object space |
| 2 | 0x1620 | LitStrip | Lit strip (two lights + specular), unclipped |
| 3 | 0x03E8 | ObjectStrip | Clipped strip of a rigid object: local positions × rotation + origin |
| 4 | 0x0080 | WorldStrip | Clipped strip in world space (+ batch offset): the course and town geometry |
| 5 | 0x1898 | OffsetStrip | Unlit strip with a per-vertex offset; unclipped |
| 6 | 0x1AA0 | SkyStrip | Clipped, back-face-culled strip with a per-vertex offset |
| 7 | 0x1318 | SetupClipped | Object setup for entries 3, 4, 6 |
| — | 0x0720 / 0x0738–0x1260 | Clipper | Shared polygon clipper and its helpers (entries 3, 4, 6) |

All three object programs and the clipper are hand-scheduled, software-pipelined loops: one
vertex's transform overlaps the previous vertex's output. The reference implementations are
written per vertex; the pipelining shows only in which values carry from one vertex (or one
run) to the next, listed below.

## Data memory (qword addresses)

| Address | Contents | Used by |
|---|---|---|
| 0–3 | World (object) matrix, columns. SetupLit overwrites it with world→screen | 1, 7 |
| 4–7 | Camera matrix (world→screen incl. viewport), columns; VU5/VU6 also serve as offset axes | 1, 5, 6 |
| 8–11 | Clip matrix (→ guard-band clip space), columns | 0, 1, 7 |
| 12–15 | Clip→screen matrix (viewport), columns | 0, 7 |
| 16–18 | A 3×3 (light directions / rotation), rows; VF0 = (0,0,0,1) is its 4th column | 0, 1, 7 |
| 19–23 | Parameter block: fog sets, colour weights, light colours (meaning depends on the setup that loads them, below) | 0, 1, 7 |
| 24 | Tint (colour scale, w = alpha scale) | 1, 5, 6 |
| 25 | Second tint (SetupLit), or four cycling per-vertex offset weights (entries 5, 6) | 1, 5, 6 |
| 26 | Batch offset (xyz added to every world position; w added to the alpha base) | 3, 4 |
| 28 | Cleared (xy) by SetupClipped | 7 |
| 29 | A one-qword GIF tag (NLOOP 0, EOP) kicked to wait for the previous clipped polygon | clipper |
| 30–60 | The clipped polygon's GIF packet | clipper |
| 61–100, 101–140 | Polygon buffers A and B (3 qwords per vertex) | clipper |
| 141, 571 | The two halves of the VIF1 double buffer (TOP); ITOP is unused | 2–6 |
| TOP + 402 (543, 973) | The batch header: a GIF packet (TEX0 etc.) sent before the batch's first primitive, once | 2–6 |
| 1006–1023 | Register save area (stack, VI14 = 1024 down) of the clipping path | clipper |

### A batch at TOP

```
TOP+0          GIF tag of the strip: NLOOP = vertex count, EOP, PRE+PRIM (strip), PACKED,
               NREG 3, REGS 0x412 (ST, RGBAQ, XYZF2). Its y word (bits 0-15) and w word carry flags.
TOP+1 ...      vertex records (layout per program)
after them     the output packet: the tag copied, then per vertex  ST(Q) | RGBAQ | XYZF2
TOP+402        batch header packet; non-zero NLOOP = not sent yet
```

Every vertex program sends the header first if its NLOOP is non-zero, then clears that NLOOP
word (whole word written as 16 bits), so a header goes out once however many batches use it.
`Init` clears both halves' headers; `SetupClipped` clears only the 571 half's (VU 973.x).

The PRIM of a strip tag seen in captures is 0x27C: triangle strip, Gouraud, textured, fogged,
alpha-blended, context 2, perspective (STQ) texturing. The clipped polygons go out with the tag's
other PRIM word (entries 3/4: the strip tag's w word, as a float plus 0; entry 6: TOP+1.y),
seen as 0x27D/0x21D: a triangle fan with the same flags.

## Registers between runs

Programs keep their state in VF registers set by the setups:

| Register | SetupLit (1) → entries 2, 5 | Init (0) / SetupClipped (7) → entries 3, 4, 6 |
|---|---|---|
| VF1–4 | world→screen = camera × world (also stored over VU0–3) | clip→screen (VU12–15) |
| VF5–7 | light directions × world rotation | Init: VU16–18; SetupClipped: VU16–18 × world (object rotation for entry 3) |
| VF8 | VU19: fog of entries 2/5/6 (x alpha cap, y max, z scale, w far) | SetupClipped: VU19 (entry 6's fog); Init leaves it. Entry 3 overwrites it with its object origin |
| VF9–11 | light 0, light 1, specular colour (VU20–22) | colour-set weights, fog set A, fog set B (Init: VU19–21, SetupClipped: VU20–22) |
| VF12 | ambient (VU23; w = alpha cap) | entries 3/4 set it to the chosen fog set; entry 6 to VF8 when it clips |
| VF13–16 | world→clip = clip × world | world→clip = clip × world (Init: VU8–11 as is) |
| VF17, VF18 | tints VU24, VU25 | entry 6: previous screen edge and position (culling) |

Which memory slot means what therefore depends on the setup the game pairs with a program; the
game uploads matching data. A full list of what any program reads before writing it (computed
from the code over all paths): VF1–VF18, VF27.w, VF28.w and VI14. The reference functions leave
exactly these as the microcode does; everything else is a temporary.

Two carries end up in packets:

- **LitStrip's STQ.w** is VF28.w: for the first vertex what the previous run left in VF28.w,
  then the previous vertex's XYZF2.w word (its fog × 16). Other runs leave VF28 = the last
  vertex's XYZF2 (2, 5, 6), its RGBA (3, 4), or VU11 (setups 1 and 7).
- **Entries 3/4's STQ.w** is VF27.w, never loaded by them: whatever the previous run left
  (entries 2, 5, 6: the last record's st.w; setups 1 and 7: VU10.w).

The GS ignores STQ.w, so these matter only for byte-exact packets.

## Per-vertex math

Notation: `M·v` is the microcode's chain MULAx, MADDAy, MADDAz, MADDw (each step one FMAC:
rounded toward zero once). `M·(v,1)` uses VF0.w = 1 as the fourth coordinate. `ftoi4(v)` =
trunc(v × 16) saturated to int32; `ftoi0` = trunc. "fog(w, P)" = clamp((P.w − w) × P.z, 0, P.y).
All vertex programs output, per vertex, the PACKED qwords

```
ST    (s·q, t·q, 1·q, carried w)            q = 1/w_screen (DIV, exact as the VU's)
RGBAQ (ftoi0 r, g, b, a)
XYZF2 (ftoi4 x·q, y·q, z·q, ftoi4 fog)      ADC (bit 15 of w) set as described per program
```

### Entry 2: LitStrip (town, menus, 2D)

Record (4 qwords): position, normal, colour (rgb), (s, t, specular weight, –).

- screen = VF1–4 · (p,1); clip = VF13–16 · (p,1).
- d = max(VF5·n.x + VF6·n.y + VF7·n.z, 0) (lanes x, y, z: the lights' N·L).
- lit = VF9·d.x + VF10·d.y + VF12 (ambient); highlight = (VF11 × specular weight) × d.z⁸.
- tint = (1,1,1), or VF18 if the tag's w word is even and non-zero, VF17 if odd.
- rgb = min(colour × tint × lit + highlight, VF8.y); a = min(fog(w, VF8), VF12.w).
- XYZF2 fog = fog(w, VF8); STQ.w carried (above).
- Not clipped: a triangle (from the third vertex) with any vertex outside the clip volume
  (CLIP of the clip-space position) gets ADC, and that vertex's whole XYZF2.w word becomes
  0x8FF0 (fog 0xFF + ADC).

### Entry 5: OffsetStrip

Record (4 qwords): position, –, colour (rgb), (s, t, depth scale, w).

- Weights ω = VU25, rotated one lane per vertex (vertex k uses ω[k mod 4]).
- screen = VU5·ω + VF1–4 · (p,1); clip = VU9·ω + VF13–16 · (p,1) (a per-vertex offset along
  the matrices' second column). VF10/VF11 are left = VU5/VU9, VF17 = VU24, VF18 = rotated ω.
- rgb = colour × VU24 (no clamp: an int beyond 255 wraps in the GS's 8 bits); a = VU24.w ×
  min(max((VF8.w − w) × VF8.z, 0), VF8.x).
- screen z is scaled by the record's st.z after the divide; ST = (s·q, t·q, q, st.w).
- ADC as in entry 2 (0x8FF0).

### Entries 3 and 4: ObjectStrip / WorldStrip (courses, towns, objects)

Record (5 qwords): position, colour set A (rgb), B (x = alpha, y, z), C (rgb),
(s, t, alpha add, –). Entry 4's records start at TOP+1 with world positions (+ VU26.xyz);
entry 3's at TOP+2, with TOP+1 = the object's origin (+ VU26.xyz) and local positions.

- Fog set: VF12 = VF10 + 0, or VF11 if the tag's y word has bit 1.
- clip: entry 4: VF13–16 · (p + VU26, 1); entry 3: VF5·x + VF6·y + VF7·z + VF8, with VF8 =
  VF13–16 · (origin + VU26, 1). screen = VF1–4 · clip (4D).
- colour (weights W = VF9.xyz): r = A.r·Wx + A.r·Wy + C.r·Wz; g, b = A·Wx + B·Wy + C·Wz;
  a = B.x·Wx + B.x·Wy + (VU26.w + st.z)·Wz (exactly in that FMAC order).
- Entry 4: a = min(a, 1); rgb = min(rgb, VF12.y); a ×= min(max((VF11.w − w) × VF11.z, 0), VF11.x)
  (fog set B as an alpha fade). Entry 3: a ×= min(max((VF12.w − w) × VF12.z, 0), VF12.x); no
  clamps.
- XYZF2 fog = fog(w, VF12). ST = (s·q, t·q, q, VF27.w carried).
- Triangle test (from the third vertex): any vertex outside → the clipping path below; the
  vertex gets ADC by OR-ing 0x8000 into its XYZF2.w (the word rewritten as 16 bits).

### Entry 6: SkyStrip

Header TOP+1.y = the clipped polygons' tag word. Record (3 qwords) from TOP+2: position,
colour, (s, t, q scale, w).

- ω = VU25 cycling as in entry 5; clip = VU6·ω + VF13–16 · (p,1); screen = VF1–4 · clip.
- rgb = colour × VU24; a = VU24.w × min(max((VF8.w − w) × VF8.z, 0), VF8.x); fog = fog(w, VF8).
- ST = (s·q, t·q, q_scale·q, st.w): the third coordinate isn't forced to 1.
- Culling: e = screen.xy (after the divide) − previous screen.xy; z = e.x·e'.y − e'.x·e.y with
  e' the previous edge (OPMULA/OPMSUB; the MAC sign flag of z). A triangle whose sign doesn't
  match the strip's alternation (negative expected on even vertices) gets ADC. The previous
  edge and position live in VF17/VF18 across runs, so a strip's first two vertices are tested
  against whatever the last sky run left (harmless: no triangle closes there).
- The clipping test takes precedence: a triangle outside goes to the clipper (with VF12 = VF8)
  and gets ADC whatever its winding.

## The clipping path (entries 3, 4, 6)

The clip space is a guard band: in the captured races and carousel, VF1–4 maps clip x to
512·x/w + ~1983 (|x| ≤ w is ±512 pixels around the centre of a 640-wide screen) and y to
240.64·y/w + ~2003 (±240 lines of a 224-line field), and z to Z = 2²³·(1 − z/w), so −z is the
near side (Z up to 2²⁴) and +z the far side (Z 0). The clip matrix seen there (VU8–11, applied to
positions the EE has already brought into view space) gives z = w − 3: the near plane is at
w = 1.5 and the +z plane is never crossed.

For each closing vertex k, with flags F(k−2), F(k−1), F(k) (CLIP: +x, −x, +y, −y, +z, −z):

1. **Accept** if no vertex is outside: nothing more.
2. **Reject** (ADC only) if all three are beyond the same one of −y, +y, −x, +x, −z, or if
   **any** vertex is beyond +z.
3. **Clip** otherwise, then ADC:
   - Save VF17–30 and VI2–15 (not VI13) at VI14 (1006–1023).
   - Buffer A (61) gets the triangle as (clip position, colour, st) per vertex, closed by a
     copy of the first: the colour is the RGBA just written to the packet, back to float
     (ITOF0); st is the record's raw (s, t, z, w).
   - Six Sutherland–Hodgman passes, in the order −z, +z, −x, +x, −y, +y, ping-ponging A→B→A…
     (the result ends in A). For each edge a→b (a re-normalised by adding 0): inside is the
     CLIP flag of that plane clear. Both in: emit a. a in, b out: emit a and the intersection.
     a out, b in: emit the intersection. Both out: nothing. The polygon is closed again with
     a copy of its first output vertex.
   - Intersection: with s = ∓1, d = v[c] − s·v.w; t = |d_a / (d_b − d_a)| (DIV, then ADD 0 and
     ABS); every lane of position, colour and st = a + (b − a)·t. Interpolated in clip space,
     before the divide, so perspective-correct.
   - After any plane but the first, an empty polygon ends the path (nothing sent). (The first
     can't empty a triangle that wasn't rejected.)
   - Send: write VU29.x = 0x8000 and kick VU29 (an empty EOP packet: waits for the previous
     polygon's transfer), or kick the batch header instead if it hasn't been sent; then the
     tag at 30 (x = 0x8000 | n, y = the fan PRIM word, z = REGS, w = memory as is) and per
     vertex ST (s·q, t·q, q, w = VF12.w − w_screen as float), RGBA (ftoi0 of the interpolated
     floats), XYZF2 (ftoi4 of x·q, y·q, z·q, fog(w, VF12)), no ADC; kick 30; clear the header's
     NLOOP if it went out here.
   - Restore and continue the strip.

The clipper's D bit (a debug break after its first plane) is ignored unless D halts are
enabled; the reference declines clipped programs when they are.

## Float semantics

As the interpreter (`ps2_vu1_exec.inl`): operands normalised (denormal → signed 0, Inf/NaN →
±FLT_MAX), each FMAC rounded toward zero once (`acc + a*b` contracts to one fused operation in
both), results fixed from the exact double (zero keeps the exact sign, overflow → ±FLT_MAX,
underflow → signed 0). MAX/MINI compare normalised operands. FTOIn scales in double and
truncates with saturation. CLIP compares raw bit patterns against |w|. DIV gives ±FLT_MAX for a
zero denominator. `vu1_hle_float.h` has them as one-lane helpers; `run()` sets FE_TOWARDZERO.

Timing: the programs were written so that every read sees the latest write in program order
(Q is read 7+ cycles after its DIV, flags 4+ after CLIP/OPMSUB), so no stale-value effects
appear in the output; the only cross-iteration values are the carries listed above. The one
timing effect the reference copies is XGKICK's: a transfer reads memory as it goes, so the
header NLOOP clear after a batch's kick is visible to a strip packet long enough to reach it
(only with impossible vertex counts).

## The reference implementations

`third_party/PS2Recomp/ps2xRuntime/`:

- `include/runtime/vu/hle/vu1_hle.h`: `ps2x::vu1hle::run(state, vuData, size, startPC, top, kick)`,
  `recognises(code)`, the live registers and scratch ranges.
- `src/lib/vu/hle/vu1_hle.cpp` (dispatch, XGKICK packet walk, entries 0, 1, 7),
  `vu1_hle_strips.cpp` (2, 5), `vu1_hle_clipped.cpp` (3, 4, 6, the clipper),
  `vu1_hle_float.h` (the VU's float operations).

`vu1_replay <capture> --hle` runs them on every record and compares: XGKICK packets byte for
byte, data memory except the register save area (1006–1023), and the live registers.
`--record N` and `--hle-dump` show one record's packets side by side. With `--fuzz N` or
`--as PC` the reference is the interpreter on mutated or reinterpreted inputs.

### Results (13 October 2026)

| Capture | Records | Entries | Mismatches |
|---|---|---|---|
| vu1-lean/caps menus, race, race2, town, town2 | 15,000 | 0, 1, 2, 4, 5 | 0 |
| macos-vu1/work/caps carousel_sparse | 9,987 | 0, 4 | 0 |
| vu1hle-caps qr0_rare, qr2_rare, qr3_rare, qr5_rare, field_rare | 30,000 | 3, 5, 6, 7 (379 runs clipped: 178 of entry 3, 201 of entry 6) | 0 |
| vu1hle-caps qr0_rare_sparse (whole Quick Race session, every 200th) | 1,230 | 3, 5, 6, 7 | 0 |
| vu1hle-caps qr0_world_sparse (every 2000th entry-4 run), qr0_world | 11,463 | 4 (84 clipped) | 0 |
| vu1hle-caps field_lit | 6,000 | 1, 2, 5 | 0 |

Every entry, including the clipper on real data, matches. Under `--as` (every record started
at each entry in turn) all eight entries match on the race, town and menu captures. Under
`--fuzz 2` (10% of the float inputs perturbed or special) the only mismatches are strip
packets so long (fuzzed vertex counts of 60+) that they run into the register save area,
whose contents are the microcode's temporaries; a real batch can't: its input would overflow
the half-buffer first.

New captures (`build/vu1hle-caps/`, lockstep, paraLLEl-GS) were made with
`RT_VU1_CAPTURE_PCS=0x30,0x50,0x60,0x70` (new: only runs starting at those slots count), over
Quick Race (Peach, Night Glow, Snow Mountain, Lagoon) sessions and a drive from Peach Town into
a field area. Entries 3, 5, 6 and 7 all run in those sessions (about 246,000 runs of them
against 6.9 million of entry 4 in one Quick Race session, boot to race).

## Notes for the native renderer

Per vertex, what a vertex shader has to compute (matrices from VF registers at the setup):

1. Position: entries 2/5: screen = W2S·(p,1) (+ axis·ω for 5), clip = W2C·(p,1) (+ axis'·ω).
   Entries 3/4/6: clip = W2C·(p + offset, 1) (entry 3: R·p + origin; entry 6: + VU6·ω),
   screen = C2S·clip.
2. q = 1/screen.w; X, Y, Z = screen.xyz·q (entry 5: Z × st.z); fog = clamp((far − w)·scale, 0, max).
3. Colour/alpha as per program above; ST = (s, t, 1)·q (entry 6: (s, t, st.z)·q).
4. Strip connectivity: tristrips; ADC marks.

What needs integer/bit exactness for golden parity:

- **XY**: the GS takes the low 16 bits of ftoi4(x·q): 12.4 fixed point, **truncated** toward
  zero (not rounded), saturating to 0x7FFFFFFF far off screen (whose low bits, 0xFFFF, the
  GS sees). Draw-kick position = trunc(x·16)/16; the 1/16-pixel snap must truncate.
- **Z**: word >> 4 of ftoi4(z), 24 bits: Z = trunc(z) as an integer. Greater = nearer;
  ZTST/ZTE semantics need the integer, and the River Raceway decal fight (#4) is two layers whose
  integer Z ties or differs by one.
- **Fog**: F = trunc(fog) (0..255), applied by the GS per pixel along Gouraud interpolation.
- **Colours**: ftoi0 truncation; entries 3, 5, 6 don't clamp rgb, so values over 255 wrap
  (the GS keeps 8 bits). Alpha 0x80 = 1.0.
- **Float order**: the VU rounds every FMAC toward zero in the chain order above. A GPU (round
  to nearest, its own fusion) differs in the last bit, which moves a truncated 12.4 XY or an
  integer Z only at boundaries; exact parity needs `SPV_KHR_float_controls` RoundingModeRTZ
  (fp32, where the driver supports it) and the same operation order, or accepting rare
  one-step differences against the hw-GS tolerance.

Rejection rules to copy (the GPU's own clipping does the rest):

- Entries 2 and 5 never clip: a triangle with **any** vertex outside the guard-band clip volume
  (|x|, |y|, |z| > |w| in clip space, CLIP's comparison on raw bits) is not drawn.
- Entries 3, 4, 6: a triangle with a vertex outside is drawn clipped against the guard-band
  volume (equivalent to GPU clipping to that volume, with colour, alpha and st interpolated in
  clip space and fog recomputed at the new vertices) unless all three vertices are beyond the
  same −y, +y, −x, +x or −z plane (nothing to draw anyway) or **any** vertex is beyond +z, in
  which case the whole triangle is dropped. The guard band is wider than the screen, so GPU
  viewport clipping inside it is invisible.
- Entry 6 also culls by winding (strip parity), above.
- The PS2 draws a clipped polygon as a fan right after the strip so far, before the strip's
  later triangles (draw order matters for blending).

## Open questions

- What each program draws in game terms: entry 6 is presumably the sky (per-vertex offsets
  along the camera's z column), entry 5 small semi-transparent quads (shadows?), entry 3 rigid
  objects (cars?). Phase 1.1 (the EE renderers) will name them.
- The meaning of colour set B's x (alpha) and the r channel's double use of set A looks like a
  packing of four channels into three sets; the data decoder (Phase 1.2) will show it.
- Whether any scene crosses the +z plane (dropped triangles) with other cameras.
