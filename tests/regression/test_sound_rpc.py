"""The sound engine's EE -> IOP RPC path (docs/game-mechanics/sound-and-daynight.md; config
[music]): the game never mixes audio itself — every BGM, SE and radio stream is a request to the
IOP's SNDMOD server (SIF RPC id 0x1234567). Two behaviors, watched from the IOP side:

- a race starts its BGM through gs_music_play_bgm (0x258EC8): the last PlayBGM's slot and song
  land in SNDMOD's bgm buffer, and the code the race's BGM table holds for the course matches;
- the town radio plays its hour-long stream at the time of day: its stream position follows the
  clock while it plays (the station switch itself is test_menus::test_radio_station's).
"""

from __future__ import annotations

from rtharness import seconds
from rtharness.adventure import FIELDS, GAME_MAP, SCENE_TOWN, continue_to_factory, scene
from rtharness.quick_race import open_course, start_race
from rtharness.town import park_traffic

MUSIC = GAME_MAP["music"]
BGMBUF = MUSIC["iop_bgmbuf"]             # slot << 8 | song of the last PlayBGM (stale when stopped)
RADIO = MUSIC["iop_radio"]               # +8 flag_play (2 = playing), +0x18 play_time, +0x1C play_tune
RACE_BGM_TABLE = MUSIC["race_bgm_table"]  # the BGM code per course id
PEACH_TOWN = 1


def test_race_bgm_reaches_the_iop(game_factory):
    """A Quick Race on Peach Raceway: the race's own BGM start (gs_music_play_bgm -> cmd 0xE/7)
    shows in SNDMOD's bgm buffer as the slot/song of the race BGM table's code for the course,
    and the race is audible."""
    game = game_factory()
    open_course(game, 0)  # Peach Raceway's card
    start_race(game)
    game.run(seconds(6))  # the card's music out, the countdown, the race's BGM in
    game.audio()
    game.run(seconds(5))
    sound = game.audio()
    assert sound["rms"] > 300, f"the race should be audible (engine and BGM): {sound}"
    played = game.u16(BGMBUF, space="iop")
    assert played != 0, "no PlayBGM reached the IOP"
    # The course's record (the race pointer the game keeps) and its code in the BGM table:
    # a0 = 0x0A .. ss nn (file, .., slot, song) -> the buffer holds slot << 8 | song.
    course = game.u8(game.u32(GAME_MAP["game"]["race_address"]))
    code = game.u32(RACE_BGM_TABLE + 4 * course)
    slot, song = code >> 8 & 0xFF, code & 0xFF
    assert played == slot << 8 | song, \
        f"the race BGM should be the table's code for course {course}: played {played:#x}, code {code:#x}"


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


def set_clock(game, hour: int, minute: int = 0) -> None:
    clock = GAME_MAP["clock"]
    game.write(clock["time_address"], int((hour * 60 + minute) * clock["vblanks_per_minute"]).to_bytes(4, "little"))


def test_radio_plays_at_the_time_of_day(game_factory):
    """In town the radio plays (SNDMOD's flag, the default station E-RADIO), and its stream
    position follows the time of day: set the clock hours apart and the radio plays at each
    hour of its day-long stream, still playing and audible."""
    game = open_town(game_factory, "sound_rpc_radio")
    assert game.u32(RADIO + 8, space="iop") == 2, "the radio should be playing"
    assert game.u32(RADIO + 0x1C, space="iop") == 1, "E-RADIO, the default station"
    positions = []
    for hour in (12, 23, 6):
        set_clock(game, hour)
        game.run(90)
        positions.append(game.u32(RADIO + 0x18, space="iop"))
        assert game.u32(RADIO + 8, space="iop") == 2, f"still playing at {hour}:00"
    assert all(p != 0 for p in positions), f"the stream should be somewhere: {positions}"
    assert len(set(positions)) == len(positions), \
        f"the radio should play at the time of day, not one position for every hour: {positions}"
    game.audio()
    game.run(seconds(3))
    assert game.audio()["rms"] > 100, "the radio should be audible"
