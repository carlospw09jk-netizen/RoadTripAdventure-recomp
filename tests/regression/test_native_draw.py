"""Native draws (RT_NATIVE_DRAW=1, hardware GS): a scene drawn with the strip programs skipped and
their batches drawn by native.vert, against the same scene drawn the old way; and the reference
arithmetic against the microprogram's own output (RT_NATIVE_DRAW_VERIFY=1).

Reports (in the test output, -s) the fallback counts by reason and the pixel difference. Always on
the hardware GS: the native path exists only there.
"""

from __future__ import annotations

import math
import re
from pathlib import Path

import pytest

from rtharness import goldens, seconds
from rtharness.adventure import FIELDS, GAME_MAP, continue_to_factory, scene
from rtharness.quick_race import COURSES, open_course, start_race
from rtharness.town import TownDriver, TownMap, car_pose, door_quads, finish_dialogue, park_traffic

HW = {"RT_GS_BACKEND": "hw"}
NATIVE = {**HW, "RT_NATIVE_DRAW": "1", "RT_NATIVE_DRAW_LOG": "1"}
VERIFY = {**NATIVE, "RT_NATIVE_DRAW_VERIFY": "1"}


def quick_race(game):
    game.run(seconds(10))
    game.press("start")
    game.run(seconds(3))
    game.press("down")
    game.run(seconds(2))
    for _ in range(8):
        game.press("cross")
        game.run(seconds(4))
    game.pad("cross")
    game.run(seconds(12))


def town(game):
    for _ in range(4):
        game.press("down")
    game.press("cross")
    game.run(seconds(2))
    game.press("cross")
    game.run(seconds(10))
    game.pad("cross")
    game.run(seconds(6))
    game.release()
    game.run(seconds(1))


