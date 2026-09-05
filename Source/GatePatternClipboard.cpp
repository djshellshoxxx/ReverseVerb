#include "GatePatternClipboard.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace rv
{
juce::String encodeGatePattern (const GatePattern& pattern)
{
    const auto clean = sanitiseGatePattern (pattern);
    juce::String result = "RVGATE1|" + juce::String (clean.activeSteps)
                        + "|" + juce::String (static_cast<juce::int64> (clean.seed)) + "|";
    for (int step = 0; step < (int) clean.steps.size(); ++step)
    {
        if (step != 0)
            result += ",";
        result += juce::String (clean.steps[(size_t) step], 6);
    }
    return result;
}

std::optional<GatePattern> decodeGatePattern (const juce::String& text)
{
    juce::StringArray fields;
    fields.addTokens (text.trim(), "|", "");
    if (fields.size() != 4 || fields[0] != "RVGATE1")
        return std::nullopt;

    if (! fields[1].containsOnly ("0123456789"))
        return std::nullopt;
    const auto activeSteps = fields[1].getIntValue();
    if (activeSteps != 16 && activeSteps != 32)
        return std::nullopt;

    const auto seedText = fields[2].toRawUTF8();
    char* seedEnd = nullptr;
    errno = 0;
    const auto seed = std::strtoull (seedText, &seedEnd, 10);
    if (errno != 0 || seedEnd == seedText || *seedEnd != '\0'
        || seed > std::numeric_limits<std::uint32_t>::max())
        return std::nullopt;

    juce::StringArray values;
    values.addTokens (fields[3], ",", "");
    if (values.size() != 32)
        return std::nullopt;

    GatePattern result;
    result.activeSteps = activeSteps;
    result.seed = static_cast<std::uint32_t> (seed);
    for (int step = 0; step < 32; ++step)
    {
        const auto valueText = values[step].trim();
        const auto utf8 = valueText.toRawUTF8();
        char* valueEnd = nullptr;
        errno = 0;
        const auto value = std::strtof (utf8, &valueEnd);
        if (errno != 0 || valueEnd == utf8 || *valueEnd != '\0'
            || ! std::isfinite (value) || value < 0.0f || value > 1.0f)
            return std::nullopt;
        result.steps[(size_t) step] = value;
    }
    return result;
}
}
