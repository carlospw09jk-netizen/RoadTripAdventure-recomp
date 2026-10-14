"""The game's map and where it goes (src/ui/MapRouter.h, MapView.h; the hardware GS's map layer).

With Map = Enhanced and no second screen, the hardware GS draws the game's map into a layer of
its own and the app draws it in the game's spot (sharp, with places marked): the GS picture has
no map there any more, the window does. The router asks for the map by the GS's own signal (game
frames that drew it), so field areas count as towns do (#15), and so do races.

These run with a window (the router runs with the UI) on the hardware GS, whatever the suite's
renderer: paraLLEl-GS can't draw the map apart.
"""

from __future__ import annotations

import math
import time
from pathlib import Path

import numpy as np
import pytest

from rtharness import seconds
from rtharness.adventure import FIELDS, continue_to_factory, scene
from rtharness.quick_race import open_course, start_race
from rtharness.town import TownDriver, TownMap, car_pose, door_quads, finish_dialogue, park_traffic

PEACH_TOWN = 1
ENHANCED = '[display]\nmap = "enhanced"\n[general]\nmenu_hint_shown = true\nsecond_screen = false'
HW_WINDOW = {"RT_GS_BACKEND": "hw", "RT_HEADLESS": "0", "RT_SECOND_DEBUG": "1"}


def map_state(game) -> dict:
    return game._call("map")


def wait_for(game, predicate, vblanks: int = 600) -> dict:
    end = game.vblank + vblanks
    m = map_state(game)
    while game.vblank < end and not predicate(m):
        game.run(5)
        m = map_state(game)
    return m


def drive_into_town(game, options, test_data) -> TownDriver:
    continue_to_factory(game)
    park_traffic(game, PEACH_TOWN)
    for _ in range(4):  # Change parts / Race / Save data / Quit game / Drive around town
        game.press("down")
    game.press("cross")
    game.run(seconds(2))
    game.press("cross")  # "Come again!"
    game.run(seconds(12))
    finish_dialogue(game)
    town_map = TownMap.load(PEACH_TOWN, Path(options.base_data) / "disc", test_data / "maps")
    driver = TownDriver(game, town_map, door_quads(game, PEACH_TOWN), town=PEACH_TOWN, licence=2)
    driver.park_doors()
    driver.settle()
    return driver


def box_pixels(frame, picture) -> np.ndarray:
    """The published map box (fractions of the GS picture) cut out of a GS frame."""
    a = frame.array().astype(np.float32)
    h, w = a.shape[:2]
    x0, y0, x1, y1 = (int(round(picture[i] * (w if i % 2 == 0 else h))) for i in range(4))
    return a[max(0, y0):min(h, y1), max(0, x0):min(w, x1), :3]


def window_shot(game, path: Path):
    """The window as presented (with our UI): the next frame the presenter draws."""
    if path.exists():
        path.unlink()
    map_state(game)
    game._call("map", shot=str(path))
    end = time.time() + 20
    while time.time() < end and not path.exists():
        game.run(1)
    time.sleep(0.5)  # encoded on another thread
    from PIL import Image
    return np.asarray(Image.open(path).convert("RGB")).astype(np.float32)


def small(a: np.ndarray, size=(48, 24)) -> np.ndarray:
    from PIL import Image
    return np.asarray(Image.fromarray(a.clip(0, 255).astype(np.uint8)).resize(size, Image.BILINEAR)).astype(np.float32)


