"""Native draws (RT_NATIVE_DRAW=1, hardware GS): a scene drawn with the strip programs skipped and
their batches drawn by native.vert, against the same scene drawn the old way; and the reference
arithmetic against the microprogram's own output (RT_NATIVE_DRAW_VERIFY=1).

Reports (in the test output, -s) the fallback counts by reason and the pixel difference. Always on
the hardware GS: the native path exists only there.
"""

from __future__ import annotations

import re

import pytest

from rtharness import goldens, seconds
from rtharness.adventure import FIELDS, GAME_MAP, continue_to_factory, scene

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


SCENES = {
    "quick_race": lambda gf, env, name: (gf(env=env, render=True, name=name), quick_race),
    "town": lambda gf, env, name: (start_town(gf, env, name), town),
}


def log_of(game) -> str:
    game.run(seconds(6))  # (a window of the counts is logged every 5 s)
    return (game.work_dir / "app.log").read_text(errors="replace")


def last(pattern: str, text: str):
    found = re.findall(pattern, text)
    return found[-1] if found else None


@pytest.mark.parametrize("name", list(SCENES))
def test_native_matches_old_path(game_factory, name):
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
    print(f"{name}: {detail}; exact pixel diff: {np.mean(diff > 0) * 100:.3f}% differ, {np.mean(diff > 16) * 100:.3f}% by more than 16, "
          f"mean {diff.mean():.4f}, max {diff.max()}")
    assert native_lines, "no native draws were logged"
    assert ok, detail


@pytest.mark.parametrize("name", list(SCENES))
def test_native_reference_exact(game_factory, name):
    """The reference arithmetic is bit for bit the microprogram's: no verify mismatches."""
    game, play = SCENES[name](game_factory, VERIFY, f"native_verify_{name}")
    play(game)
    text = log_of(game)
    pairs = re.findall(r"verify: runs (\d+), mismatching runs (\d+)", text)
    mism = [sum(int(p[0]) for p in pairs), sum(int(p[1]) for p in pairs)]
    print(f"{name}: verify runs/mismatching runs = {mism}")
    print("\n".join(l for l in text.splitlines() if "[native-verify]" in l))
    assert mism and mism[0] > 0, "nothing was verified"
    assert mism[1] == 0


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
