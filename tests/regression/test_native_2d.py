"""Native 2D draws (RT_NATIVE_2D, hardware GS): the game's HUD, menus, text and fades as draw records
(gs_native_2d.h), against the same scene drawn from GIF packets (RT_NATIVE_2D=0), and the records
against what the packet path draws (RT_NATIVE_2D_VERIFY=1).

Reports (in the test output, -s, and tests' native2d_summary.txt) the `[native2d]` counters per scene:
payloads taken as records and, by reason, the payloads that went on as packets. Always on the
hardware GS.
"""

from __future__ import annotations

import re

import pytest

from rtharness import goldens, seconds
from rtharness.adventure import FIELDS, boot_to_main_menu, continue_to_factory
from rtharness.quick_race import open_course, start_race

OLD = {"RT_GS_BACKEND": "hw", "RT_NATIVE_2D": "0"}
NEW = {"RT_GS_BACKEND": "hw", "RT_NATIVE_2D": "1", "RT_NATIVE_DRAW_LOG": "1"}
VERIFY = {**NEW, "RT_NATIVE_2D_VERIFY": "1"}


def title(game):
    game.run(seconds(8))


def main_menu(game):
    boot_to_main_menu(game)


def game_options(game):
    # the game's own Title > Options pages (ours replaces them unless RT_GAME_OPTIONS=1)
    boot_to_main_menu(game)
    for _ in range(3):
        game.press("down")
    game.press("cross")
    game.run(seconds(3))
    game.press("down")
    game.run(seconds(1))


def carousel(game):
    open_course(game, 0)


def carousel_card(game):
    open_course(game, 3)
    game.run(seconds(2))


def quick_race(game):
    open_course(game, 0)
    start_race(game)
    game.run(seconds(6))
    game.pad("cross")
    game.run(seconds(10))


def town(game):
    continue_to_factory(game)
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


def town_start_menu(game):
    town(game)
    game.press("start")
    game.run(seconds(2))
    game.press("right")
    game.run(seconds(1))


def factory(game):
    continue_to_factory(game)
    game.run(seconds(2))


SCENES = {
    "title": (title, False),
    "main_menu": (main_menu, False),
    "game_options": (game_options, False),
    "carousel": (carousel, False),
    "carousel_card": (carousel_card, False),
    "quick_race": (quick_race, False),
    "factory": (factory, True),
    "town": (town, True),
    "town_start_menu": (town_start_menu, True),
}


def make(game_factory, env, name, adventure):
    kwargs = dict(env={**env, "RT_GAME_OPTIONS": "1"}, render=True, name=name)
    if adventure:
        kwargs.update(checkpoint="adventure_first_save",
                      progress_edits={FIELDS["location"]["offset"]: bytes([1]), FIELDS["licence"]["offset"]: bytes([2])})
    return game_factory(**kwargs)


def note(test_data, text: str) -> None:
    print(text)
    with open(test_data / "native2d_summary.txt", "a") as f:
        f.write(text + "\n")


def counters(game) -> tuple[str, dict]:
    game.run(seconds(6))  # (a window of the counts is logged every 5 s)
    text = (game.work_dir / "app.log").read_text(errors="replace")
    lines = [l for l in text.splitlines() if l.startswith("[native2d]")]
    total = {"payloads": 0, "native": 0, "draws": 0, "primitives": 0, "fallback": 0, "mismatches": 0, "verified": 0}
    reasons: dict[str, int] = {}
    for l in lines:
        m = re.match(r"\[native2d\] payloads (\d+): native (\d+) \(draws (\d+), primitives (\d+)", l)
        if m:
            for k, v in zip(("payloads", "native", "draws", "primitives"), m.groups()):
                total[k] += int(v)
        m = re.search(r"total=(\d+)", l)
        if m:
            total["fallback"] += int(m.group(1))
        for r, n in re.findall(r"(\w+)=(\d+)(?= |$)", l.split("fallback:")[-1].split(" total=")[0]):
            reasons[r] = reasons.get(r, 0) + int(n)
        m = re.search(r"verify: payloads (\d+), mismatches (\d+)", l)
        if m:
            total["verified"] += int(m.group(1))
            total["mismatches"] += int(m.group(2))
    total["reasons"] = reasons
    return "\n".join(lines[-2:]), total


@pytest.mark.parametrize("name", list(SCENES))
def test_native_2d_matches_packets(game_factory, test_data, name):
    play, adventure = SCENES[name]
    game = make(game_factory, OLD, f"n2d_off_{name}", adventure)
    play(game)
    old = game.frame()
    game.close()

    game = make(game_factory, NEW, f"n2d_on_{name}", adventure)
    play(game)
    new = game.frame()
    last, total = counters(game)
    ok, detail, _ = goldens.hw_verdict(old.array(), new.array())
    import numpy as np
    a, b = old.array().astype(np.int16), new.array().astype(np.int16)
    diff = np.abs(a - b).max(axis=2)
    note(test_data, f"{name}: {detail}; exact pixel diff: {np.mean(diff > 0) * 100:.3f}% differ, "
                    f"{np.mean(diff > 16) * 100:.3f}% by more than 16; payloads {total['payloads']}, native {total['native']}, "
                    f"fallback {total['fallback']} {total['reasons']}\n{last}")
    assert total["payloads"] > 0, "no 2D payloads were offered"
    assert total["native"] > 0, "nothing was taken as records"
    assert ok, detail


@pytest.mark.parametrize("name", list(SCENES))
def test_native_2d_verify(game_factory, test_data, name):
    """The records are the packet path's draws: no mismatches (vertices, state, class)."""
    play, adventure = SCENES[name]
    game = make(game_factory, VERIFY, f"n2d_verify_{name}", adventure)
    play(game)
    last, total = counters(game)
    text = (game.work_dir / "app.log").read_text(errors="replace")
    mism = [l for l in text.splitlines() if l.startswith("[native2d] MISMATCH") or l.startswith("   v")]
    note(test_data, f"{name}: verified {total['verified']} payloads, {total['mismatches']} mismatching; fallback {total['fallback']} "
                    f"{total['reasons']}\n" + "\n".join(mism[:8]))
    assert total["verified"] > 0, "nothing was verified"
    assert total["mismatches"] == 0
