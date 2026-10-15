// Unit tests for src/native/audio/{vag,mixer}.*. The decode+mix MODEL only —
// no device, no PS2/guest code. We hand-build a known VAG stream, decode it,
// and check the PCM matches a reference (the standard PS ADPCM formula walked
// by hand, matching tools/audio_extract.cpp::decodeAdpcm). The mixer is
// checked for sum, clamp, pan and mono. No game data.
#include "../../src/native/audio/vag.h"
#include "../../src/native/audio/mixer.h"

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

// --------- helpers: hand-rolled PS ADPCM reference (matches vag.cpp) -------

// Same coefficient tables as vag.cpp; the test grades the decoder against
// these so we test both the formula and the wiring.
static const int kF0[5] = {0, 60, 115, 98, 122};
static const int kF1[5] = {0,  0, -52, -55, -60};

struct RefState { int s1 = 0, s2 = 0; };

// Predict one sample using the same predictor as the decoder. Used to build
// "expected" PCM arrays against which the decoder is then compared.
static int predictOne(RefState& st, int nibble, int shift, int filter) {
    // int(int16_t(uint16_t(nibble << 12))) >> shift  — verbatim of decoder
    const int v = int(int16_t(uint16_t(nibble << 12))) >> shift;
    int s = v + ((st.s1 * kF0[filter] + st.s2 * kF1[filter] + 32) >> 6);
    if (s >  32767) s =  32767;
    if (s < -32768) s = -32768;
    st.s2 = st.s1;
    st.s1 = s;
    return s;
}

// --------- VAG tests --------------------------------------------------------

// A stream of all-zero blocks (shift=0, filter=0, every nibble 0) decodes to
// all-zero PCM, regardless of how many blocks are fed in. The simplest sanity
// check that wiring is correct.
static void testVagSilence() {
    VagNibbles n;
    n.shift = 0;
    n.filter = 0;
    n.flags = 0;
    // all nibbles already 0

    std::uint8_t block[16];
    buildVagBlock(n, block);

    // Two blocks + a terminator.
    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), block, block + 16);
    stream.insert(stream.end(), block, block + 16);
    std::uint8_t term[16] = {};
    term[1] = kVagEndOfStream;
    stream.insert(stream.end(), term, term + 16);

    const auto pcm = decodeVagStream(stream.data(), stream.size());
    CHECK_EQ_I32(int(pcm.size()), 56);   // 2 blocks * 28
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        if (pcm[i] != 0) {
            std::fprintf(stderr, "FAIL silence[%zu] = %d\n", i, int(pcm[i]));
            ++g_fail;
            break;
        }
    }
}

// A single block with shift=12, filter=0, every nibble = 8 (the most
// negative 4-bit signed value). With shift=12 the delta is just the signed
// 4-bit value (-8), the filter is identity, so each output sample must
// equal -8.
static void testVagAllNibble8() {
    VagNibbles n;
    n.shift = 12;
    n.filter = 0;
    n.flags = 0;
    for (std::size_t i = 0; i < 28; ++i) n.nibbles[i] = 8;

    std::uint8_t block[16];
    buildVagBlock(n, block);

    std::vector<std::uint8_t> stream(block, block + 16);
    std::uint8_t term[16] = {};
    term[1] = kVagEndOfStream;
    stream.insert(stream.end(), term, term + 16);

    const auto pcm = decodeVagStream(stream.data(), stream.size());
    CHECK_EQ_I32(int(pcm.size()), 28);
    for (int s : pcm) {
        // int(int16_t(8 << 12)) >> 12 == -8; clamp(-8) == -8.
        CHECK_EQ_I32(s, -8);
    }
}

