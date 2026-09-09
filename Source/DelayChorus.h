#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace rv
{
// A single modulated delay line doubling as delay, chorus, or echo depending
// on its settings: short time + heavy modulation reads as chorus, longer time
// plus feedback reads as echo/delay, and settings in between blend the two.
// Processes in place, per channel, with the modulation phase offset between
// channels for stereo width. Placing this call before or after this plugin's
// own Rise-mode reversal step is what turns it into a *reverse* echo/delay/
// chorus - the caller decides that by choosing where to call it from.
void applyDelayChorus (juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate,
                       float timeMs, float feedback, float modRateHz, float modDepth, float mix) noexcept;
}
