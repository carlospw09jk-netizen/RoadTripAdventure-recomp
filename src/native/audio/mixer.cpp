// Authored mixer model implementation. See mixer.h.
//
// Mix math, per output frame, per channel:
//   src   = (channel.pcm != null && cursor < channel.pcmFrames)
//             ? (int32)channel.pcm[cursor++] : 0
//   gain  = masterVolume * channel.volume * (channel.mono ? 0.5 : 1.0)
//   Lgain = channel.mono ? 0.5 : PanLaw::left(channel.pan)        (== 0.5 when centred)
//   Rgain = channel.mono ? 0.5 : PanLaw::right(channel.pan)
//   sumL += src * gain * Lgain
//   sumR += src * gain * Rgain
// After all channels:
//   out[2*k + 0] = saturate16(sumL)
//   out[2*k + 1] = saturate16(sumR)
//
// The mono flag zeroes out the pan split (so a mono voice with full volume
// and pan=0 produces equal L and R at half amplitude each — sum to the same
// loudness as a stereo-centred channel). The master mono flag mirrors that
// behaviour globally for the same reason.
#include "audio/mixer.h"

#include <algorithm>

namespace rt::native::audio {
namespace {

inline std::int32_t saturate16(std::int32_t s) {
    if (s >  32767) return  32767;
    if (s < -32768) return -32768;
    return s;
}

}  // namespace

void MixerState::setChannelPcm(std::size_t i, const std::int16_t* pcm, std::size_t frames) {
    if (i >= channels.size()) return;
    channels[i].pcm = pcm;
    channels[i].pcmFrames = frames;
    channels[i].readCursor = 0;
}

void MixerState::setVolume(std::size_t i, float v) {
    if (i >= channels.size()) return;
    channels[i].volume = v;
}

void MixerState::setMono(std::size_t i, bool mono) {
    if (i >= channels.size()) return;
    channels[i].mono = mono;
}

std::size_t MixerState::mix(OutputBuffer& out, std::size_t frames) const {
    if (frames == 0 || out.channels == 0) return 0;
    const std::size_t wanted = frames * out.channels;
    // Out buffer is caller-owned; we resize to fit. This matches the test
    // harness's "let me hand you a half-formed buffer and let the mixer
    // finish it" pattern, and avoids a special case for test buffers that
    // want to compare against a fresh vector each tick.
    if (out.samples.size() < wanted) out.samples.resize(wanted, 0);
    // A writable view over the first `wanted` slots; the rest is left
    // untouched (zero on a fresh resize).
    std::int16_t* dst = out.samples.data();

    // The starting read cursor is captured per channel up front. We write
    // straight to the channel's cursor as we go so a long-running mixer
    // doesn't have to copy state in and out every frame.
    std::vector<std::size_t> cursors(channels.size());
    for (std::size_t c = 0; c < channels.size(); ++c) {
        cursors[c] = channels[c].readCursor;
    }

    for (std::size_t f = 0; f < frames; ++f) {
        std::int32_t accL = 0;
        std::int32_t accR = 0;
        for (std::size_t c = 0; c < channels.size(); ++c) {
            const MixerChannel& ch = channels[c];
            const std::int16_t* p = ch.pcm;
            const std::size_t cursor = cursors[c];
            std::int32_t src = 0;
            if (p != nullptr && cursor < ch.pcmFrames) {
                src = static_cast<std::int32_t>(p[cursor]);
                cursors[c] = cursor + 1;
            }

            const float masterMul = std::max(0.f, masterVolume);
            const float base      = masterMul * ch.volume;
            float leftGain;
            float rightGain;
            if (masterMono || ch.mono) {
                // Mono override: ignore pan and put half the source in each
                // side. Total energy matches a centred stereo channel
                // (which puts the full source in each side, summing to 2*
                // the source; the mono half-and-half sums to the same).
                leftGain  = base * 0.5f;
                rightGain = base * 0.5f;
            } else {
                leftGain  = base * PanLaw::left(ch.pan);
                rightGain = base * PanLaw::right(ch.pan);
            }

            accL += static_cast<std::int32_t>(src * leftGain);
            accR += static_cast<std::int32_t>(src * rightGain);
        }
        // Write to interleaved sample slots (always indexable: out.samples
        // grew above). Stereo layout: frames of `out.channels` interleaved
        // by slot. A 1-channel output is the conventional stereo-downmix
        // mono fold (L + R) / 2, which preserves a centred channel's
        // loudness rather than doubling it.
        if (out.channels == 1) {
            const std::int32_t mono = (accL + accR + 1) >> 1;
            dst[f] = static_cast<std::int16_t>(saturate16(mono));
        } else {
            for (std::size_t ch = 0; ch < out.channels; ++ch) {
                const std::int32_t v = (ch == 0) ? accL : accR;
                dst[f * out.channels + ch] =
                    static_cast<std::int16_t>(saturate16(v));
            }
        }
    }

    // Commit the cursors back into the channel state. Cast away const: the
    // mixer is logically mutable here (run-forward state), but `mix()` is
    // const so callers can keep the state in a const slot during a tick.
    for (std::size_t c = 0; c < channels.size(); ++c) {
        MixerChannel& ch = const_cast<MixerChannel&>(channels[c]);
        ch.readCursor = cursors[c];
    }

    return frames;
}

}  // namespace rt::native::audio
