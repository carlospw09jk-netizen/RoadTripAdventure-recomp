"""The game's own car physics state (docs/game-mechanics/car-physics.md): the car object at
0x177AC50 + k*0x270, in a Quick Race on Peach Raceway. The steering angle ramp and speed-gated
self-center, the engine/gearbox's gear and rpm cap, the brake ramp, the odometer's accumulation
of |speed|, the fall speeds of the three body points (grounded, airborne, touchdown) and the
ground contact words' material bits.

test_driving_feel covers the app's control layer on top of the same tick; this file watches the
game's own fields, per the mechanics doc. Addresses not in config/game_state.toml are the doc's.
"""

from __future__ import annotations

import struct

from rtharness import seconds
from rtharness.adventure import GAME_MAP
from rtharness.driver import CARS_BASE
from rtharness.quick_race import open_course, start_race

PEACH_RACEWAY = 0
CAR = CARS_BASE - 0x90   # the car object (0x177AC50): the position lives at +0x90 (= CARS_BASE)
DRIVE_FLAGS = GAME_MAP["game"]["drive_flags"]  # 0x335928: bit 0x04 = can drive

# Car object fields (car-physics.md, "Car state object"; +0x1D0/+0x1D8/+0x1FE/+0x1FF also in
# config/game_state.toml [controls.car_object]).
STEER_ANGLE = CAR + 0x1CE  # s16: steering angle input; ±1 per held frame to ±0x20, self-center ±4
CURVATURE = CAR + 0x1CC    # s16: curvature from the angle, scaled by speed (+0x242) and speed
RPM = CAR + 0x1D0          # u16: rpm/tachometer, capped at 10000
GEAR = CAR + 0x1FF         # u8: gear 1..5, in and out of the engine/gearbox model
BRAKE_RAMP = CAR + 0x1FE   # u8: brake ramp 0..32 over the frames held
SPEED = CAR + 0x1D8        # u32: |speed| (display value)
ODOMETER = CAR + 0x204     # s32: odometer, accumulation of |speed|
FALLS = (CAR + 0x1C0, CAR + 0x1C4, CAR + 0x1C8)      # s32 x3: fall speeds (0 grounded)
CONTACTS = tuple(CAR + o for o in range(0x19C, 0x1B8, 4))  # the 7 contact words
POSITION = CARS_BASE      # +0x90: world position floats (x, y, z)


def s16(b: bytes) -> int:
    return struct.unpack("<h", b)[0]


def u16(b: bytes) -> int:
    return struct.unpack("<H", b)[0]


def u32(b: bytes) -> int:
    return struct.unpack("<I", b)[0]


def s32(b: bytes) -> int:
    return struct.unpack("<i", b)[0]


def to_grid(game):
    """Quick Race on Peach Raceway, standing at the grid once the race is drivable."""
    open_course(game, PEACH_RACEWAY)
    start_race(game)
    end = game.vblank + seconds(30)
    while game.vblank < end and not game.u32(DRIVE_FLAGS) & 0x04:  # 0x04: can drive
        game.run(10)
    assert game.u32(DRIVE_FLAGS) & 0x04, "the race never became drivable"


def test_steering_ramp_self_center_and_release(game_factory):
    """Held, the steering angle walks ±1 per frame to its ±0x20 lock; the self-centering needs
    the car rolling: released at a standstill the angle stays put, released while rolling it
    walks back ±4 per frame to 0. With no steering the curvature is 0. (The steering command
    +0x1D2 of car-physics.md never moved in a Quick Race, standstill or rolling — the damping
    claim did not reproduce; see the survey.)"""
    game = game_factory()
    to_grid(game)
    assert s16(game.read(CURVATURE, 2)) == 0, "no curvature without steering"
    angles = []
    for _ in range(40):  # 32 frames to full lock
        angles.append(s16(game.step(1, [], lx=0, reads=[(STEER_ANGLE, 2)])[0]))
    ramp = [abs(a) for a in angles]
    assert 0x20 in ramp[28:38], f"the angle should reach its ±0x20 lock: {ramp[28:38]}"
    assert ramp[-1] == 0x20, ramp[-1]
    assert all(a <= 0x20 for a in ramp), ramp
    winding = [b - a for a, b in zip(ramp, ramp[1:]) if b < 0x20]
    assert winding and set(winding) == {1}, f"the angle walks ±1 per held frame: {sorted(set(winding))}"

    # Released at a standstill the angle stays put: the self-centering needs the car rolling.
    stayed = [s16(game.step(1, [], reads=[(STEER_ANGLE, 2)])[0]) for _ in range(20)]
    assert all(abs(a) == 0x20 for a in stayed), f"the released angle should stay at a standstill: {stayed}"

    # Rolling: the stick centred, the angle self-centers; held again, it ramps just the same.
    game.step(45, ["cross"])  # creep off the line
    assert s16(game.read(STEER_ANGLE, 2)) == 0, "the angle should self-center while the car rolls"
    held = [s16(game.step(1, ["cross"], lx=0, reads=[(STEER_ANGLE, 2)])[0]) for _ in range(34)]
    rolling = [abs(a) for a in held]
    assert rolling[-1] == 0x20, f"full lock again while rolling: {rolling}"
    winding = [b - a for a, b in zip(rolling, rolling[1:]) if b < 0x20]
    assert winding and set(winding) == {1}, f"the angle walks ±1 per held frame while rolling: {sorted(set(winding))}"
    back = [s16(game.step(1, ["cross"], reads=[(STEER_ANGLE, 2)])[0]) for _ in range(10)]
    eased = [abs(a) for a in back]
    assert 0 in eased, f"released while rolling, the angle should self-center: {back}"
    to_zero = eased[:eased.index(0) + 1]
    assert all(-4 <= b - a <= 0 for a, b in zip(to_zero, to_zero[1:])), \
        f"the self-center walks ±4 per frame: {to_zero}"


