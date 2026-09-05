#include <juce_core/juce_core.h>

#include "../Source/Envelope.h"

#include <cmath>

namespace
{
class EnvelopeTests final : public juce::UnitTest
{
public:
    EnvelopeTests() : juce::UnitTest ("Envelope", "DSP") {}

    void runTest() override
    {
        testNoInteriorPointsMatchesTwoPointFormula();
        testAddingNearbyPointDoesNotClobberADifferentValue();
        testCapacityIsRespected();
        testRemoveNearestPoint();
        testSanitiseClampsAndSorts();
    }

private:
    void expectNear (float actual, float expected, float tolerance = 1.0e-4f)
    {
        expect (std::abs (actual - expected) <= tolerance,
                "Expected " + juce::String (expected, 6) + ", got " + juce::String (actual, 6));
    }

    void testNoInteriorPointsMatchesTwoPointFormula()
    {
        beginTest ("With no interior points, evaluation matches the plain start/end/tension formula");
        rv::Envelope env;
        const float start = 0.2f, end = 0.9f, tension = 0.4f;
        for (float t = 0.0f; t <= 1.0f; t += 0.1f)
        {
            // Mirrors PluginProcessor.h's tensionCurve for t>=0 (same formula).
            const float k = 1.0f + 5.0f * std::abs (tension);
            const float curve = tension > 0.0f ? std::pow (t, k) : 1.0f - std::pow (1.0f - t, k);
            const float expected = start + (end - start) * curve;
            expectNear (rv::envelopeValueAt (env, t, start, end, tension), expected);
        }
    }

    void testAddingNearbyPointDoesNotClobberADifferentValue()
    {
        beginTest ("Adding a point near an existing one in X does not overwrite it if the value is far away");
        // Regression test: withInteriorPoint used to merge by X-position alone,
        // so clicking anywhere in the same vertical strip as an existing point
        // (even far above/below it) would silently overwrite that point.
        auto env = rv::withInteriorPoint (rv::Envelope {}, 0.5f, 1.0f, 0.0f, 1.0f);
        expectEquals (env.numInterior, 1);
        env = rv::withInteriorPoint (env, 0.505f, 0.0f, 0.0f, 1.0f);
        expectEquals (env.numInterior, 2);
        bool foundHigh = false, foundLow = false;
        for (int i = 0; i < env.numInterior; ++i)
        {
            if (std::abs (env.interior[(size_t) i].value - 1.0f) < 1.0e-4f) foundHigh = true;
            if (std::abs (env.interior[(size_t) i].value - 0.0f) < 1.0e-4f) foundLow = true;
        }
        expect (foundHigh, "The original point (value 1.0) should still exist");
        expect (foundLow, "The new point (value 0.0) should have been added, not merged");
    }

    void testCapacityIsRespected()
    {
        beginTest ("withInteriorPoint stops adding once at capacity, without disturbing existing points");
        rv::Envelope env;
        for (int i = 0; i < rv::Envelope::maxInteriorPoints; ++i)
            env = rv::withInteriorPoint (env, 0.02f + 0.9f * (float) i / (float) rv::Envelope::maxInteriorPoints, 0.5f, 0.0f, 1.0f);
        expectEquals (env.numInterior, rv::Envelope::maxInteriorPoints);
        const auto before = env;
        env = rv::withInteriorPoint (env, 0.99f, 0.9f, 0.0f, 1.0f);
        expectEquals (env.numInterior, before.numInterior);
        for (int i = 0; i < env.numInterior; ++i)
        {
            expectNear (env.interior[(size_t) i].pos, before.interior[(size_t) i].pos);
            expectNear (env.interior[(size_t) i].value, before.interior[(size_t) i].value);
        }
    }

    void testRemoveNearestPoint()
    {
        beginTest ("withoutNearestInteriorPoint removes only the closest point");
        auto env = rv::withInteriorPoint (rv::Envelope {}, 0.2f, 0.1f, 0.0f, 1.0f);
        env = rv::withInteriorPoint (env, 0.8f, 0.9f, 0.0f, 1.0f);
        expectEquals (env.numInterior, 2);
        env = rv::withoutNearestInteriorPoint (env, 0.21f);
        expectEquals (env.numInterior, 1);
        expectNear (env.interior[0].pos, 0.8f);
    }

    void testSanitiseClampsAndSorts()
    {
        beginTest ("sanitiseEnvelope clamps values and sorts interior points by position");
        rv::Envelope env;
        env.numInterior = 2;
        env.interior[0] = { 0.9f, 5.0f };   // value out of [0,1], and out of order
        env.interior[1] = { 0.1f, -3.0f };
        const auto clean = rv::sanitiseEnvelope (env, 0.0f, 1.0f);
        expectEquals (clean.numInterior, 2);
        expect (clean.interior[0].pos < clean.interior[1].pos, "Points should be sorted by position");
        expectNear (clean.interior[0].value, 0.0f);
        expectNear (clean.interior[1].value, 1.0f);
    }
};

EnvelopeTests envelopeTests;
}
