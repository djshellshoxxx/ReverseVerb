#pragma once

#include "GateEngine.h"

#include <juce_core/juce_core.h>

#include <optional>

namespace rv
{
[[nodiscard]] juce::String encodeGatePattern (const GatePattern& pattern);
[[nodiscard]] std::optional<GatePattern> decodeGatePattern (const juce::String& text);
}