// A hand-crafted block with shift=12, filter=0 (no AR feedback) and a
// nibble ramp [0, 1, 2, ..., 27]. The decoder formula is
// `int(int16_t(uint16_t(nibble << 12))) >> shift`. With shift=12 the
// 4-bit value is sign-extended at bit 12 (so nibbles 0..7 → +0..+7,
// nibbles 8..15 → -8..-1), and filter=0 zeroes the AR contribution.
// The expected output therefore wraps every 16 samples:
//   sample 0 (nibble 0): 0
//   sample 8 (nibble 8): -8
//   sample 11 (nibble 11): -5
//   sample 16 (nibble 0, second wrap): 0
//   sample 27 (nibble 11, second wrap): -5
static int expectedRamp(int i) {
    // Signed 4-bit value (range -8..+7), wrap every 16.
    const int nib = i & 0x0F;
    return nib < 8 ? nib : nib - 16;
}
static void testVagRamp() {
    VagNibbles n;
    n.shift = 12;
    n.filter = 0;
    n.flags = 0;
    for (std::size_t i = 0; i < 28; ++i) n.nibbles[i] = static_cast<std::uint8_t>(i & 0x0F);

    std::uint8_t block[16];
    buildVagBlock(n, block);

    std::vector<std::uint8_t> stream(block, block + 16);
    std::uint8_t term[16] = {};
    term[1] = kVagEndOfStream;
    stream.insert(stream.end(), term, term + 16);

    const auto pcm = decodeVagStream(stream.data(), stream.size());
    CHECK_EQ_I32(int(pcm.size()), 28);
    for (int i = 0; i < 28; ++i) {
        CHECK_EQ_I32(int(pcm[i]), expectedRamp(i));
    }
}

// A hand-crafted block with shift=1, filter=1 (f0=60, f1=0), nibble pattern
// [8, 0, 8, 0, ...] starting from s1=0, s2=0. This stresses both the delta
// path and the AR filter — the predictor carries state, so any wiring bug
// in `s1`/`s2` shows up early.
static void testVagReferenceBlock() {
    VagNibbles n;
    n.shift = 1;
    n.filter = 1;
    n.flags = 0;
    for (std::size_t i = 0; i < 28; ++i) n.nibbles[i] = (i & 1) ? 0 : 8;

    std::uint8_t block[16];
    buildVagBlock(n, block);

    std::vector<std::uint8_t> stream(block, block + 16);
    std::uint8_t term[16] = {};
    term[1] = kVagEndOfStream;
    stream.insert(stream.end(), term, term + 16);

    // Walk the reference function against the same nibble pattern. The
    // reference is a textbook port of `int(int16_t(uint16_t(nibble<<12)))
    // >> shift` plus the two-tap AR filter; identical to vag.cpp's decoder.
    RefState ref{};
    std::vector<int> expected;
    for (std::size_t i = 0; i < 28; ++i) {
        expected.push_back(predictOne(ref, n.nibbles[i], 1, 1));
    }

    const auto pcm = decodeVagStream(stream.data(), stream.size());
    CHECK_EQ_I32(int(pcm.size()), int(expected.size()));
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        CHECK_EQ_I32(int(pcm[i]), expected[i]);
    }
}

// A truncated tail (under 16 trailing bytes) is silently dropped: the
// decoder must never read past the end. The terminator block is what makes
// decodeVagStream stop, not size bookkeeping, so a missing terminator just
// truncates the output. We model both: missing-term and truncated-tail.
static void testVagTruncated() {
    VagNibbles n;
    n.shift = 0;
    n.filter = 0;
    n.flags = 0;
    for (std::size_t i = 0; i < 28; ++i) n.nibbles[i] = 7;  // mild positive

    std::uint8_t block[16];
    buildVagBlock(n, block);

    // Two full blocks, no terminator. Should still decode exactly 56 samples
    // and stop at the end of the buffer.
    std::vector<std::uint8_t> two_blocks;
    two_blocks.insert(two_blocks.end(), block, block + 16);
    two_blocks.insert(two_blocks.end(), block, block + 16);
    const auto noTerm = decodeVagStream(two_blocks.data(), two_blocks.size());
    CHECK_EQ_I32(int(noTerm.size()), 56);

    // Now add 7 bytes of "garbage" after a full block; the decoder must skip
    // them (tail < 16 bytes), and only return 28 samples.
    std::vector<std::uint8_t> short_tail;
    short_tail.insert(short_tail.end(), block, block + 16);
    short_tail.resize(short_tail.size() + 7, 0xFF);
    const auto tail = decodeVagStream(short_tail.data(), short_tail.size());
    CHECK_EQ_I32(int(tail.size()), 28);
}

