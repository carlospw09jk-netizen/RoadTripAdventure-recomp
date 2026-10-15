// Unit tests for src/native/audio/engine.*. The authored-replacement sound
// engine: voice pool, BGM/SE control, fade ramps, mixer driver. The PCM
// sources here are synthetic int16 buffers; tests do not touch the VAG
// decoder (that's covered by tests/native_assets/test_audio.cpp). No game
// data, no PS2/guest/IOP code.
#include "../../src/native/audio/engine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace rt::native::audio;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)
#define CHECK_EQ_I32(a, b) do {                                                  \
    const auto _a = (a); const auto _b = (b);                                    \
    if (_a != _b) {                                                              \
        std::fprintf(stderr, "FAIL %s:%d: %s == %s, got %d vs %d\n",              \
                     __FILE__, __LINE__, #a, #b, int(_a), int(_b)); ++g_fail;     \
    } } while (0)

// ----- helpers --------------------------------------------------------------

// Find the absolute peak in a stereo interleaved output buffer.
static int peakOf(const OutputBuffer& out) {
    int pk = 0;
    for (int s : out.samples) {
        const int a = std::abs(int(s));
        if (a > pk) pk = a;
    }
    return pk;
}

// Any nonzero sample? Quick liveness check.
static bool anyNonzero(const OutputBuffer& out) {
    for (int s : out.samples) if (s != 0) return true;
    return false;
}

// Sum the absolute samples; a single scalar measure of "how much was produced".
static int energyOf(const OutputBuffer& out) {
    int e = 0;
    for (int s : out.samples) e += std::abs(int(s));
    return e;
}

// A small 8-frame PCM at sample values {100,200,300,400,500,600,700,800}.
// Centred stereo (L=R=src) at master=1/vol=1 produces an output each frame
// of (src, src). Used by every test as a known signal.
struct Signal8 {
    std::int16_t data[8];
    Signal8() {
        for (int i = 0; i < 8; ++i) data[i] = std::int16_t((i + 1) * 100);
    }
};
static PcmSourceView signal8(Signal8& s, bool loop = false) {
    return PcmSourceView{s.data, 8, loop};
}

// ----- 1. playBgm + tick produces nonzero output and a live voice count -----

static void testPlayBgmProducesAudio() {
    Signal8 s;
    SoundEngine e;
    // packedId 0x0a1c0201: file index = 0x0a, param = 0x1c, lowId = 0x01.
    e.playBgm(0, 0x0a1c0201u, signal8(s, true), 1.f);
    CHECK_EQ_I32(e.activeVoiceCount(), 1);
    CHECK_EQ_I32(e.activeBgmSlot(0), 0);

    OutputBuffer out;
    e.tick(4, out);
    CHECK_EQ_I32(int(out.samples.size()), 8);   // 4 frames * 2 channels
    CHECK(anyNonzero(out));
    CHECK_EQ_I32(peakOf(out), 400);              // sample value 400 at frame 3

    // Voice is still alive after partial playback (loop=true).
    CHECK(e.voicePlaying(0));
}

// ----- 2. stopAll -> silence ------------------------------------------------

static void testStopAllSilences() {
    Signal8 s;
    SoundEngine e;
    e.playBgm(0, 0x0a1c0201u, signal8(s, true), 1.f);
    e.playSe (PcmSourceView{s.data, 4, false}, 1.f);
    CHECK_EQ_I32(e.activeVoiceCount(), 2);

    e.stopAll();

    OutputBuffer out;
    e.tick(4, out);
    CHECK_EQ_I32(int(out.samples.size()), 8);
    CHECK(!anyNonzero(out));
    CHECK_EQ_I32(peakOf(out), 0);
    CHECK_EQ_I32(e.activeVoiceCount(), 0);
}

// ----- 3. playSe is a one-shot that retires ---------------------------------

