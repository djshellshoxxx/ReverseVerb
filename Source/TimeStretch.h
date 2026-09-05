#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace rv
{
// Granular overlap-add time-stretch: spreads `source` out to roughly
// `ratio` times its length while leaving pitch alone, so the source
// material itself - not just an appended reverb tail - can span a long,
// evolving riser or faller. ratio <= 1 (or a source too short to grain)
// returns the input unchanged. The output is capped at maxOutputSamples
// (checked before any large allocation happens, not after) so an extreme
// ratio on a long source can't attempt a runaway allocation.
// Not noexcept: an allocation can still legitimately fail (e.g. under real
// memory pressure); callers should catch std::bad_alloc and degrade rather
// than let it escape as an unhandled abort.
[[nodiscard]] juce::AudioBuffer<float> timeStretch (const juce::AudioBuffer<float>& source,
                                                     double sampleRate,
                                                     double ratio,
                                                     int maxOutputSamples);
}
