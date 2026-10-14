"""Per-call native records (RT_NATIVE_RECORDS=1, hardware GS; docs/native-records.md): a model call is one record,
not one draw per strip. Checks that the record path draws the same picture as the per-strip native path and that the
record's constants (snapshot of the VU state at the call) equal what every strip's own build saw (RT_NATIVE_RECORDS_VERIFY=1).
"""

from __future__ import annotations

import re

import numpy as np
import pytest

from rtharness import goldens
from test_native_draw import HW, SCENES, log_of, note

CHAINS = {**HW, "RT_NATIVE_DRAW": "1", "RT_NATIVE_DRAW_LOG": "1", "RT_NATIVE_DRAW_LOG_MS": "1000", "RT_NATIVE_CHAINS": "1"}
RECORDS = {**CHAINS, "RT_NATIVE_RECORDS": "1"}
VERIFY = {**CHAINS, "RT_NATIVE_RECORDS_VERIFY": "1"}
SCENE_NAMES = ["quick_race", "town", "course_0"]


def record_lines(text: str) -> list[str]:
    return [l for l in text.splitlines() if l.startswith("[native-records]")]


@pytest.mark.parametrize("name", SCENE_NAMES)
def test_records_match_per_strip(game_factory, test_data, name):
    game, play = SCENES[name](game_factory, CHAINS, f"records_off_{name}")
    play(game)
    old = game.frame()
    game.close()

    game, play = SCENES[name](game_factory, RECORDS, f"records_on_{name}")
    play(game)
    new = game.frame()
    text = log_of(game)
    lines = record_lines(text)
    ok, detail, _ = goldens.hw_verdict(old.array(), new.array())
    a, b = old.array().astype(np.int16), new.array().astype(np.int16)
    diff = np.abs(a - b).max(axis=2)
    note(test_data, f"{name}: records vs per-strip: {detail}; {np.mean(diff > 0) * 100:.3f}% differ, {np.mean(diff > 16) * 100:.3f}% by more than 16\n"
         + "\n".join(lines[-2:]))
    assert lines, "no records were taken"
    assert ok, detail


@pytest.mark.parametrize("name", SCENE_NAMES)
def test_records_constants_exact(game_factory, test_data, name):
    game, play = SCENES[name](game_factory, VERIFY, f"records_verify_{name}")
    play(game)
    text = log_of(game)
    lines = record_lines(text)
    strips = mism = side = 0
    for l in lines:
        m = re.search(r"verify: (\d+) strips, (\d+) mismatches, (\d+) side-effect", l)
        if m:
            strips += int(m.group(1))
            mism += int(m.group(2))
            side += int(m.group(3))
    note(test_data, f"{name}: records verify: strips {strips}, constant mismatches {mism}, side-effect mismatches {side}\n"
         + "\n".join(l for l in text.splitlines() if "[native-records]" in l and ("MISMATCH" in l or "caller" in l or "side" in l))[:2000])
    assert strips > 0, "nothing was verified"
    assert mism == 0