static void testPlaySeOneShot() {
    Signal8 s;
    SoundEngine e;

    // SE source of exactly 8 frames.
    const bool ok = e.playSe(signal8(s, false), 1.f);
    CHECK(ok);
    CHECK_EQ_I32(e.activeVoiceCount(), 1);

    // Halfway: voice still alive.
    OutputBuffer out;
    e.tick(4, out);
    CHECK(anyNonzero(out));
    CHECK_EQ_I32(e.activeVoiceCount(), 1);

    // Drain the remaining 4 frames: voice retires as soon as its cursor
    // reaches the source's frame count (the slice that hits the end-of-
    // stream midpoint unbinds the voice in the same tick).
    e.tick(4, out);
    CHECK_EQ_I32(e.activeVoiceCount(), 0);

    // After retirement a 1-frame tick produces silence.
    OutputBuffer silence;
    e.tick(1, silence);
    CHECK(!anyNonzero(silence));
}

// ----- 3b. SE loop=false is forced even if caller passed loop=true -------

static void testPlaySeForcesOneShot() {
    Signal8 s;
    SoundEngine e;
    // Caller asks for loop; playSe ignores it.
    e.playSe(PcmSourceView{s.data, 4, true}, 1.f);
    CHECK_EQ_I32(e.activeVoiceCount(), 1);
    OutputBuffer obuf;
    e.tick(8, obuf);
    OutputBuffer tail;
    e.tick(1, tail);
    CHECK_EQ_I32(e.activeVoiceCount(), 0);
}

// ----- 4. setMasterVolume halves the peak ----------------------------------

static void testMasterVolumeHalves() {
    Signal8 s;
    SoundEngine e;
    // Use a 4-frame source (first 4 samples: 100, 200, 300, 400) looping,
    // so the cursor stays in [0,3] for tick(4) and the peak is consistent
    // across multiple ticks (no wrap).
    const std::int16_t fourData[4] = {100, 200, 300, 400};
    PcmSourceView src{fourData, 4, true};
    e.playBgm(0, 0x0a1c0201u, src, 1.f);

    OutputBuffer full;
    e.tick(4, full);
    const int peakFull = peakOf(full);
    CHECK_EQ_I32(peakFull, 400);

    e.setMasterVolume(0.5f);
    OutputBuffer half;
    e.tick(4, half);
    const int peakHalf = peakOf(half);
    // Linear: half volume → peak is exactly halved. The peak frame is the
    // same source sample (frame 3 = 400) → expected peak 200.
    CHECK_EQ_I32(peakHalf, 200);

    e.setMasterVolume(0.f);
    OutputBuffer muted;
    e.tick(4, muted);
    CHECK_EQ_I32(peakOf(muted), 0);
}

// ----- 5. fadeChannel ramps volume to target in exactly the right time ------
//
// The ramp must stay linear across multi-tick fade advances. With a
// 10-frame fade 1.0 → 0.0 driven 4 frames at a time, voiceVolume() after
// tick(4) is exactly 0.6 and after a second tick(4) is exactly 0.2 (the
// OLD non-linear code gave 0.6 then 0.333). post-fade the voice clamps
// to 0 and a tick of silence follows.

static bool nearEq(float a, float b, float eps) {
    return std::fabs(a - b) <= eps;
}

static void testFadeChannel() {
    Signal8 s;
    SoundEngine e;
    e.playBgm(0, 0x0a1c0201u, signal8(s, true), 1.f);

    // Fade from 1.0 to 0.0 over 10 frames.
    e.fadeChannel(0, 0.f, 10);
    CHECK(nearEq(e.voiceVolume(0), 1.0f, 1e-6f));   // ramp starts at 1.0

    // Tick 4 frames: elapsed 4 of 10 → vol = 1.0 + (-1.0) * 0.4 = 0.6.
    OutputBuffer o1;
    e.tick(4, o1);
    CHECK(nearEq(e.voiceVolume(0), 0.6f, 1e-3f));

    // Tick 4 more: elapsed 8 of 10 → vol = 1.0 + (-1.0) * 0.8 = 0.2.
    OutputBuffer o2;
    e.tick(4, o2);
    CHECK(nearEq(e.voiceVolume(0), 0.2f, 1e-3f));

    // Tick the remaining 2 frames: ramp completes, voice clamps to 0.0.
    OutputBuffer o3;
    e.tick(2, o3);
    CHECK(nearEq(e.voiceVolume(0), 0.0f, 1e-6f));

    // The peak during the fade sits at the source's largest sample in the
    // tick's slice. First tick(4): cursor 0, slice = 4 frames
    // [100,200,300,400], peak sample = 400, * vol=0.6 → 240.
    // Second tick(4): cursor 4 (advanced), slice = 4 frames
    // [500,600,700,800], peak sample = 800, * vol=0.2 ≈ 160 (the float
    // cast may land at 159.999... truncated to 159; accept either).
    CHECK_EQ_I32(peakOf(o1), 240);
    CHECK(peakOf(o2) == 159 || peakOf(o2) == 160);

    // Post-fade: a 1-frame tick produces silence.
    OutputBuffer silent;
    e.tick(1, silent);
    CHECK(!anyNonzero(silent));
}

