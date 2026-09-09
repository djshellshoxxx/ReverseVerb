#pragma once

#include "Envelope.h"

#include <juce_data_structures/juce_data_structures.h>

namespace rv
{
inline const juce::Identifier volumeEnvelopeStateType { "VOLUME_ENVELOPE" };
inline const juce::Identifier panEnvelopeStateType { "PAN_ENVELOPE" };

[[nodiscard]] juce::ValueTree envelopeToValueTree (const juce::Identifier& type, const Envelope&,
                                                    float minValue, float maxValue);
[[nodiscard]] Envelope envelopeFromValueTree (const juce::Identifier& type, const juce::ValueTree& state,
                                              float minValue, float maxValue, const Envelope& fallback = {});
}