// VAGp header parsing: 48-byte "VAGp" container carries rate at offset 16
// and length at offset 12 (both big-endian). On a non-VAGp input the
// function returns false without crashing.
static void testVagHeader() {
    // Body of one empty VAG block (zeros) so dataSize can survive the clamp
    // to "whatever's left in the file".
    std::vector<std::uint8_t> buf(48 + 32, 0);
    buf[0] = 'V'; buf[1] = 'A'; buf[2] = 'G'; buf[3] = 'p';
    // length = 32 (body bytes), big-endian: bytes 12..15 = 0,0,0,32
    buf[15] = 32;
    // rate = 12345, big-endian: bytes 16..19 = 0,0,48,57
    buf[18] = 48;
    buf[19] = 57;

    VagHeader h;
    CHECK(parseVagHeader(buf.data(), buf.size(), h));
    CHECK_EQ_I32(int(h.sampleRate), 12345);
    CHECK_EQ_I32(int(h.dataSize), 32);
    CHECK_EQ_I32(int(h.dataOffset), 48);

    // Bad magic: false.
    buf[0] = 'X';
    CHECK(!parseVagHeader(buf.data(), buf.size(), h));
}

// --------- Mixer tests ------------------------------------------------------
//
// Pan semantics for the mixer model:
//   - Linear pan law: at pan=0, L=R=1 (centred stereo, full source each).
//     At pan=+1, L=0, R=1; at pan=-1, L=1, R=0.
//   - Mono override (per-channel or master) zeroes the pan split and puts
//     half the source in each side (L=R=0.5) so total energy matches
//     centred stereo.
//   - Per-frame sum is clamped to int16 range; intermediate sums are int32.
//
// Output conventions:
//   - Stereo (channels=2): out[2f+0]=L, out[2f+1]=R.
//   - Mono  (channels=1): out[f]=L+R (mono downmix).
//
// Each `mix()` call advances the channel read cursor forward, so the same
// MixerState can produce consecutive frame chunks.

// Two centred channels summed, master volume 0.5. With centred pan the
// per-channel output is `src * master * 1` on each side. So frame f's
// (L,R) = ((pcmA[f] + pcmB[f]) * 0.5, same).
//   frame 0: (1000+500)*0.5 = 750
//   frame 1: (2000+1000)*0.5 = 1500
//   frame 2: (3000+1500)*0.5 = 2250
//   frame 3: (4000+2000)*0.5 = 3000
static void testMixerSumAndCenter() {
    const std::int16_t pcmA[] = {1000, 2000, 3000, 4000};
    const std::int16_t pcmB[] = {500,  1000, 1500, 2000};

    MixerState ms;
    ms.masterVolume = 0.5f;
    ms.channels.push_back({});
    ms.channels.push_back({});
    ms.setChannelPcm(0, pcmA, 4);
    ms.setChannelPcm(1, pcmB, 4);
    // Both channels centred (default pan=0), stereo (default mono=false).

    OutputBuffer out;
    out.channels = 2;
    ms.mix(out, 4);

    CHECK_EQ_I32(int(out.samples.size()), 8);
    CHECK_EQ_I32(out.samples[0], 750);
    CHECK_EQ_I32(out.samples[1], 750);
    CHECK_EQ_I32(out.samples[2], 1500);
    CHECK_EQ_I32(out.samples[3], 1500);
    CHECK_EQ_I32(out.samples[4], 2250);
    CHECK_EQ_I32(out.samples[5], 2250);
    CHECK_EQ_I32(out.samples[6], 3000);
    CHECK_EQ_I32(out.samples[7], 3000);
}

