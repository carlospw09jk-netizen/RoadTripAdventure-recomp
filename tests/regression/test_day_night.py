"""The day/night cycle (docs/game-mechanics/sound-and-daynight.md): the vblank clock at world
+0x14 (config [clock] time_address) and the lighting it drives — updateTimeOfDayLight
(0x225988) blends the day/dusk/night weights, fog and sky colours from the clock every frame:
night weights and short fog (far 300) at midnight, day weights and far fog (800) at noon,
ramping between through dawn and sunset.

test_stamps sets the clock to reach evening behaviour but asserts nothing about it; this file
does. The lighting globals are sound-and-daynight.md's (not yet in game_state.toml).
"""

from __future__ import annotations

from rtharness import seconds
from rtharness.adventure import FIELDS, GAME_MAP, SCENE_TOWN, continue_to_factory, scene
from rtharness.town import park_traffic

PEACH_TOWN = 1
CLOCK = GAME_MAP["clock"]["time_address"]  # 0x335914: vblanks since midnight, 150 per game minute
WRAP = 216000  # one game day of vblanks (an hour of real time)

# updateTimeOfDayLight's outputs (sound-and-daynight.md, "Day/night & lighting"): the three blend
# weights (sceneParams), the fog far distance (300 at night -> 800 by day) and the sky colour.
DAY_WEIGHT, DUSK_WEIGHT, NIGHT_WEIGHT = 0x177F720, 0x177F724, 0x177F728
FOG_FAR = 0x177F73C
SKY_COLOUR = 0x177F754


def open_town(game_factory, name):
    """A game driving around Peach Town, streets quiet (no NPC cars to interrupt the clock)."""
    edits = {FIELDS["location"]["offset"]: bytes([PEACH_TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits, name=name)
    continue_to_factory(game)
    park_traffic(game, PEACH_TOWN)
    for _ in range(4):  # Change parts / Race / Save data / Quit game / Drive around town
        game.press("down")
    game.press("cross")
    game.run(seconds(2))
    game.press("cross")  # dismiss "Come again!"
    game.run(seconds(10))
    assert scene(game) == SCENE_TOWN, "driving in town"
    return game


def set_clock(game, hour: int, minute: int = 0) -> None:
    clock = GAME_MAP["clock"]
    game.write(clock["time_address"], int((hour * 60 + minute) * clock["vblanks_per_minute"]).to_bytes(4, "little"))


def lighting(game) -> dict:
    """The blended lighting of the moment: the three weights, the fog far distance and the sky's
    brightness (the red channel of its colour)."""
    return {"day": game.f32(DAY_WEIGHT), "dusk": game.f32(DUSK_WEIGHT), "night": game.f32(NIGHT_WEIGHT),
            "fog_far": game.f32(FOG_FAR), "sky": game.u32(SKY_COLOUR) & 0xFF}


def test_clock_counts_vblanks_and_wraps(game_factory):
    """In town the clock counts exactly one tick per vblank (150 per game minute, a day of
    216000 per real hour) and wraps past a day back to midnight."""
    game = open_town(game_factory, "day_night_clock")
    for vblanks in (1, 10, seconds(5)):
        start = game.u32(CLOCK)
        game.run(vblanks)
        assert game.u32(CLOCK) - start == vblanks, \
            f"{vblanks} vblanks should tick the clock as many times: {game.u32(CLOCK) - start}"
    game.write(CLOCK, (WRAP - 100).to_bytes(4, "little"))
    game.run(200)
    assert game.u32(CLOCK) == 100, f"the clock should wrap at a day: {game.u32(CLOCK)}"


def test_lighting_follows_the_clock(game_factory):
    """The clock picks the lighting: at midnight the night weight is up with the day weight ~0,
    the fog drawn in close (far ~300) and the sky dark; at noon the day weight is up, the fog far
    out (~800) and the sky bright; at dawn and sunset the blend is mid-ramp between them."""
    game = open_town(game_factory, "day_night_lighting")
    set_clock(game, 0)  # midnight
    game.run(30)
    night = lighting(game)
    assert night["night"] > 0.5 and night["day"] < 0.05, night
    assert 250 < night["fog_far"] < 350, night
    assert night["sky"] < 0x30, night
    set_clock(game, 12)  # noon
    game.run(30)
    day = lighting(game)
    assert day["day"] > 0.5 and day["night"] < 0.05 and day["dusk"] < 0.05, day
    assert 750 < day["fog_far"] < 850, day
    assert day["sky"] > 0xC0, day
    set_clock(game, 6)  # mid-dawn (the 49500-58500 ramp)
    game.run(30)
    dawn = lighting(game)
    assert 0.05 < dawn["day"] < 0.95, dawn
    assert 300 < dawn["fog_far"] < 800, dawn
    set_clock(game, 17, 30)  # mid-sunset (the 153000-162000 ramp)
    game.run(30)
    sunset = lighting(game)
    assert 0.05 < sunset["dusk"] < 0.95, sunset
