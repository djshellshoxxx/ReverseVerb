#include "EnvelopeState.h"

namespace rv
{
namespace
{
bool isNumeric (const juce::var& value) noexcept
{
    return value.isInt() || value.isInt64() || value.isDouble() || value.isBool();
}
}

juce::ValueTree envelopeToValueTree (const juce::Identifier& type, const Envelope& env,
                                     float minValue, float maxValue)
{
    const auto clean = sanitiseEnvelope (env, minValue, maxValue);
    juce::ValueTree state (type);
    state.setProperty ("numInterior", clean.numInterior, nullptr);
    for (int i = 0; i < clean.numInterior; ++i)
    {
        state.setProperty ("pos" + juce::String (i), (double) clean.interior[(size_t) i].pos, nullptr);
        state.setProperty ("value" + juce::String (i), (double) clean.interior[(size_t) i].value, nullptr);
    }
    return state;
}

Envelope envelopeFromValueTree (const juce::Identifier& type, const juce::ValueTree& state,
                                float minValue, float maxValue, const Envelope& fallback)
{
    auto result = sanitiseEnvelope (fallback, minValue, maxValue);
    if (! state.isValid() || ! state.hasType (type))
        return result;

    const auto countProp = state.getProperty ("numInterior");
    const int count = isNumeric (countProp) ? juce::jlimit (0, Envelope::maxInteriorPoints, (int) countProp) : 0;

    Envelope loaded;
    for (int i = 0; i < count; ++i)
    {
        const auto posProp = state.getProperty ("pos" + juce::String (i));
        const auto valueProp = state.getProperty ("value" + juce::String (i));
        if (! isNumeric (posProp) || ! isNumeric (valueProp))
            continue;
        loaded.interior[(size_t) loaded.numInterior++] = { (float) static_cast<double> (posProp),
                                                            (float) static_cast<double> (valueProp) };
    }
    return sanitiseEnvelope (loaded, minValue, maxValue);
}
}
