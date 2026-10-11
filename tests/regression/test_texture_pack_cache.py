"""Texture pack cache (gs_texture_pack_cache.h): a pack converts once (#17).

A small pack of made-up images (their hashes needn't match any texture: only the conversion is
checked) is started with twice on the hardware GS. The first start converts every image; the
second converts none. A PNG that can't be read fails once and is not tried again. A pack whose
usual cache folder can't be written (the Thor's textures folder had been copied in by root) caches
in the app's own data instead of failing every image and converting again at every start.
"""

import os
import re
import stat

import numpy as np
import pytest
from PIL import Image

ENV = {"RT_GS_BACKEND": "hw"}


def make_pack(root, n=12):
    pack = root / "packs" / "tiny"
    pack.mkdir(parents=True)
    rng = np.random.default_rng(17)
    for i in range(n):
        pixels = rng.integers(0, 256, size=(64, 64, 4), dtype=np.uint8)
        Image.fromarray(pixels, "RGBA").save(pack / f"{0x17000000 + i:016x}_32x32_psm0.png")
    return pack


def start(game_factory, name, pack, extra=None):
    game = game_factory(name=name, env={**ENV, "RT_TEXTURE_PACK": str(pack), **(extra or {})})
    log = game.log_text()  # the startup conversion runs before the game, so it is all logged by now
    if "[textures] pack cache: none" in log or "can't sample" in log:
        pytest.skip("no ASTC/BC7 cache on this GPU")
    return game, log


def prepared(log):
    # (stderr: the [setup] line on stdout is buffered until the app exits)
    m = re.search(r"\[textures\] pack prepare .*: (\d+) of \d+ done, (\d+) failed", log)
    return (int(m.group(1)), int(m.group(2))) if m else (0, 0)


@pytest.mark.parametrize("fmt", ["astc", "bc7"])
def test_pack_converts_once(game_factory, tmp_path, fmt):
    pack = make_pack(tmp_path)
    (pack / "ffffffffffffffff_8x8_psm0.png").write_bytes(b"not a png")
    extra = {"RT_TEXTURE_CACHE_FORMAT": fmt}
    _, log = start(game_factory, f"pack_cache_{fmt}_1", pack, extra)
    assert prepared(log) == (13, 1), log[-3000:]
    assert "pack image not prepared (load)" in log
    _, log = start(game_factory, f"pack_cache_{fmt}_2", pack, extra)
    assert "pack prepare" not in log, "converted again on the second start"
    assert "without a current" not in log


def test_pack_cache_falls_back_when_unwritable(game_factory, tmp_path):
    textures = tmp_path / "textures"
    pack = make_pack(textures)
    os.chmod(textures, stat.S_IRUSR | stat.S_IXUSR)  # textures/cache can't be made
    try:
        game, log = start(game_factory, "pack_cache_fallback", pack)
        assert prepared(log) == (12, 0), log[-3000:]
        fallback = game.data_dir / "cache" / "packs" / "tiny"
        assert "pack cache in" in log and fallback.is_dir()
        assert len(list(fallback.glob("*.astc")) + list(fallback.glob("*.bc7"))) == 12
    finally:
        os.chmod(textures, stat.S_IRWXU)
