#include "Envelope.h"

#include <algorithm>
#include <cmath>

namespace rv
{
namespace
{
// Local copy of PluginProcessor.h's FL-style tension curve, kept private here
// to avoid a circular include between Envelope.h and PluginProcessor.h.
float tensionCurveLocal (float x, float t) noexcept
{
    x = std::clamp (x, 0.0f, 1.0f);
    if (std::abs (t) < 0.001f) return x;
    const float k = 1.0f + 5.0f * std::abs (t);
    return t > 0.0f ? std::pow (x, k) : 1.0f - std::pow (1.0f - x, k);
}
}

Envelope sanitiseEnvelope (const Envelope& env, float minValue, float maxValue) noexcept
{
    Envelope result;
    const int count = std::clamp (env.numInterior, 0, Envelope::maxInteriorPoints);
    for (int i = 0; i < count; ++i)
    {
        auto p = env.interior[(size_t) i];
        if (! std::isfinite (p.pos) || ! std::isfinite (p.value))
            continue;
        p.pos = std::clamp (p.pos, 0.01f, 0.99f);
        p.value = std::clamp (p.value, minValue, maxValue);
        result.interior[(size_t) result.numInterior++] = p;
    }
    std::sort (result.interior.begin(), result.interior.begin() + result.numInterior,
               [] (const EnvelopePoint& a, const EnvelopePoint& b) { return a.pos < b.pos; });
    return result;
}

float envelopeValueAt (const Envelope& env, float t, float startValue, float endValue, float tension) noexcept
{
    t = std::clamp (t, 0.0f, 1.0f);
    float prevPos = 0.0f, prevValue = startValue;
    for (int i = 0; i < env.numInterior; ++i)
    {
        const auto& p = env.interior[(size_t) i];
        if (t <= p.pos)
        {
            const float span = p.pos - prevPos;
            const float local = span > 0.0001f ? (t - prevPos) / span : 0.0f;
            return prevValue + (p.value - prevValue) * tensionCurveLocal (local, tension);
        }
        prevPos = p.pos;
        prevValue = p.value;
    }
    const float span = 1.0f - prevPos;
    const float local = span > 0.0001f ? (t - prevPos) / span : 0.0f;
    return prevValue + (endValue - prevValue) * tensionCurveLocal (local, tension);
}

Envelope withInteriorPoint (const Envelope& env, float pos, float value,
                            float minValue, float maxValue, float tolerance) noexcept
{
    auto clean = sanitiseEnvelope (env, minValue, maxValue);
    pos = std::clamp (pos, 0.01f, 0.99f);
    value = std::clamp (value, minValue, maxValue);

    for (int i = 0; i < clean.numInterior; ++i)
        if (std::abs (clean.interior[(size_t) i].pos - pos) < tolerance)
        {
            clean.interior[(size_t) i].value = value;
            return clean;
        }

    if (clean.numInterior >= Envelope::maxInteriorPoints)
        return clean;

    clean.interior[(size_t) clean.numInterior++] = { pos, value };
    return sanitiseEnvelope (clean, minValue, maxValue);
}

Envelope withoutNearestInteriorPoint (const Envelope& env, float pos, float tolerance) noexcept
{
    auto clean = sanitiseEnvelope (env, -1.0e9f, 1.0e9f);
    int best = -1;
    float bestDist = tolerance;
    for (int i = 0; i < clean.numInterior; ++i)
    {
        const float d = std::abs (clean.interior[(size_t) i].pos - pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    if (best < 0)
        return clean;
    for (int i = best; i < clean.numInterior - 1; ++i)
        clean.interior[(size_t) i] = clean.interior[(size_t) (i + 1)];
    --clean.numInterior;
    return clean;
}

int nearestInteriorPoint (const Envelope& env, float pos, float value, float valueSpan, float tolerance) noexcept
{
    int best = -1;
    float bestDist = tolerance;
    const float vSpan = valueSpan > 0.0001f ? valueSpan : 1.0f;
    for (int i = 0; i < env.numInterior; ++i)
    {
        const auto& p = env.interior[(size_t) i];
        const float dx = p.pos - pos;
        const float dy = (p.value - value) / vSpan;
        const float d = std::sqrt (dx * dx + dy * dy);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}
}
