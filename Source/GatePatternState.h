#pragma once

#include "GateEngine.h"

#include <juce_data_structures/juce_data_structures.h>

namespace rv
{
inline const juce::Identifier gatePatternStateType { "GATOR_PATTERN" };

[[nodiscard]] juce::ValueTree gatePatternToValueTree (const GatePattern& pattern);
[[nodiscard]] GatePattern gatePatternFromValueTree (const juce::ValueTree& state,
                                                     const GatePattern& fallback = {});
}