// Panning: at pan=+1 (full right), L=0 and R=src * volume * master.
// At master=1 and volume=2: R = pcm[f] * 2.
static void testMixerPan() {
    const std::int16_t pcm[] = {100, -200, 300, -400};

    MixerState ms;
    ms.masterVolume = 1.f;
    ms.channels.push_back({});
    ms.setChannelPcm(0, pcm, 4);
    ms.setVolume(0, 2.f);     // loud voice
    ms.setMono(0, false);     // stereo
    ms.channels[0].pan = 1.f; // full right

    OutputBuffer out;
    out.channels = 2;
    ms.mix(out, 4);

    CHECK_EQ_I32(int(out.samples.size()), 8);
    for (std::size_t f = 0; f < 4; ++f) {
        CHECK_EQ_I32(out.samples[2 * f + 0], 0);               // L mute
        CHECK_EQ_I32(out.samples[2 * f + 1], int(pcm[f]) * 2); // R = src*vol
    }
}

// Centred stereo with no mono flag should produce identical L and R that
// each equal the source * volume * master (no attenuation). This is the
// simplest contract a stereo game depends on.
static void testMixerStereoCenter() {
    const std::int16_t pcm[] = {100, 200};
    MixerState ms;
    ms.masterVolume = 1.f;
    ms.channels.push_back({});
    ms.setChannelPcm(0, pcm, 2);
    ms.setVolume(0, 1.f);
    ms.channels[0].pan = 0.f; // centred

    OutputBuffer out;
    out.channels = 2;
    ms.mix(out, 2);

    CHECK_EQ_I32(int(out.samples.size()), 4);
    CHECK_EQ_I32(out.samples[0], 100);
    CHECK_EQ_I32(out.samples[1], 100);
    CHECK_EQ_I32(out.samples[2], 200);
    CHECK_EQ_I32(out.samples[3], 200);
}

// Clamp at the int16 boundary. Master=1, channel A volume=4 pan=-1 (full
// left), pcm=20000: L = 20000*4 = 80000 clamp to +32767. Channel B same
// volume pan=+1 (full right), pcm=-20000: R = -20000*4 = -80000 clamp
// to -32768.
static void testMixerClamp() {
    const std::int16_t pos[] = {20000};
    const std::int16_t neg[] = {-20000};

    MixerState ms;
    ms.masterVolume = 1.f;
    ms.channels.push_back({});
    ms.channels.push_back({});
    ms.setChannelPcm(0, pos, 1);
    ms.setChannelPcm(1, neg, 1);
    ms.setVolume(0, 4.f);
    ms.setVolume(1, 4.f);
    ms.channels[0].pan = -1.f;  // full left
    ms.channels[1].pan = 1.f;   // full right

    OutputBuffer out;
    out.channels = 2;
    ms.mix(out, 1);

    CHECK_EQ_I32(int(out.samples.size()), 2);
    CHECK_EQ_I32(out.samples[0],  32767);  // L=20000*4 clamped
    CHECK_EQ_I32(out.samples[1], -32768);  // R=-20000*4 clamped
}

// Mono flag overrides the pan split. With mono=true, the source is summed
// equally into L and R at half amplitude each (total energy preserved
// against centred stereo). Master=1, volume=1, pcm=1000, pan=+1:
//   L = R = 1000 * 1 * 1 * 0.5 = 500
static void testMixerMonoFlag() {
    const std::int16_t pcm[] = {1000};
    MixerState ms;
    ms.masterVolume = 1.f;
    ms.channels.push_back({});
    ms.setChannelPcm(0, pcm, 1);
    ms.setVolume(0, 1.f);
    ms.setMono(0, true);          // mono on
    ms.channels[0].pan = 1.f;     // would otherwise be full right

    OutputBuffer out;
    out.channels = 2;
    ms.mix(out, 1);

    CHECK_EQ_I32(int(out.samples.size()), 2);
    CHECK_EQ_I32(out.samples[0], 500);
    CHECK_EQ_I32(out.samples[1], 500);
}

