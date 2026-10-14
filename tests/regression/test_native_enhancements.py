"""The enhancements on the native draw path (docs/native-enhancements.md): what has to work with
the strip programs drawn by native.vert and no VU1->GS packet path for the 3D.

Always on the hardware GS with native draws. Pictures are compared with the old path only as a dev
oracle (`RT_NATIVE_DRAW=0`, hw tolerance of goldens.hw_verdict); the other checks stand alone.
"""

from __future__ import annotations

import re

import pytest

from rtharness import seconds
from rtharness.adventure import boot_to_main_menu
from test_native_draw import NATIVE, SCENES, declined, log_of, note, quick_race
from test_save_states import first_divergence, load, replay, save

LIVE = {"RT_TIME": "real", "RT_TEST_LIVE": "1"}


def counts(text: str, key: str) -> dict[int, int]:
    """The `key` section of the [native] lines (`with motion:` / `by program:`): entry -> sum over windows."""
    total: dict[int, int] = {}
    for line in text.splitlines():
        if not line.startswith("[native] strips"):
            continue
        m = re.search(re.escape(key) + r"((?: e\d+=\d+(?:\(fb \d+\))?)*)", line)
        if m:
            for e, n in re.findall(r" e(\d+)=(\d+)", m.group(1)):
                total[int(e)] = total.get(int(e), 0) + int(n)
    return total


# ---------------------------------------------------------------------------------------------
# Motion vectors (TAA, GSR 2, ASR, MetalFX, the plugins) and frame generation's shadow frames need
# the object's matrices on every native draw of the 3D, whichever program drew it.

@pytest.mark.parametrize("name", ["quick_race", "town", "course_2", "course_3"])
def test_motion_on_every_program(game_factory, test_data, name):
    """Every program that runs in the scene (lit strips, billboards, terrain, sky, offset strips)
    gives its draws motion matrices; the ones that do not are named (the HUD's draws carry none)."""
    env = {**NATIVE, "RT_HWGS_MOTION": "1"}
    game, play = SCENES[name](game_factory, env, f"nat_enh_motion_{name}")
    play(game)
    text = log_of(game)
    game.close()
    runs, motion = counts(text, "by program:"), counts(text, "with motion:")
    note(test_data, f"motion {name}: runs {runs}, with motion {motion}")
    assert sum(runs.values()) > 0, "no native draws"
    assert not any(declined(text).values()), declined(text)
    for entry, n in runs.items():
        if n < 200:
            continue
        # The HUD's strips (the minimap, entry 5 in towns and races) are not 3D: they carry none.
        floor = 0.5 if entry != 5 else 0.0
        assert motion.get(entry, 0) >= floor * n, f"program {entry}: {motion.get(entry, 0)} of {n} draws carry motion"


# ---------------------------------------------------------------------------------------------
# Save states: the native queue and the model of what a skipped VU1 run leaves must survive a load
# (several, so a leak in the queue shows), the machine and the picture replaying exactly.

def test_save_load_with_native_draws(game_factory, tmp_path, test_data):
    from test_save_states import race
    game = race(game_factory, "nat_enh_save_load", env={**NATIVE, "RT_GS_SSAA": "1"})
    path = tmp_path / "slot1.rtstate"
    save(game, path)
    expected = replay(game)
    for _ in range(4):
        game.step(90, buttons=["circle"], lx=30)
        load(game, path)
        assert first_divergence(expected, replay(game)) is None
    text = log_of(game)
    game.close()
    lines = [l for l in text.splitlines() if l.startswith("[native] strips")]
    note(test_data, "save/load: " + (lines[-1] if lines else "(no native lines)"))
    assert lines, "no native draws"
    assert "ring_full" not in lines[-1], lines[-1]
    assert native_after_loads(lines), "native draws stopped after a load"


def native_after_loads(lines) -> bool:
    last = re.search(r"native (\d+) \(gpu (\d+)", lines[-1])
    return bool(last) and int(last.group(2)) > 0


def test_cross_process_with_native_draws(game_factory, tmp_path):
    """A state saved with native draws loads in a fresh process and plays the same."""
    from test_save_states import race
    path = tmp_path / "slot1.rtstate"
    first = race(game_factory, "nat_enh_cross_a", env={**NATIVE, "RT_GS_SSAA": "1"})
    save(first, path)
    expected = replay(first)
    first.close()
    second = game_factory(name="nat_enh_cross_b", render=True, env={**NATIVE, "RT_GS_SSAA": "1"})
    load(second, path)
    assert first_divergence(expected, replay(second)) is None


# ---------------------------------------------------------------------------------------------
# Frame skip: a skipped frame's native draws are consumed, the frame still counts as 3D for the
# classifier (widescreen verdict, HUD start), and drawing comes back.

def test_frame_skip_with_native_draws(game_factory, test_data):
    env = {**NATIVE, **LIVE, "RT_FRAME_SKIP": "1", "RT_GS_TEST_LOAD_US": "30000", "RT_PERF_LOG": "1",
           "RT_HWGS_MOTION": "1"}
    game = game_factory(env=env, render=True, settings='[display]\naspect = "16:9"\nhud = "edges"\n[general]\nmenu_hint_shown = true')
    boot_to_main_menu(game)
    game.press("down")
    game.run(seconds(2))
    for _ in range(8):
        game.press("cross")
        game.run(seconds(4))
    game.pad("cross")
    game.run(seconds(25))
    wide = game._call("wide")
    text = (game.work_dir / "app.log").read_text(errors="replace")
    game.close()
    skipped = sum(int(n) for n in re.findall(r"skip level \d+ \((\d+) skipped\)", text))
    note(test_data, f"frame skip with native: {skipped} frames skipped, wide {wide}")
    assert skipped > 0, "frame skip never started (the load should make frames late)"
    assert not wide.get("frame_2d"), f"a driving frame was judged 2D while skipping: {wide}"
    lines = [l for l in text.splitlines() if l.startswith("[native] strips")]
    assert lines and "ring_full" not in lines[-1], lines[-1:]
