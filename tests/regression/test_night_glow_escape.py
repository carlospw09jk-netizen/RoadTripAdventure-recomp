"""Diagnostic (RT_NG_PROBE=1): drive Sandpolis Night Glow A with the bot to the point where the car
slides along the wall at the first right-hander (about (575, 665)), save the state there and try
human-like inputs from it. Prints what each one does. Not a regression test."""
from __future__ import annotations

import math
import os
import struct

import pytest

from rtharness import seconds
from rtharness.driver import CARS_BASE, Driver, DriverConfig
from rtharness.adventure import FIELDS, GAME_MAP, SCENE_RACING, continue_to_factory, enter_race, scene

pytestmark = pytest.mark.skipif(not os.environ.get("RT_NG_PROBE"), reason="diagnostic probe")
TOWN = GAME_MAP["towns"]["names"].index("Sandpolis")
RACE = os.environ.get("RT_NG_RACE", "Night Glow Raceway A")
OUT = os.environ.get("RT_NG_OUT", "/private/tmp/claude-501/ng_exp")

EXPERIMENTS = {
    "reverse_long": [(200, ["circle"], 128)],
    "reverse_long_then_gas_right": [(200, ["circle"], 128), (120, ["cross"], 255)],
    "reverse_long_left_then_gas_right": [(200, ["circle"], 0), (120, ["cross"], 255)],
    "reverse_long_right_then_gas_right": [(200, ["circle"], 255), (120, ["cross"], 255)],
    "gas_right": [(120, ["cross"], 255)],
    "gas_half_right": [(120, ["cross"], 200)],
    "gas_straight": [(120, ["cross"], 128)],
    "gas_left": [(120, ["cross"], 0)],
    "reverse_straight_then_gas_right": [(40, ["circle"], 128), (120, ["cross"], 255)],
    "reverse_left_then_gas_right": [(40, ["circle"], 0), (120, ["cross"], 255)],
    "reverse_right_then_gas_left": [(40, ["circle"], 255), (120, ["cross"], 0)],
    "brake_then_gas_right": [(30, ["square"], 128), (150, ["cross"], 255)],
}


def state(game):
    raw = game.read(CARS_BASE - 0x90, 0x270)
    x, _, z = struct.unpack_from("<3f", raw, 0x90)
    fx, _, fz = struct.unpack_from("<3f", raw, 0x90 + 0xB0)
    rx, _, rz = struct.unpack_from("<3f", raw, 0x90 + 0xD0)
    contacts = [struct.unpack_from("<I", raw, o)[0] for o in range(0x19C, 0x1B8, 4)]
    return (x, z), math.atan2(fz - rz, fx - rx), struct.unpack_from("<I", raw, 0x1D8)[0], contacts


def test_escape(game_factory):
    town_races = GAME_MAP["towns"]["races"][TOWN]
    edits = {FIELDS["location"]["offset"]: bytes([TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits)
    continue_to_factory(game)
    enter_race(game, town_races.index(RACE))
    driver = Driver(game, DriverConfig(backtrack=RACE in GAME_MAP["bot"]["backtrack"]))
    while scene(game) != SCENE_RACING:
        game.run(seconds(1))
    trigger = (float(os.environ.get("RT_NG_X", "575")), float(os.environ.get("RT_NG_Z", "672")))
    for _ in range(4000):
        driver.drive(2)
        if driver.index > 100 and math.dist(driver.positions()[0], trigger) < 4:
            break
    else:
        pytest.fail("the car never got to the trigger point")
    game.release()
    os.makedirs(OUT, exist_ok=True)
    game.frame().image().save(f"{OUT}/at_trigger.png")
    path = game.work_dir / "ng.rtstate"
    r = game._call("save_state", path=str(path), max_vblanks=600)
    assert r["written"], r
    report = []
    for name, plan in EXPERIMENTS.items():
        r = game._call("load_state", path=str(path), max_vblanks=600)
        assert r.get("ok"), r
        pos0, head0, _, _ = state(game)
        line = [f"{name}: start {pos0[0]:.1f},{pos0[1]:.1f} head {head0:.2f}"]
        for vbl, buttons, lx in plan:
            for _ in range(vbl // 20):
                game.step(20, buttons, lx)
                pos, head, spd, contacts = state(game)
                line.append(f"({pos[0]:.1f},{pos[1]:.1f} h{head:.2f} s{spd} c{sum(1 for c in contacts if c == 0xFFFFFFFF)})")
        game.frame().image().save(f"{OUT}/{name}.png")
        report.append(" ".join(line))
    open(f"{OUT}/report.txt", "w").write("\n".join(report) + "\n")
    print("\n".join(report))


def test_shots(game_factory):
    """Pictures of the bot's approach to the first right-hander (RT_NG_SHOTS=<dir>)."""
    out = os.environ.get("RT_NG_SHOTS")
    if not out:
        pytest.skip("RT_NG_SHOTS not set")
    os.makedirs(out, exist_ok=True)
    town_races = GAME_MAP["towns"]["races"][TOWN]
    edits = {FIELDS["location"]["offset"]: bytes([TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits)
    continue_to_factory(game)
    enter_race(game, town_races.index(RACE))
    driver = Driver(game, DriverConfig(backtrack=RACE in GAME_MAP["bot"]["backtrack"]))
    while scene(game) != SCENE_RACING:
        game.run(seconds(1))
    shots = 0
    for _ in range(6000):
        driver.drive(2)
        me = driver.positions()[0]
        if driver.index >= 112 and shots < 24 and (game.vblank % 10) < 2:
            game.frame().image().save(f"{out}/s{shots:02d}_{me[0]:.0f}_{me[1]:.0f}.png")
            shots += 1
        if shots >= 24:
            break


def test_accel(game_factory):
    """Same input in both builds: full gas from the start (RT_NG_ACCEL=<file>), then full-left steer
    from vblank 360, a straight stretch, and a brake."""
    out = os.environ.get("RT_NG_ACCEL")
    if not out:
        pytest.skip("RT_NG_ACCEL not set")
    town_races = GAME_MAP["towns"]["races"][TOWN]
    edits = {FIELDS["location"]["offset"]: bytes([TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits)
    continue_to_factory(game)
    enter_race(game, town_races.index(RACE))
    while scene(game) != SCENE_RACING:
        game.run(seconds(1))
    lines = []
    plan = [(["cross"], 128)] * 30 + [(["cross"], 0)] * 10 + [(["cross"], 128)] * 10 + [(["square"], 128)] * 8
    for k, (buttons, lx) in enumerate(plan):
        game.step(20, buttons, lx)
        pos, head, spd, contacts = state(game)
        lines.append(f"{k*20} {'+'.join(buttons)} lx{lx} pos=({pos[0]:.2f},{pos[1]:.2f}) head={head:.3f} spd={spd}")
    open(out, "w").write("\n".join(lines) + "\n")
