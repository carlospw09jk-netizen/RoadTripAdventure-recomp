#!/usr/bin/env python3
"""Which of the scenes that call `cos` (0x274810) run? Drive the title/demo, Quick Race's course
carousel and card, a race, and a town drive in the app built by mkprobe.sh and print its
[cos-probe] lines per scenario.

usage: cos_probe.py <probe app binary> <data dir (disc + game)> <checkpoint dir> <out dir>
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests/regression"))
from rtharness import Game, seconds  # noqa: E402
from rtharness.adventure import FIELDS, boot_to_main_menu, continue_to_factory  # noqa: E402
from rtharness.quick_race import open_course, start_race  # noqa: E402
from rtharness.town import finish_dialogue  # noqa: E402

app, data, checkpoint, out = (Path(a) for a in sys.argv[1:5])


def probe_lines(game, seen):
    lines = [ln.strip() for ln in game.log_text().splitlines() if "[cos-probe]" in ln]
    new = [ln for ln in lines if ln not in seen]
    seen.update(new)
    return new


def scenario(name, work, fn, **kw):
    import os
    if os.environ.get("COS_ONLY") and os.environ["COS_ONLY"] not in name:
        return
    with Game(data, out / work, app=app, **kw) as g:
        fn(g)
        print(f"== {name}")
        for ln in probe_lines(g, set()):
            print("  ", ln)
        print("   (none)" if not probe_lines(g, set()) else "")


def attract(g):
    g.run(seconds(100))


def carousel(g):
    open_course(g, 0)   # carousel, then the card
    g.run(seconds(5))
    open_log = probe_lines(g, set())
    print("   after the card:", len(open_log), "lines")
    start_race(g)
    g.run(seconds(40))  # countdown and the race start


def town(g):
    continue_to_factory(g)
    for _ in range(4):
        g.press("down")
    g.press("cross")
    g.run(seconds(2))
    g.press("cross")
    g.run(seconds(12))
    finish_dialogue(g)
    g.run(seconds(20))


def two_player_right_away(g):
    boot_to_main_menu(g)
    g.press("down")
    g.press("down")
    g.press("cross")
    g.run(seconds(3))
    for _ in range(3):  # Race Right-Away, the first event's card, start
        g.press("cross")
        g.run(seconds(3))
    g.run(seconds(6))


def two_player_saved(g, item=1):
    boot_to_main_menu(g)
    g.press("down")
    g.press("down")
    g.press("cross")
    g.run(seconds(3))
    for _ in range(item):
        g.press("down")
    for _ in range(5):  # prompt, loaded, carousel (Trade Items, Change Parts, Save), card, start
        g.press("cross")
        g.run(seconds(4))
    g.run(seconds(6))


scenario("title + attract demo (100 s)", "w_attract", attract)
scenario("2 Player: Race Right-Away", "w_2p", two_player_right_away)
scenario("2 Player: Random Race (saved cars; carousel with Change Parts)", "w_2p_saved", two_player_saved,
         checkpoint=checkpoint, card2=checkpoint)
scenario("Quick Race carousel, card and race start", "w_carousel", carousel)
scenario("adventure: factory then driving in town", "w_town", town, checkpoint=checkpoint,
         progress_edits={FIELDS["location"]["offset"]: bytes([8]), FIELDS["licence"]["offset"]: bytes([2])})