def test_gears_up_and_rpm_caps(game_factory):
    """Full gas from the grid: the car starts in gear 1 and works up through the gearbox, each
    upshift one gear at a time near the top of the rev range; the tachometer never passes its
    10000 cap and is alive whenever the car moves."""
    game = game_factory()
    to_grid(game)
    gears, rpms, speeds = [], [], []
    for _ in range(seconds(15)):
        d = game.step(1, ["cross"], reads=[(GEAR, 1), (RPM, 2), (SPEED, 4)])
        gears.append(d[0][0])
        rpms.append(u16(d[1]))
        speeds.append(u32(d[2]))
    assert gears[0] == 1, "the car starts in first gear"
    assert all(1 <= g <= 5 for g in gears), sorted(set(gears))
    assert max(gears) >= 4, f"full gas should work up through the gearbox: {sorted(set(gears))}"
    up = [b - a for a, b in zip(gears, gears[1:]) if b > a]
    assert up and all(u == 1 for u in up), f"upshifts should be one gear at a time: {sorted(set(up))}"
    assert max(rpms) <= 10000, f"the rpm cap: {max(rpms)}"
    assert all(r > 0 for r, s in zip(rpms, speeds) if s > 500), "the tachometer follows the motion"
    shifts = [i for i in range(1, len(gears)) if gears[i] == gears[i - 1] + 1]
    assert shifts, "the gearbox should shift up under full gas"
    assert rpms[shifts[0] - 1] > 5 * rpms[0], f"the tach should climb into the first shift: {rpms[0]} -> {rpms[shifts[0] - 1]}"
    for i in shifts:
        assert rpms[i - 1] > 0.6 * max(rpms), f"shift {i} below the top of the range: {rpms[i - 1]}"


