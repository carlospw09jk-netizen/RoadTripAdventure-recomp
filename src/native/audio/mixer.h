// Authored mixer model for the native audio vertical. Pure DSP (no device,
// no PS2/guest code). Companion to vag.h.
//
// The mixer sums N playback channels into one stereo output buffer. Each
// channel carries:
//   - a decoded mono PCM buffer (int16, one sample per frame slot)
//   - a mono gain and a per-channel volume (the master volume scaled in by
//     ApplySoundVolumes: the SNDMOD cmd 0x31 master value, multiplied by the
//     per-channel voice level)
//   - a pan (-1 = full left, 0 = centre, +1 = full right)
//   - a mono flag that, when on, sums the channel's source into both output
//     channels equally (so a mono SE comes out of both speakers regardless
//     of pan). This matches the app's [sound] `mono` behaviour: with mono on
//     the UI is told one speaker is disabled; the mixer honors it by NOT
//     applying pan. With mono off, the pan const-power/linear curve splits
//     the source between left and right at the right amplitude.
//
// Quantisation: outputs are int16 with clamping at [-32768, 32767]; the
// mixer keeps intermediate sums in int32 so a fat stack of voices doesn't
// wrap. We do not model the host audio device or output format; the test
// harness and the future native runtime take the produced int16 stereo
// stream from `mix()` and forward it.
//
// Channels are addressed by index; the producer is responsible for keeping
// alive (or moving) the PCM buffers it sets on each channel.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rt::native::audio {

// One playback channel. The PCM buffer is interleaved-mono int16 (one
// sample per frame slot). A channel with empty pcm contributes silence.
struct MixerChannel {
    // Pointer is non-owning; see `attachPcm`.
    const std::int16_t* pcm = nullptr;
    std::size_t pcmFrames = 0;   // mono frames in the channel's buffer
    std::size_t readCursor = 0;  // frames already consumed by mix()

    float volume = 1.f;          // [0..1]; can exceed 1 (headroom) but mix() clamps
    float pan = 0.f;             // [-1..+1]; 0 = centre
    bool mono = false;           // true = ignore pan; sum equally L+R
};

// Linear pan law: at pan=0 the channel is centred and the source reaches
// both speakers at full amplitude (L=R=1). At pan=+1 the source goes only
// to R (L=0, R=1); at pan=-1 only to L. This is the simplest split that
// preserves a centred channel's loudness, which is what a console PCM mix
// typically wants. An equal-power sqrt split (which halves the centre
// channel) is also common but introduces a -3 dB centre that callers must
// compensate for; we deliberately do not.
struct PanLaw {
    static float left(float pan) {
        return pan >= 0.f ? std::min(1.f - pan, 1.f) : 1.f;
    }
    static float right(float pan) {
        return pan <= 0.f ? std::min(1.f + pan, 1.f) : 1.f;
    }
};

// Output buffer dimensions. Frames = samples-per-channel, channels = 1
// for mono mixdown or 2 for stereo. `mix()` writes `totalSamples()` values
// (frames * channels) into the buffer, summing every channel's contribution.
struct OutputBuffer {
    std::vector<std::int16_t> samples;
    std::size_t channels = 2;
};

// Per-mix master volume (the app's scaled SNDMOD cmd 0x31 value, [0..1]).
// Default 1.0.
struct MixerState {
    std::vector<MixerChannel> channels;
    float masterVolume = 1.f;
    bool masterMono = false;  // global mono override (the `[sound]` master toggle)

    // Mix `frames` frames of audio into `out`. Each channel's pcm is read
    // forward by up to its remaining length (silence after end), and the
    // read cursor advances accordingly. Out-of-bounds writes are guarded:
    // we never write past samples.size(). Channels with a nullptr/empty
    // pcm are skipped. Each output sample is clamped to int16 range.
    // Returns the number of frames actually mixed (== frames, modulo the
    // padding-slop logic at the end of a stream).
    std::size_t mix(OutputBuffer& out, std::size_t frames) const;

    // Convenience: replace a channel's pcm pointer.
    void setChannelPcm(std::size_t i, const std::int16_t* pcm, std::size_t frames);

    // Convenience: set the channel's per-voice volume (ApplySoundVolumes
    // contracts cmd 0x21 to per-channel volume).
    void setVolume(std::size_t i, float v);

    // Convenience: toggle the channel's mono flag (matches the SNDMOD voice
    // being routed to one speaker only — we model it as "ignore pan and sum
    // to both outputs"). The [sound] settings row that the Options hook
    // opens maps "Mono" to true here for the channel it picks.
    void setMono(std::size_t i, bool mono);
};

}  // namespace rt::native::audio
