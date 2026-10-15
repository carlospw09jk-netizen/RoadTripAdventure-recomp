# TSQ / TVB music data (Road Trip, SLUS-20398)

Source: byte analysis of `SOUND/` from a user-extracted disc. Parsers live in
`src/native/assets/music_seq.{h,cpp}` (tests: `tests/native_assets/test_music_seq.cpp`).

## BGM.TVB — tone bank with embedded sample pool

- Header: 256 little-endian u32 entries starting at file offset 0.
  - Entries 0–1 are zero; entries 2..255 are a **monotonic, 16-byte-aligned
    offset table** (`0x980, 0x8A50, 0x9C10, 0x10790, …`, last `0x48C00`).
  - Entry 256 begins unrelated metadata — the offset table is exactly 256
    entries (254 sample slots + 2 leading zeros).
- `BGM.TVB` size `0x49400`; sample pool spans `[0x980, 0x48C00)` **inside the
  TVB itself** — the bank is self-contained (it does NOT index the big
  `1CH/2CH/3CH*.VAG` pool files).
- Pool entries have **no `VAGp` magic**: the samples are headerless PS1-style
  4-bit ADPCM blocks (16-byte frames: one shift/filter byte, one flags byte,
  then 14 packed-nibble bytes = 28 PCM samples). Decodeable with the raw-block
  helper in `audio/vag.h`; the TVB range determines the end, not a VAG stream
  end flag.

## TSQ — sequence files (BGM_01..09, ACTION, CQ_MAIN, SELECT, …)

- Layout: 4-byte header, then a table of `{u16 segment, u16 fileOffset}`
  pointer records. `segment` is normally `0x0090`; BGM files carry one leading
  `0x0000` record. The pointer table ends at the lowest track target offset
  (offsets are not sorted).
- Track counts: BGM-family files = **50 tracks**, boundary `0xCC`;
  ACTION / CQ_MAIN = **99 tracks**, boundary `0x190`.
- Track event streams are raw bytes: BGM tracks commonly start
  `F0 FF FF 00`, ACTION tracks `E9 F0 F9 0B`.
- **Event opcode semantics (note on/off, duration, pitch, tempo) are NOT yet
  cracked** — the parser exposes bounded raw spans per track. Sequenced
  playback is the open TODO; the container/track structure is proven.