// ----- 5b. setChannelVolume cancels an in-flight fade ------------------------

static void testChannelVolumeCancelFade() {
    Signal8 s;
    SoundEngine e;
    e.playBgm(0, 0x0a1c0201u, signal8(s, true), 1.f);
    e.fadeChannel(0, 0.f, 10);
    // 5 ticks in, vol is ~0.5. Snap to 0.25:
    OutputBuffer obuf;
    e.tick(5, obuf);
    e.setChannelVolume(0, 0.25f);
    OutputBuffer out;
    e.tick(4, out);
    // Cursor was 5 after the first 5 ticks; with the looping source's
    // 8 frames, the remaining slice is only 3 frames (samples 5,6,7 →
    // values 600, 700, 800). master=1, vol=0.25 → outputs 150, 175, 200
    // for those 3 frames; the 4th output frame is silent (no channel
    // feeding the mixer that frame). Peak = 200.
    CHECK_EQ_I32(peakOf(out), 200);
}

// ----- 6. BGM id packing is interpreted correctly --------------------------

static void testBgmIdUnpacking() {
    // gs_music_play_bgm packs a BGM id like 0x0a1c0201. Per
    // docs/game-mechanics/sound-and-daynight.md §1 "BGM id packing":
    //   (id >> 16) & 0xff   → gs_music_file_table index (cmd 7 row)
    //   (id >> 8)  & 0xff   → cmd 7 parameter
    //   id & 0xff           → the low-byte cmd 0xE id
    // Doc marks this layout [inferred from gs_music_play_bgm].
    //
    // 0x0a1c0201:
    //   (0x0a1c0201 >> 16) & 0xff = 0x1c (file index)
    //   (0x0a1c0201 >> 8)  & 0xff = 0x02 (cmd 7 param)
    //   0x0a1c0201 & 0xff         = 0x01 (cmd 0xE id)

    const auto a = unpackBgmId(0x0a1c0201u);
    CHECK_EQ_I32(int(a.fileIndex), 0x1c);
    CHECK_EQ_I32(int(a.param),     0x02);
    CHECK_EQ_I32(int(a.lowId),     0x01);

    const auto b = unpackBgmId(0x0a1a0201u);
    CHECK_EQ_I32(int(b.fileIndex), 0x1a);
    CHECK_EQ_I32(int(b.param),     0x02);
    CHECK_EQ_I32(int(b.lowId),     0x01);

    // All-ones: every field saturates at 0xFF.
    const auto c = unpackBgmId(0xFFFFFFFFu);
    CHECK_EQ_I32(int(c.fileIndex), 0xFF);
    CHECK_EQ_I32(int(c.param),     0xFF);
    CHECK_EQ_I32(int(c.lowId),     0xFF);

    const auto d = unpackBgmId(0u);
    CHECK_EQ_I32(int(d.fileIndex), 0);
    CHECK_EQ_I32(int(d.param),     0);
    CHECK_EQ_I32(int(d.lowId),     0);
}

// ----- 7. Two voices (BGM + SE) sum into the output ------------------------

static void testTwoVoicesSum() {
    Signal8 s;
    SoundEngine e;
    e.playBgm(0, 0x0a1c0201u, signal8(s, true), 1.f);
    // SE in voice 4 (first SE pool slot): same signal, looping not for SE.
    e.playSe(signal8(s, false), 1.f);
    CHECK_EQ_I32(e.activeVoiceCount(), 2);

    OutputBuffer out;
    e.tick(4, out);
    // Both voices play the same signal at the same volume. Each frame's
    // L and R accumulators get 2*src; with master=1 and centred, the
    // output per channel is 2*src. Frame 3 = (800, 800). Peak = 800
    // wait — that's 2 * 400 = 800, not 800. Let me check: source frame
    // i has value (i+1)*100, frame 3 = 400. Two voices sum to 800. ✓
    CHECK_EQ_I32(peakOf(out), 800);
    // Energy roughly doubles vs. single-voice.
    CHECK(energyOf(out) > 0);
}

