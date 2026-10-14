"""Course maps with Map = Enhanced: the map is up, its box is the same from frame to frame and sane,
on several courses (the box and placement differ per course: 0x2A4050 in the game)."""

from __future__ import annotations

import pytest

from rtharness import seconds
from rtharness.quick_race import open_course, start_race
from test_map_overlay import ENHANCED, HW_WINDOW, map_state, wait_for, window_shot


@pytest.mark.parametrize("course", [0, 2, 4, 6])
def test_course_map_box_is_steady(game_factory, test_data, course):
    game = game_factory(settings=ENHANCED, render=True, env=HW_WINDOW)
    open_course(game, course)
    start_race(game)
    end = game.vblank + seconds(60)
    while game.vblank < end and not (game.u32(0x335928) & 0x20):  # racing
        game.run(30)
    game.pad("cross")
    m = wait_for(game, lambda m: m["shown"] and m["kind"] == 2, 900)
    game.release()
    assert m["dest"] == "overlay" and m["shown"] and m["kind"] == 2 and m["overlay"], m
    boxes = []
    for _ in range(12):
        game.run(40)
        s = map_state(game)
        assert s["dest"] == "overlay" and s["shown"] and s["overlay"], s
        boxes.append(s["picture"] + s["uv"])
    spread = max(max(b[i] for b in boxes) - min(b[i] for b in boxes) for i in range(8))
    assert spread < 0.02, f"course {course}: the map box moves ({spread:.3f}): {boxes[0]} .. {boxes[-1]}"
    p = boxes[0][:4]
    assert 0 <= p[0] < p[2] <= 1 and 0 <= p[1] < p[3] <= 1, f"box outside the picture: {p}"
    window_shot(game, test_data / "runs" / f"map_audit_course{course}.png")


def test_game_map_for_comparison(game_factory, test_data):
    """The game's own map on course 6 (what ours is compared with by eye: Tin Raceway's clutter is the game's)."""
    game = game_factory(settings=ENHANCED, render=True, env=HW_WINDOW)
    open_course(game, 6)
    start_race(game)
    end = game.vblank + seconds(60)
    while game.vblank < end and not (game.u32(0x335928) & 0x20):
        game.run(30)
    game.pad("cross")
    wait_for(game, lambda m: m["shown"] and m["kind"] == 2, 900)
    game.release()
    game._call("map", enhanced=0)
    game.run(30)
    window_shot(game, test_data / "runs" / "map_audit_course6_game.png")


def test_no_map_in_the_attract_demo(game_factory):
    """The demo's map stays the game's (the router never diverts it) and our overlay stays off."""
    game = game_factory(settings=ENHANCED, render=True, env=HW_WINDOW)
    game.run(seconds(45))
    for _ in range(6):
        game.run(60)
        m = map_state(game)
        assert m["dest"] == "game" and not m["overlay"], m


@pytest.mark.parametrize("hud", ["4:3", "edges"])
def test_town_map_in_a_wide_window(game_factory, options, test_data, hud):
    """16:9 window: the map overlay keeps its shape and sits where the HUD placement puts the game's
    map (4:3 centred: inside the middle 4:3; edges: at the window's left edge)."""
    from rtharness.adventure import FIELDS
    from test_map_overlay import PEACH_TOWN, drive_into_town
    edits = {FIELDS["location"]["offset"]: bytes([PEACH_TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    wide = ENHANCED.replace('map = "enhanced"', f'map = "enhanced"\naspect = "16:9"\nhud = "{hud}"\nwidth = 1600\nheight = 900')
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits, settings=wide, render=True, env=HW_WINDOW)
    drive_into_town(game, options, test_data)
    m = wait_for(game, lambda m: m["shown"] and m["overlay"], 600)
    assert m["dest"] == "overlay" and m["overlay"], m
    shot = window_shot(game, test_data / "runs" / f"map_audit_wide_{hud.replace(':', '')}.png")
    h, w = shot.shape[:2]
    m = map_state(game)
    assert m["overlay"], m
    rect = m["rect"]
    aspect = (m["picture"][2] - m["picture"][0]) * rect[2] * w / ((m["picture"][3] - m["picture"][1]) * rect[3] * h)
    # the game's own map window is 112 x 51 field pixels at 4:3 = about 1.65:1 with its bezel
    assert 1.0 < aspect < 2.4, f"the map keeps its shape ({aspect:.2f}): {m}"
    # (the outer box only grows while a map is up: in 4:3 mode it may reach further left than the narrowed map)
    far = m["picture"][2]
    if hud == "edges":
        assert far < 0.3 and m["picture"][0] < 0.06, f"the map sits at the left edge: {m['picture']}"
    else:
        assert far > 0.3, f"the map stays in the 4:3 middle: {m['picture']}"


def test_no_map_at_the_factory(game_factory):
    """Q's Factory's menu draws no map: nothing of ours is laid over it, and nothing lingers."""
    from rtharness.adventure import continue_to_factory
    game = game_factory(checkpoint="adventure_first_save", settings=ENHANCED, render=True, env=HW_WINDOW)
    continue_to_factory(game)
    game.run(seconds(3))
    for _ in range(4):
        game.run(60)
        m = map_state(game)
        assert not m["overlay"], m
