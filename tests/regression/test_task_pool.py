"""The task scheduler (docs/game-mechanics/task-scheduler.md; config [tasks]): 48 slots of
0x30 bytes at gs_tasks_pool, walked once per vblank by gs_tasks_walker — every live task's
handler runs and its frame counter (+0x8) counts up. Scenes spawn and kill tasks (the town's
Pause menu runs the Pause list's page handler while it is up) while the pool's tree links stay
consistent and its handlers stay code addresses.

Everything the game does is a task; a broken scheduler is invisible to the goldens, so the
pool is asserted directly, between exact vblanks.
"""

from __future__ import annotations

import struct

from rtharness import seconds
from rtharness.adventure import FIELDS, GAME_MAP, SCENE_TOWN, boot_to_main_menu, continue_to_factory, scene
from rtharness.town import park_traffic

POOL = GAME_MAP["tasks"]["pool"]  # 0x177A0D0: 48 slots x 0x30 bytes
ITERATION_NODE = POOL + 0x30  # 0x177A100: the walker's own first-iteration pointer (task-scheduler.md's
                             # globals table) — pool memory that parses as a live slot but is not a task
SLOTS, SLOT_SIZE = 48, 0x30
# The ELF's code: functions.tsv covers 0x200000-0x26E000 plus the 0x27-0x28 low-level libs.
TEXT_FIRST, TEXT_LAST = 0x200000, 0x2A0000
PEACH_TOWN = 1
PAUSE_STATE = GAME_MAP["options"]["pause_state"]   # +0 is the page shown (0 the list)


def pool(game) -> list[dict]:
    """The 48 slots: live flag, frame counter, handler, child/sibling/parent links."""
    raw = game.read(POOL, SLOTS * SLOT_SIZE)
    slots = []
    for i in range(SLOTS):
        b = i * SLOT_SIZE
        slots.append({
            "at": POOL + b,
            "active": raw[b] != 0,
            "ticks": struct.unpack_from("<I", raw, b + 8)[0],
            "handler": struct.unpack_from("<I", raw, b + 0xC)[0],
            "child": struct.unpack_from("<I", raw, b + 0x10)[0],
            "sibling": struct.unpack_from("<I", raw, b + 0x14)[0],
            "parent": struct.unpack_from("<I", raw, b + 0x18)[0],
        })
    return slots


def live(slots) -> list[dict]:
    return [s for s in slots if s["active"]]


def handlers(slots) -> set[int]:
    return {s["handler"] for s in live(slots)}


def check_pool(game, where: str) -> list[dict]:
    """Asserts the pool's structural rules and returns its slots: some task is always live,
    a live task's handler is a code address, its tree links are pool addresses (or 0) and its
    parent and children are live too (killing a task tears down its subtree)."""
    slots = pool(game)
    active = live(slots)
    assert 0 < len(active) <= SLOTS, f"{where}: {len(active)} live tasks"
    at = {s["at"] for s in active}
    for s in active:
        assert TEXT_FIRST <= s["handler"] < TEXT_LAST, f"{where}: handler {s['handler']:#x} is not code"
        for key in ("child", "sibling", "parent"):
            p = s[key]
            assert p == 0 or POOL <= p < POOL + SLOTS * SLOT_SIZE, f"{where}: {key} {p:#x} is not a slot"
        if s["parent"]:
            assert s["parent"] in at, f"{where}: a live task's parent is dead"
        if s["child"]:
            assert s["child"] in at, f"{where}: a live task's child is dead"
    return slots


def ticks_advance(before: list[dict], after: list[dict], vblanks: int, where: str) -> None:
    """Every task live in both snapshots ticked exactly once per vblank in between (the walker's
    own iteration node aside — the one pool address that is not a task)."""
    was = {s["at"]: s["ticks"] for s in live(before)}
    now = {s["at"]: s["ticks"] for s in live(after)}
    stayed = [at for at in was if at in now and at != ITERATION_NODE]
    assert len(stayed) >= 3, f"{where}: the pool emptied"
    for at in stayed:
        assert now[at] == was[at] + vblanks, \
            f"{where}: task at {at:#x} ticked {now[at] - was[at]} times in {vblanks} vblanks"


