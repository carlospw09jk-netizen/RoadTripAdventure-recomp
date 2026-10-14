"""Diagnostic probe (RT_NG_PROBE=1): drive Sandpolis Night Glow A with the bot and log the car's
state around the place where it gets stuck. Not a regression test."""
from __future__ import annotations

import json
import math
import os
import struct

import pytest

from rtharness import seconds
from rtharness.driver import CARS_BASE, Driver, DriverConfig
from rtharness.adventure import (FIELDS, GAME_MAP, SCENE_RACING, SCENE_RACE_OVER, continue_to_factory, enter_race,
                                 scene)

pytestmark = pytest.mark.skipif(not os.environ.get("RT_NG_PROBE"), reason="diagnostic probe")
TOWN = GAME_MAP["towns"]["names"].index("Sandpolis")
RACE = os.environ.get("RT_NG_RACE", "Night Glow Raceway A")


def test_probe(game_factory):
    town_races = GAME_MAP["towns"]["races"][TOWN]
    index = town_races.index(RACE)
    edits = {FIELDS["location"]["offset"]: bytes([TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits)
    continue_to_factory(game)
    enter_race(game, index)
    game.audio()
    driver = Driver(game, DriverConfig(backtrack=RACE in GAME_MAP["bot"]["backtrack"], **json.loads(os.environ.get("RT_NG_CFG", "{}"))))
    start = game.vblank
    while scene(game) != SCENE_RACING:
        game.run(seconds(1))
    out = open(os.environ.get("RT_NG_LOG", "/tmp/ng.log"), "w")
    last_log = 0
    maxv = seconds(int(os.environ.get("RT_NG_SECONDS", "400")))
    while game.vblank - start < maxv:
        driver.drive(2)
        now = game.vblank
        me = driver.positions()[0]
        near = (now - start) % 100 < 2 or driver.recoveries > 0
        if near and now - last_log >= (10 if driver.recoveries else 50):
            last_log = now
            raw = game.read(CARS_BASE - 0x90, 0x270)
            u = lambda o: struct.unpack_from("<I", raw, o)[0]
            f = lambda o: struct.unpack_from("<f", raw, o + 0x90)[0]
            contacts = [hex(u(o)) for o in range(0x19C, 0x1B8, 4)]
            fall = [struct.unpack_from("<i", raw, o)[0] for o in (0x1C0, 0x1C4, 0x1C8)]
            out.write(f"{now-start} pos=({me[0]:.1f},{me[1]:.1f}) y={f(4):.2f} head={driver.heading} idx={driver.index} "
                      f"rec={driver.recoveries} bt={driver.backtracks}/{int(driver.backtracking_until > now)} off={driver.line_distance:.0f} rpm={struct.unpack_from('<H', raw, 0x1D0)[0]} spd={u(0x1D8)} "
                      f"gear={raw[0x1FF]} brake={raw[0x1FE]} contacts={contacts} fall={fall}\n")
            if driver.recoveries and driver.recoveries != getattr(driver, "_dumped", 0):
                driver._dumped = driver.recoveries
                out.write("DUMP " + raw.hex() + "\n")
            out.flush()
        if driver.recoveries >= int(os.environ.get("RT_NG_STOP", "4")) or scene(game) == SCENE_RACE_OVER:
            break
    json.dump({"trail": driver.trail, "lap": driver.lap_vblanks, "leader": driver.leader}, open(os.environ.get("RT_NG_LOG", "/tmp/ng.log") + ".trail.json", "w"))
    out.write(f"end zones={driver.slow_zones} bt={driver.backtracks} rec={driver.recoveries} idx={driver.index}/{len(driver.trail)} scene={scene(game)}\n")
    out.close()