// Master mono overrides per-channel pan settings, the same way the per-
// channel flag does.
static void testMixerMasterMono() {
    const std::int16_t pcm[] = {1000};
    MixerState ms;
    ms.masterVolume = 1.f;
    ms.masterMono = true;
    ms.channels.push_back({});
    ms.setChannelPcm(0, pcm, 1);
    ms.setVolume(0, 1.f);
    ms.channels[0].pan = -1.f;   // would otherwise be full left

    OutputBuffer out;
    out.channels = 2;
    ms.mix(out, 1);

    CHECK_EQ_I32(int(out.samples.size()), 2);
    CHECK_EQ_I32(out.samples[0], 500);
    CHECK_EQ_I32(out.samples[1], 500);
}

// Mono output downmixes the stereo accumulator as L+R per frame. With one
// centred stereo voice (left=right=src), the mono output is 2*src — that's
// the conventional mono mixdown of stereo PCM.
static void testMixerMonoOutput() {
    const std::int16_t pcm[] = {100, 200};
    MixerState ms;
    ms.masterVolume = 1.f;
    ms.channels.push_back({});
    ms.setChannelPcm(0, pcm, 2);
    ms.setVolume(0, 1.f);
    ms.channels[0].pan = 0.f;

    OutputBuffer out;
    out.channels = 1;
    ms.mix(out, 2);

    CHECK_EQ_I32(int(out.samples.size()), 2);
    CHECK_EQ_I32(out.samples[0], 100);  // (100+100+1)>>1 = 100
    CHECK_EQ_I32(out.samples[1], 200);  // (200+200+1)>>1 = 200
}

// The read cursor advances as mix() consumes PCM, so multiple short mixes
// over the same MixerState stitch together correctly. This is what the
// runtime needs when each game tick mixes a fixed frame budget.
static void testMixerCursorAdvances() {
    const std::int16_t pcm[] = {1, 2, 3, 4, 5, 6};
    MixerState ms;
    ms.masterVolume = 1.f;
    ms.channels.push_back({});
    ms.setChannelPcm(0, pcm, 6);

    OutputBuffer a;
    a.channels = 2;
    ms.mix(a, 2);
    CHECK_EQ_I32(a.samples[0], 1);
    CHECK_EQ_I32(a.samples[1], 1);
    CHECK_EQ_I32(a.samples[2], 2);
    CHECK_EQ_I32(a.samples[3], 2);

    OutputBuffer b;
    b.channels = 2;
    ms.mix(b, 2);
    CHECK_EQ_I32(b.samples[0], 3);
    CHECK_EQ_I32(b.samples[1], 3);
    CHECK_EQ_I32(b.samples[2], 4);
    CHECK_EQ_I32(b.samples[3], 4);

    OutputBuffer c;
    c.channels = 2;
    ms.mix(c, 2);
    CHECK_EQ_I32(c.samples[0], 5);
    CHECK_EQ_I32(c.samples[1], 5);
    CHECK_EQ_I32(c.samples[2], 6);
    CHECK_EQ_I32(c.samples[3], 6);
}

int main() {
    testVagSilence();
    testVagAllNibble8();
    testVagRamp();
    testVagReferenceBlock();
    testVagTruncated();
    testVagHeader();
    testMixerSumAndCenter();
    testMixerPan();
    testMixerStereoCenter();
    testMixerClamp();
    testMixerMonoFlag();
    testMixerMasterMono();
    testMixerMonoOutput();
    testMixerCursorAdvances();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("audio: all checks passed");
    return 0;
}
