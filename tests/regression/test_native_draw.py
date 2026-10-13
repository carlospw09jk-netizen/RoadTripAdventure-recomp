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