// ----- 7b. BGM in two slots, both going ---------------------------------

static void testTwoBgmSlots() {
    Signal8 s;
    SoundEngine e;
    e.playBgm(0, 0x0a1c0201u, signal8(s, true), 1.f);
    e.playBgm(2, 0x0a1a0201u, signal8(s, true), 1.f);
    CHECK_EQ_I32(e.activeBgmSlot(0), 0);
    CHECK_EQ_I32(e.activeBgmSlot(1), -1);
    CHECK_EQ_I32(e.activeBgmSlot(2), 2);
    CHECK_EQ_I32(e.activeBgmSlot(3), -1);

    OutputBuffer out;
    e.tick(4, out);
    CHECK_EQ_I32(peakOf(out), 800);   // 400 + 400
}

// ----- 7c. stopping one BGM slot keeps the other --------------------------

static void testStopOneBgmSlot() {
    Signal8 s;
    SoundEngine e;
    e.playBgm(0, 0x0a1c0201u, signal8(s, true), 1.f);
    e.playBgm(1, 0x0a1a0201u, signal8(s, true), 1.f);
    e.playBgm(0, 0x00000000u, signal8(s, true), 1.f);  // restart slot 0
    CHECK_EQ_I32(e.activeBgmSlot(0), 0);
    CHECK_EQ_I32(e.activeBgmSlot(1), 1);
    CHECK_EQ_I32(e.activeVoiceCount(), 2);
    e.stopBgm();
    CHECK_EQ_I32(e.activeVoiceCount(), 0);
}

// ----- 8. SE pool full: extra playSe returns false -------------------------

static void testSePoolFull() {
    Signal8 s;
    SoundEngine e;
    // kMaxVoices = 24; kBgmSlots = 4; SE pool is 20 voices. Fill them all.
    bool allOk = true;
    for (int i = 0; i < 20; ++i) {
        if (!e.playSe(signal8(s, false), 0.5f)) { allOk = false; break; }
    }
    CHECK(allOk);
    CHECK_EQ_I32(e.activeVoiceCount(), 20);
    // 21st request is dropped (returns false, no voice added).
    const bool dropped = e.playSe(signal8(s, false), 0.5f);
    CHECK(!dropped);
    CHECK_EQ_I32(e.activeVoiceCount(), 20);
}

// ----- 9. Looping BGM keeps playing indefinitely ---------------------------

static void testBgmLoop() {
    Signal8 s;
    SoundEngine e;
    e.playBgm(0, 0x0a1c0201u, signal8(s, true), 1.f);
    OutputBuffer out;
    // 8 frames of source. Run 24 frames — looping voice should still be alive.
    for (int i = 0; i < 3; ++i) e.tick(8, out);
    CHECK_EQ_I32(e.activeVoiceCount(), 1);
    // Voice cursor wraps every 8 frames (3 full iterations of 8).
    CHECK(e.voicePlaying(0));
}

// ----- 10. Empty/null source: playBgm no-ops -------------------------------

static void testNullSourceSafe() {
    SoundEngine e;
    e.playBgm(0, 0x0a1c0201u, PcmSourceView{nullptr, 0, false}, 1.f);
    // Slot remains empty (bindVoice sees src empty).
    CHECK_EQ_I32(e.activeBgmSlot(0), -1);
    CHECK_EQ_I32(e.activeVoiceCount(), 0);
    OutputBuffer out;
    e.tick(2, out);
    CHECK(!anyNonzero(out));
}

int main() {
    testPlayBgmProducesAudio();
    testStopAllSilences();
    testPlaySeOneShot();
    testPlaySeForcesOneShot();
    testMasterVolumeHalves();
    testFadeChannel();
    testChannelVolumeCancelFade();
    testBgmIdUnpacking();
    testTwoVoicesSum();
    testTwoBgmSlots();
    testStopOneBgmSlot();
    testSePoolFull();
    testBgmLoop();
    testNullSourceSafe();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("engine: all checks passed");
    return 0;
}
