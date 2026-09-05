#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace rv
{
// Granular overlap-add time-stretch: spreads `source` out to roughly
// `ratio` times its length while leaving pitch alone, so the source
// material itself - not just an appended reverb tail - can span a long,
// evolving riser or faller. ratio <= 1 (or a source too short to grain)
// returns the input unchanged.
[[nodiscard]] juce::AudioBuffer<float> timeStretch (const juce::AudioBuffer<float>& source,
                                                     double sampleRate,
                                                     double ratio) noexcept;
}