def test_every_live_task_ticks_once_per_vblank(game_factory):
    """The walker runs every live task once per frame: each still-live task's frame counter
    advances by exactly 1 over a vblank (and 10 over ten)."""
    game = game_factory()
    game.run(seconds(12))  # the title
    a = check_pool(game, "title")
    game.run(1)
    ticks_advance(a, pool(game), 1, "title")
    game.run(10)
    ticks_advance(a, pool(game), 11, "title")


def test_pool_links_and_handlers_stay_valid(game_factory):
    """Across the title, the main menu and the game's Options screen the pool stays consistent,
    and each scene runs its own handlers (scene changes spawn and kill tasks)."""
    game = game_factory()
    game.run(seconds(12))  # the title
    title = handlers(check_pool(game, "title"))
    boot_to_main_menu(game)
    menu = handlers(check_pool(game, "main menu"))
    for _ in range(3):  # the game's own Options (RT_GAME_OPTIONS=1 keeps them)
        game.press("down")
    game.press("cross")
    game.run(seconds(3))
    options = handlers(check_pool(game, "options"))
    assert menu != title, "the main menu should run its own tasks"
    assert options != menu, "the Options screen should run its own tasks"


def open_town(game_factory, name):
    """A game driving around Peach Town, streets quiet (no NPC cars to interrupt)."""
    edits = {FIELDS["location"]["offset"]: bytes([PEACH_TOWN]), FIELDS["licence"]["offset"]: bytes([2])}
    game = game_factory(checkpoint="adventure_first_save", progress_edits=edits, name=name)
    continue_to_factory(game)
    park_traffic(game, PEACH_TOWN)
    for _ in range(4):  # Change parts / Race / Save data / Quit game / Drive around town
        game.press("down")
    game.press("cross")
    game.run(seconds(2))
    game.press("cross")  # dismiss "Come again!"
    game.run(seconds(10))
    assert scene(game) == SCENE_TOWN, "driving in town"
    return game


def test_the_pause_menu_spawns_and_kills_its_task(game_factory):
    """The town's Start menu (pause_task, spawned by the town task) runs while it is up: with the
    game paused, the menu's own task ticks on while the world's tasks stand still, and after the
    menu every task ticks again — and the menu's task is killed."""
    game = open_town(game_factory, "task_pool_pause")
    pause_task = GAME_MAP["options"]["pause_task"]  # 0x217148, spawned by the town task
    assert pause_task not in handlers(pool(game)), "the Pause menu should not run unprompted"
    game.press("start")
    game.run(seconds(2))
    assert game.u32(PAUSE_STATE) == 0, "the Pause list"
    paused = pool(game)
    assert pause_task in handlers(paused), \
        f"the Pause menu's task: {sorted(hex(h) for h in handlers(paused))}"

    def counters(slots):
        return {s["at"]: s["ticks"] for s in live(slots) if s["at"] != ITERATION_NODE}

    was = counters(paused)
    menu_at = next(s["at"] for s in live(paused) if s["handler"] == pause_task)
    game.run(2)
    now = counters(pool(game))
    assert now[menu_at] == was[menu_at] + 2, "the Pause menu's task ticks on while the game is paused"
    ticking = [at for at in was if at in now and now[at] == was[at] + 2]
    frozen = [at for at in was if at in now and now[at] == was[at]]
    assert len(ticking) >= 2, f"the menu machinery should tick on: {len(ticking)} tasks"
    assert frozen, "the world's tasks should stand still while the game is paused"

    game.press("triangle")
    game.run(seconds(2))
    assert scene(game) == SCENE_TOWN, "back to driving"
    after = pool(game)
    assert pause_task not in handlers(after), "leaving the menu should kill its task"
    game.run(2)
    ticks_advance(after, pool(game), 2, "after the pause")