def test_brake_ramp_winds_up_and_down(game_factory):
    """Holding Square from speed, the brake ramp winds up by exactly one per held frame to its
    32-frame top while the car slows; released, it lets go again."""
    game = game_factory()
    to_grid(game)
    for _ in range(seconds(4) // 2):
        game.step(2, ["cross"])  # up to speed
    ramps, speeds = [], []
    for _ in range(48):
        d = game.step(1, ["square"], reads=[(BRAKE_RAMP, 1), (SPEED, 4)])
        ramps.append(d[0][0])
        speeds.append(u32(d[1]))
    assert ramps[0] <= 1, ramps[0]
    assert max(ramps) == 32, f"the ramp should reach its 32-frame top: {max(ramps)}"
    assert all(r <= 32 for r in ramps), ramps
    winding = [b - a for a, b in zip(ramps, ramps[1:]) if b < 32]
    assert winding and set(winding) == {1}, f"one step per held frame: {sorted(set(winding))}"
    assert speeds[-1] < speeds[0], f"braking should slow the car: {speeds[0]} -> {speeds[-1]}"
    after = [game.step(1, [], reads=[(BRAKE_RAMP, 1)])[0][0] for _ in range(30)]
    assert 0 in after, f"released, the ramp should let go: {after}"


def test_odometer_counts_the_distance(game_factory):
    """The odometer accumulates |speed|: over a flat-out run it only grows, and its growth
    matches the per-vblank |speed| added up (within a few frames' read alignment)."""
    game = game_factory()
    to_grid(game)
    speeds, odos = [], []
    for _ in range(seconds(10)):
        d = game.step(1, ["cross"], reads=[(SPEED, 4), (ODOMETER, 4)])
        speeds.append(u32(d[0]))
        odos.append(s32(d[1]))
    travelled = odos[-1] - odos[0]
    assert travelled > 0, "driving should wind the odometer"
    assert all(b >= a for a, b in zip(odos, odos[1:])), "the odometer only accumulates"
    assert abs(travelled - sum(speeds)) <= max(speeds) * 3 + 8, \
        f"the odometer should count |speed|: {travelled} travelled, {sum(speeds)} summed"


def lift(game, units: float) -> None:
    """Puts the car `units` straight up, the way the harness's other car edits move it: the
    position floats (+0x90), the fixed-point word the tick commits into (+0xA0, its scale fitted
    from where the car stands) and the seven world-space points it keeps (+0x110..)."""
    raw = bytearray(game.read(POSITION, 0xF0))
    x, _, _ = struct.unpack_from("<3f", raw, 0)
    fixed_x, _, _ = struct.unpack_from("<3i", raw, 0x10)
    scale = fixed_x / x if x else 1.0
    struct.pack_into("<f", raw, 4, struct.unpack_from("<f", raw, 4)[0] + units)
    struct.pack_into("<i", raw, 0x14, struct.unpack_from("<i", raw, 0x14)[0] + round(units * scale))
    for offset in range(0x80, 0xF0, 0x10):
        struct.pack_into("<f", raw, offset + 4, struct.unpack_from("<f", raw, offset + 4)[0] + units)
    game.write(POSITION, bytes(raw))


def test_airborne_fall_speeds_and_landing(game_factory):
    """Lifted off the ground, all three body points' fall speeds go positive (the airborne
    condition) from the first frame, grow every vblank — at the course's gravity (0x59, observed
    88 per frame) at first, tapered by the tick's quadratic drag as the fall builds — and zero at
    touchdown, the car landing back at the height it started from. (The pattern-1 landing rumble
    is masked for the player's car — car-physics.md's 0xFB0 note — so the pads stay still; the
    fall physics is what is asserted.)"""
    game = game_factory()
    to_grid(game)
    d = game.step(1, [], reads=[(f, 4) for f in FALLS])
    assert (s32(d[0]), s32(d[1]), s32(d[2])) == (0, 0, 0), "grounded at the grid"
    y0 = struct.unpack("<f", game.read(POSITION + 4, 4))[0]
    lift(game, 400.0)
    series, airborne_seen = [], False
    for _ in range(1200):
        d = game.step(1, [], reads=[(f, 4) for f in FALLS] + [(POSITION + 4, 4)])
        series.append((*(s32(x) for x in d[:3]), struct.unpack("<f", d[3])[0]))
        if all(series[-1][:3]):
            airborne_seen = True
        elif airborne_seen:
            break  # touchdown: back on the ground
    assert airborne_seen, f"the lift should leave the car falling: {[f for f, *_ in series[:10]]}"
    airs = [(a, b, c) for a, b, c, _ in series]
    up = [i for i, (a, b, c) in enumerate(airs) if a > 0 and b > 0 and c > 0]
    assert up[0] <= 1, f"off the ground the frame after the lift: {airs[:3]}"
    assert up == list(range(up[0], up[-1] + 1)), "one stretch of airborne frames"
    assert len(up) >= 8, "the car should fall for a while"
    for col in range(3):
        vs = [airs[i][col] for i in up]
        assert all(b > a for a, b in zip(vs, vs[1:])), \
            f"fall speed {col} should grow every vblank while airborne: {vs}"
    growth = [airs[i + 1][0] - airs[i][0] for i in up[:-1]]
    assert all(80 <= g <= 95 for g in growth[:30]), \
        f"the fall should start at the course's gravity (0x59/frame): {growth[:30]}"
    quarter = max(1, len(growth) // 4)
    assert sum(growth[-quarter:]) / quarter < 0.9 * sum(growth[:quarter]) / quarter, \
        f"the quadratic drag should taper the fall: {sum(growth[:quarter]) / quarter:.1f} -> " \
        f"{sum(growth[-quarter:]) / quarter:.1f} per vblank"
    touchdown = up[-1] + 1
    assert airs[touchdown] == (0, 0, 0), \
        f"touchdown zeroes the fall speeds: {airs[touchdown - 1:touchdown + 2]}"
    assert abs(series[touchdown][3] - y0) < 10, \
        f"landed back at the starting height: {series[touchdown][3]:.1f} from {y0:.1f}"


def test_ground_contact_material_bits(game_factory):
    """The contact words' material bits (8-10) name the ground: asphalt (0) under every point at
    the grid, none pushed into a wall (-1); flat out down the first straight the wheels reach
    the grass of the first bend (3), every body point grounded the whole way."""
    game = game_factory()
    to_grid(game)
    words = [u32(game.read(c, 4)) for c in CONTACTS]
    assert all(w != 0xFFFFFFFF for w in words), "no point pushed into a wall at the grid"
    assert all(w >> 8 & 7 == 0 for w in words), f"asphalt under every point: {[hex(w) for w in words]}"
    materials, airborne = [], False
    for _ in range(seconds(16) // 4):
        d = game.step(4, ["cross"], reads=[(f, 4) for f in FALLS] + [(c, 4) for c in CONTACTS])
        airborne |= all(s32(x) > 0 for x in d[:3])  # airborne: all three body points falling
        materials.append(sorted({u32(w) >> 8 & 7 for w in d[3:] if u32(w) != 0xFFFFFFFF}))
    assert not airborne, "the wheels stay on the ground down the straight"
    seen = sorted({m for ms in materials for m in ms})
    assert materials[0] == [0], f"the grid is asphalt: {materials[0]}"
    assert 3 in seen, f"the grass of the first bend should come under the wheels: {seen}"
