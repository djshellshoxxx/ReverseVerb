#include "GatePatternState.h"

#include <cmath>

namespace rv
{
namespace
{
bool isNumeric (const juce::var& value) noexcept
{
    return value.isInt() || value.isInt64() || value.isDouble() || value.isBool();
}
}

juce::ValueTree gatePatternToValueTree (const GatePattern& pattern)
{
    const auto clean = sanitiseGatePattern (pattern);
    juce::ValueTree state (gatePatternStateType);
    state.setProperty ("activeSteps", clean.activeSteps, nullptr);
    state.setProperty ("seed", static_cast<juce::int64> (clean.seed), nullptr);
    for (int step = 0; step < (int) clean.steps.size(); ++step)
        state.setProperty ("step" + juce::String (step), (double) clean.steps[(size_t) step], nullptr);
    return state;
}

GatePattern gatePatternFromValueTree (const juce::ValueTree& state, const GatePattern& fallback)
{
    auto result = sanitiseGatePattern (fallback);
    if (! state.isValid() || ! state.hasType (gatePatternStateType))
        return result;

    const auto activeSteps = state.getProperty ("activeSteps");
    if (isNumeric (activeSteps))
        result.activeSteps = (int) activeSteps;

    const auto seed = state.getProperty ("seed");
    if (seed.isInt() || seed.isInt64())
    {
        const auto value = static_cast<juce::int64> (seed);
        if (value >= 0 && static_cast<juce::uint64> (value) <= 0xffffffffULL)
            result.seed = static_cast<std::uint32_t> (value);
    }

    for (int step = 0; step < (int) result.steps.size(); ++step)
    {
        const auto property = state.getProperty ("step" + juce::String (step));
        if (isNumeric (property))
            result.steps[(size_t) step] = (float) static_cast<double> (property);
    }
    return sanitiseGatePattern (result);
}
}