def start_town(game_factory, env, name):
    edits = {FIELDS["location"]["offset"]: bytes([1]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits, env=env, render=True, name=name)
    continue_to_factory(game)
    return game


def course_race(index):
    """Quick Race on the carousel's `index`th course: the start, then full gas for a while."""

    def play(game):
        open_course(game, index)
        start_race(game)
        game.run(seconds(6))
        game.pad("cross")
        game.run(seconds(14))

    return play


def new_scenes():
    out = {
        "quick_race": lambda gf, env, name: (gf(env=env, render=True, name=name), quick_race),
        "town": lambda gf, env, name: (start_town(gf, env, name), town),
    }
    # Several courses: a roadside course, a night one, snow, a loop (every program draws different things there).
    for index in (0, 2, 3, 5):
        out[f"course_{index}"] = (lambda i: lambda gf, env, name: (gf(env=env, render=True, name=name), course_race(i)))(index)
    return out


SCENES = new_scenes()


def note(test_data, text: str) -> None:
    """Results the run reports (pytest hides a passing test's output under xdist)."""
    print(text)
    with open(test_data / "native_summary.txt", "a") as f:
        f.write(text + "\n")


def log_of(game) -> str:
    game.run(seconds(6))  # (a window of the counts is logged every 5 s)
    return (game.work_dir / "app.log").read_text(errors="replace")


def last(pattern: str, text: str):
    found = re.findall(pattern, text)
    return found[-1] if found else None


@pytest.mark.parametrize("name", list(SCENES))
def test_native_matches_old_path(game_factory, test_data, name):
    game, play = SCENES[name](game_factory, HW, f"native_off_{name}")
    play(game)
    old = game.frame()
    game.close()

    game, play = SCENES[name](game_factory, NATIVE, f"native_on_{name}")
    play(game)
    new = game.frame()
    text = log_of(game)
    native_lines = [l for l in text.splitlines() if l.startswith("[native] strips")]
    print("\n".join(native_lines[-3:]))
    ok, detail, _ = goldens.hw_verdict(old.array(), new.array())
    import numpy as np
    a, b = old.array().astype(np.int16), new.array().astype(np.int16)
    diff = np.abs(a - b).max(axis=2)
    note(test_data, f"{name}: {detail}; exact pixel diff: {np.mean(diff > 0) * 100:.3f}% differ, {np.mean(diff > 16) * 100:.3f}% by more than 16, "
          f"mean {diff.mean():.4f}, max {diff.max()}\n" + "\n".join(native_lines[-2:]))
    assert native_lines, "no native draws were logged"
    assert ok, detail
    # Every strip program is native now: nothing falls back for its entry.
    assert "unknown_entry" not in native_lines[-1], native_lines[-1]


@pytest.mark.parametrize("name", list(SCENES))
def test_native_reference_exact(game_factory, test_data, name):
    """The reference arithmetic is bit for bit the microprogram's: no verify mismatches."""
    game, play = SCENES[name](game_factory, VERIFY, f"native_verify_{name}")
    play(game)
    text = log_of(game)
    pairs = re.findall(r"verify: runs (\d+), mismatching runs (\d+)", text)
    mism = [sum(int(p[0]) for p in pairs), sum(int(p[1]) for p in pairs)]
    note(test_data, f"{name}: verify runs/mismatching runs = {mism}")
    print("\n".join(l for l in text.splitlines() if "[native-verify]" in l))
    assert mism and mism[0] > 0, "nothing was verified"
    assert mism[1] == 0


def field_area(game, options, test_data):
    """Peach Town, then out of town into a field area (the same drive as test_map_overlay's)."""
    continue_to_factory(game)
    park_traffic(game, 1)
    for _ in range(4):
        game.press("down")
    game.press("cross")
    game.run(seconds(2))
    game.press("cross")
    game.run(seconds(12))
    finish_dialogue(game)
    town_map = TownMap.load(1, Path(options.base_data) / "disc", test_data / "maps")
    driver = TownDriver(game, town_map, door_quads(game, 1), town=1, licence=2)
    driver.park_doors()
    driver.settle()
    goal, last = (-900.0, 800.0), car_pose(game)
    for _ in range(40):
        driver.drive_to(goal, radius=8, timeout_vblanks=300, stop_on_dialogue=False)
        p = car_pose(game)
        if math.dist(p[:2], last[:2]) > 400:  # a new map tile: a field area's coordinates
            driver.drive_to((p[0] - 150, p[1]), radius=8, timeout_vblanks=300, stop_on_dialogue=False)
            break
        last = p
    game.release()
    game.run(seconds(4))


def field_game(game_factory, env, name):
    edits = {FIELDS["location"]["offset"]: bytes([1]), FIELDS["licence"]["offset"]: bytes([2])}
    return game_factory(checkpoint="adventure_first_save", progress_edits=edits, env=env, render=True, name=name)


def test_native_field_area_verify(game_factory, options, test_data):
    game = field_game(game_factory, VERIFY, "native_verify_field")
    field_area(game, options, test_data)
    text = log_of(game)
    pairs = re.findall(r"verify: runs (\d+), mismatching runs (\d+)", text)
    mism = [sum(int(p[0]) for p in pairs), sum(int(p[1]) for p in pairs)]
    note(test_data, f"field: verify runs/mismatching runs = {mism}")
    print("\n".join(l for l in text.splitlines() if "[native-verify]" in l))
    assert mism[0] > 0 and mism[1] == 0


def test_native_field_area_matches_old_path(game_factory, options, test_data):
    game = field_game(game_factory, HW, "native_off_field")
    field_area(game, options, test_data)
    old = game.frame()
    game.close()
    game = field_game(game_factory, NATIVE, "native_on_field")
    field_area(game, options, test_data)
    new = game.frame()
    text = log_of(game)
    native_lines = [l for l in text.splitlines() if l.startswith("[native] strips")]
    ok, detail, _ = goldens.hw_verdict(old.array(), new.array())
    import numpy as np
    a, b = old.array().astype(np.int16), new.array().astype(np.int16)
    diff = np.abs(a - b).max(axis=2)
    note(test_data, f"field: {detail}; exact pixel diff: {np.mean(diff > 0) * 100:.3f}% differ, {np.mean(diff > 16) * 100:.3f}% by more than 16\n" + "\n".join(native_lines[-2:]))
    assert native_lines and ok, detail
    assert "unknown_entry" not in native_lines[-1], native_lines[-1]


# ---------------------------------------------------------------------------------------------
# Native draws take part in the configurations that used to decline them (re/native-gen): motion
# vectors (TAA, GSR 2, ASR, MetalFX), the UI mask, and frame generation's shadow frames. Headless,
# the backend is told what a presenter would ask for (RT_HWGS_MOTION, RT_HWGS_UI, RT_HWGS_SHADOWS);
# RT_NATIVE_DECLINE=<reason> puts the old decline of that reason back (A/B).

CONFIGS = {
    "motion": {"RT_HWGS_MOTION": "1"},
    "ui_mask": {"RT_HWGS_UI": "1"},
    "frame_gen": {"RT_HWGS_SHADOWS": "2"},
}
REASONS = ["classifier", "backend", "skipped", "other", "map", "ui_mask", "motion", "frame_gen"]


def declined(text: str) -> dict[str, int]:
    total = {r: 0 for r in REASONS}
    for line in text.splitlines():
        if line.startswith("[native] strips"):
            for reason, n in re.findall(r"declined_(\w+)=(\d+)", line):
                total[reason] = total.get(reason, 0) + int(n)
    return total


def native_count(text: str) -> int:
    return sum(int(n) for n in re.findall(r"\[native\] strips: native (\d+)", text))


@pytest.mark.parametrize("name", list(SCENES))
@pytest.mark.parametrize("config", list(CONFIGS))
def test_native_takes_part(game_factory, name, config):
    """With motion vectors, the UI mask or shadow frames asked for, no strip is declined, and the
    picture is the one the declining (old) path draws."""
    env = {**NATIVE, **CONFIGS[config]}
    game, play = SCENES[name](game_factory, env, f"native_{config}_{name}")
    play(game)
    new = game.frame()
    text = log_of(game)
    after = declined(text)
    taken = native_count(text)
    game.close()
    print(f"{name}/{config}: native {taken}, declined {after}")
    assert taken > 0, "no native draws"
    assert not any(after.values()), f"declined strips: {after}"

    game, play = SCENES[name](game_factory, {**env, "RT_NATIVE_DECLINE": f"{config},classifier"}, f"native_{config}_old_{name}")
    play(game)
    old = game.frame()
    before = declined(log_of(game))
    print(f"{name}/{config} (old declines back): {before}")
    if not (config == "frame_gen" and name == "quick_race"):  # (shadow frames didn't start in the headless race)
        assert before[config] > 0 or before["classifier"] > 0, f"the old path declined nothing here: {before}"
    ok, detail, _ = goldens.hw_verdict(old.array(), new.array())
    print(f"{name}/{config}: {detail}")
    assert ok, detail


# ---------------------------------------------------------------------------------------------
# The map layer (second screen, Enhanced map): the town minimap's batches are native draws too.

def town_map_run(game_factory, options, test_data, env, name):
    from test_map_overlay import ENHANCED, HW_WINDOW, PEACH_TOWN, drive_into_town, map_state, wait_for, window_shot, box_pixels
    edits = {FIELDS["location"]["offset"]: bytes([PEACH_TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits, settings=ENHANCED, render=True,
                        env={**HW_WINDOW, **env, "RT_NATIVE_DRAW_LOG": "1"}, name=name)
    drive_into_town(game, options, test_data)
    m = wait_for(game, lambda m: m["shown"])
    game.run(60)
    m = map_state(game)
    shot = window_shot(game, test_data / "runs" / f"{name}_window.png")
    return game, m, shot


def test_native_map_layer(game_factory, options, test_data):
    """Peach Town with Map = Enhanced: with native draws the map's batches go to the map layer as
    before (same box, same picture), and nothing is declined for them."""
    import numpy as np

    game, new_map, new_shot = town_map_run(game_factory, options, test_data, {"RT_NATIVE_DRAW": "1"}, "native_map_on")
    text = log_of(game)
    after = declined(text)
    print("declined with native map draws:", after)
    assert new_map["dest"] == "overlay" and new_map["shown"] and new_map["kind"] == 1, new_map
    assert not any(after.values()), after
    game.close()

    game, old_map, old_shot = town_map_run(game_factory, options, test_data,
                                           {"RT_NATIVE_DRAW": "1", "RT_NATIVE_DECLINE": "map,ui_mask,classifier"}, "native_map_decl")
    before = declined(log_of(game))
    print("declined with the old declines back:", before)
    assert before["map"] > 0, before
    assert old_map["kind"] == 1
    for key in ("picture", "rect"):
        assert np.allclose(new_map[key], old_map[key], atol=0.01), (key, new_map[key], old_map[key])
    # Our map (drawn from the layer) where the game drew its own, in both windows.
    def crop(m, shot):
        picture, rect = m["picture"], m["rect"]
        sh, sw = shot.shape[:2]
        px0, py0, pw, ph = rect[0] * sw, rect[1] * sh, rect[2] * sw, rect[3] * sh
        return shot[int(py0 + picture[1] * ph):int(py0 + picture[3] * ph), int(px0 + picture[0] * pw):int(px0 + picture[2] * pw)]

    a, b = crop(new_map, new_shot), crop(old_map, old_shot)
    from test_map_overlay import small
    from PIL import Image
    Image.fromarray(a.clip(0, 255).astype(np.uint8)).save(test_data / "runs" / "native_map_ours_native.png")
    Image.fromarray(b.clip(0, 255).astype(np.uint8)).save(test_data / "runs" / "native_map_ours_old.png")
    diff = float(np.abs(small(a) - small(b)).mean())  # (the windows differ by a few pixels in size, the cars by timing)
    print(f"map region difference at a coarse scale: {diff:.3f}")
    assert diff < 6.0, diff


WIDE = '[display]\naspect = "16:9"\nhud = "edges"\n[general]\nmenu_hint_shown = true'


@pytest.mark.parametrize("name", ["quick_race", "town"])
def test_native_widescreen_hud(game_factory, name):
    """Widescreen with the HUD at the screen's edges: the HUD's native draws (the minimap) are placed
    by the same rule as the packet path's, in the shader, and nothing is declined."""
    results = []
    for tag, extra in (("new", {}), ("old", {"RT_NATIVE_DECLINE": "classifier"})):
        env = {**NATIVE, **extra}
        if name == "quick_race":
            game, play = game_factory(settings=WIDE, env=env, render=True, name=f"native_wide_{tag}_{name}"), quick_race
        else:
            game, play = start_town(lambda **kw: game_factory(settings=WIDE, **kw), env, f"native_wide_{tag}_{name}"), town
        play(game)
        frame = game.frame()
        text = log_of(game)
        results.append((frame, declined(text)))
        game.close()
    (new, after), (old, before) = results
    print(f"{name}: declined now {after}, with the old classifier decline {before}")
    assert not any(after.values()), after
    assert before["classifier"] > 0, before
    ok, detail, _ = goldens.hw_verdict(old.array(), new.array())
    print(detail)
    assert ok, detail