def test_enhanced_map_replaces_the_games_in_town(game_factory, options, test_data):
    """Peach Town with Map = Enhanced: the GS picture has no map at its spot, our map is there in
    the window (it looks like the game's map at that spot), and Map = The game's puts the game's
    back at once."""
    edits = {FIELDS["location"]["offset"]: bytes([PEACH_TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits, settings=ENHANCED, render=True,
                        env=HW_WINDOW)
    drive_into_town(game, options, test_data)
    m = wait_for(game, lambda m: m["shown"])
    assert m["dest"] == "overlay" and m["shown"] and m["up"], m
    assert m["kind"] == 1, f"the town's window map: {m}"
    assert m["overlay"], f"drawn on the main screen: {m}"
    picture = m["picture"]
    assert 0.0 <= picture[0] < picture[2] <= 0.5 and 0.5 <= picture[1] < picture[3] <= 1.0, \
        f"the map's spot is the game's (bottom left): {picture}"
    serial = m["serial"]
    game.run(60)
    m = map_state(game)
    assert m["serial"] >= serial + 25, f"a map published every frame (60 vblanks: {m['serial'] - serial})"
    without = box_pixels(game.frame(), picture)

    shot = window_shot(game, test_data / "runs" / "map_overlay_window.png")
    rect = m["rect"]
    sh, sw = shot.shape[:2]
    px0, py0 = rect[0] * sw, rect[1] * sh
    pw, ph = rect[2] * sw, rect[3] * sh
    ours = shot[int(py0 + picture[1] * ph):int(py0 + picture[3] * ph), int(px0 + picture[0] * pw):int(px0 + picture[2] * pw)]

    game._call("map", enhanced=0)
    m = wait_for(game, lambda m: m["dest"] == "game" and not m["latched"], 60)
    assert m["dest"] == "game" and not m["shown"], f"the game's map back at once: {m}"
    game.run(10)
    games = box_pixels(game.frame(), picture)
    assert games.shape == without.shape
    gone = float(np.abs(games - without).mean())
    assert gone > 12, f"the game's map left the GS picture with Enhanced on (difference {gone:.1f})"
    # Our map is the game's, sharper: alike at a coarse scale (the marks differ a little).
    from PIL import Image
    Image.fromarray(ours.clip(0, 255).astype(np.uint8)).save(test_data / "runs" / "map_overlay_ours.png")
    Image.fromarray(games.clip(0, 255).astype(np.uint8)).save(test_data / "runs" / "map_overlay_games.png")
    alike = float(np.abs(small(ours) - small(games)).mean())
    assert alike < 28, f"our map looks like the game's at its spot (difference {alike:.1f})"


def test_field_area_map_is_routed(game_factory, options, test_data):
    """#15: driving out of Peach Town into a field area, the map stays ours (it was the game's
    flags, town handler and drivable, that sent it back; the GS's own signal counts fields)."""
    edits = {FIELDS["location"]["offset"]: bytes([PEACH_TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits, settings=ENHANCED, render=True,
                        env=HW_WINDOW)
    driver = drive_into_town(game, options, test_data)
    town_tile = map_state(game)["state"]
    goal, last = (-900.0, 800.0), car_pose(game)
    crossed = False
    for _ in range(40):
        driver.drive_to(goal, radius=8, timeout_vblanks=300, stop_on_dialogue=False)
        p = car_pose(game)
        if math.dist(p[:2], last[:2]) > 400:  # a new map tile: the field area's coordinates
            crossed = True
            driver.drive_to((p[0] - 150, p[1]), radius=8, timeout_vblanks=300, stop_on_dialogue=False)
            break
        last = p
    assert crossed, "drove out of town"
    game.release()
    game.run(120)
    # (our map stays off 2D frames: a tile change's loading frames can come just before)
    m = wait_for(game, lambda m: m["shown"] and m["overlay"], 300)
    assert m["state"] != town_tile, f"in another tile: {m['state']}"
    assert m["dest"] == "overlay" and m["shown"] and m["up"] and m["overlay"], \
        f"the field area's map is ours: {m} wide {game._call('wide')}"


def test_race_map_is_routed(game_factory):
    """A Quick Race with Map = Enhanced: the course map goes the same way (kind 2, no bezel)."""
    game = game_factory(settings=ENHANCED, render=True, env=HW_WINDOW)
    open_course(game, 0)
    start_race(game)
    end = game.vblank + seconds(60)
    while game.vblank < end and not (game.u32(0x335928) & 0x20):  # racing (drive_flags 0x20, GameStats.cpp)
        game.run(30)
    game.pad("cross")
    m = wait_for(game, lambda m: m["shown"] and m["kind"] == 2, 900)
    game.release()
    assert m["dest"] == "overlay" and m["shown"] and m["kind"] == 2 and m["overlay"], m
