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
