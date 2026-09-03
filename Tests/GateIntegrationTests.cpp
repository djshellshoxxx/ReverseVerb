#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "../Source/GatePatternState.h"
#include "../Source/GatedMixer.h"

#include <cmath>

namespace
{
class GateIntegrationTests final : public juce::UnitTest
{
public:
    GateIntegrationTests() : juce::UnitTest ("Gate integration", "DSP") {}

    void runTest() override
    {
        testPatternStateRoundTrip();
        testMalformedStateFallback();
        testLayerTargets();
        testBlockPartitionEquivalence();
    }

private:
    void expectNear (float actual, float expected, float tolerance = 1.0e-6f)
    {
        expect (std::abs (actual - expected) <= tolerance,
                "Expected " + juce::String (expected, 6)
                    + ", got " + juce::String (actual, 6));
    }

    static rv::HostTiming timing()
    {
        rv::HostTiming result;
        result.bpm = 60.0;
        result.sampleRate = 1000.0;
        result.timeSignature = { 4, 4 };
        return result;
    }

    static RenderedSample layers (int samples)
    {
        RenderedSample rendered;
        rendered.wetAudio.setSize (1, samples);
        rendered.dryAudio.setSize (1, samples);
        rendered.wetAudio.clear();
        rendered.dryAudio.clear();
        for (int sample = 0; sample < samples; ++sample)
        {
            rendered.wetAudio.setSample (0, sample, 1.0f);
            rendered.dryAudio.setSample (0, sample, 2.0f);
        }
        return rendered;
    }

    void testPatternStateRoundTrip()
    {
        beginTest ("All 32 steps, active length, and random seed survive state round-trip");
        rv::GatePattern source;
        source.activeSteps = 32;
        source.seed = 0x1234abcdu;
        for (int step = 0; step < 32; ++step)
            source.steps[(size_t) step] = (float) step / 31.0f;

        const auto restored = rv::gatePatternFromValueTree (rv::gatePatternToValueTree (source));
        expectEquals (restored.activeSteps, 32);
        expectEquals ((int64_t) restored.seed, (int64_t) 0x1234abcdu);
        for (int step = 0; step < 32; ++step)
            expectNear (restored.steps[(size_t) step], (float) step / 31.0f);
    }

    void testMalformedStateFallback()
    {
        beginTest ("Malformed pattern state is finite, clamped, and keeps fallback values");
        rv::GatePattern fallback;
        fallback.steps.fill (0.25f);
        juce::ValueTree state (rv::gatePatternStateType);
        state.setProperty ("activeSteps", 27, nullptr);
        state.setProperty ("seed", "bad", nullptr);
        state.setProperty ("step0", -5.0, nullptr);
        state.setProperty ("step1", 8.0, nullptr);

        const auto restored = rv::gatePatternFromValueTree (state, fallback);
        expectEquals (restored.activeSteps, 32);
        expectEquals ((int64_t) restored.seed, (int64_t) fallback.seed);
        expectNear (restored.steps[0], 0.0f);
        expectNear (restored.steps[1], 1.0f);
        expectNear (restored.steps[2], 0.25f);
    }

    void testLayerTargets()
    {
        beginTest ("Gate target isolates Swell, Hit, Both, and bypass");
        const auto rendered = layers (4);
        rv::GatePattern pattern;
        pattern.steps.fill (0.0f);
        rv::GateSettings settings;
        settings.rate = rv::Division::quarter;
        settings.smoothingMilliseconds = 0.0f;
        juce::AudioBuffer<float> output (1, 4);
        rv::GateEngine engine;

        settings.target = rv::GateTarget::swell;
        output.clear();
        rv::mixGatedRenderedRange (output, rendered, 0, 0, 4, 1.0f, 1.0f,
                                   engine, pattern, settings, timing(), 0, 0, true);
        expectNear (output.getSample (0, 0), 2.0f);

        settings.target = rv::GateTarget::hit;
        output.clear();
        engine.reset();
        rv::mixGatedRenderedRange (output, rendered, 0, 0, 4, 1.0f, 1.0f,
                                   engine, pattern, settings, timing(), 0, 0, true);
        expectNear (output.getSample (0, 0), 1.0f);

        settings.target = rv::GateTarget::both;
        output.clear();
        engine.reset();
        rv::mixGatedRenderedRange (output, rendered, 0, 0, 4, 1.0f, 1.0f,
                                   engine, pattern, settings, timing(), 0, 0, true);
        expectNear (output.getSample (0, 0), 0.0f);

        output.clear();
        rv::mixGatedRenderedRange (output, rendered, 0, 0, 4, 1.0f, 1.0f,
                                   engine, pattern, settings, timing(), 0, 0, false);
        expectNear (output.getSample (0, 0), 3.0f);
    }

    void testBlockPartitionEquivalence()
    {
        beginTest ("One-shot export and partitioned live blocks use identical gate state");
        constexpr int sampleCount = 1536;
        const auto rendered = layers (sampleCount);
        rv::GatePattern pattern;
        rv::GateSettings settings;
        settings.rate = rv::Division::sixteenth;
        settings.smoothingMilliseconds = 4.0f;
        settings.target = rv::GateTarget::both;
        juce::AudioBuffer<float> oneShot (1, sampleCount), partitioned (1, sampleCount);
        oneShot.clear();
        partitioned.clear();
        rv::GateEngine exportGate, liveGate;

        rv::mixGatedRenderedRange (oneShot, rendered, 0, 0, sampleCount, 0.8f, 0.6f,
                                   exportGate, pattern, settings, timing(), 0, 0, true);
        rv::mixGatedRenderedRange (partitioned, rendered, 0, 0, 511, 0.8f, 0.6f,
                                   liveGate, pattern, settings, timing(), 0, 0, true);
        rv::mixGatedRenderedRange (partitioned, rendered, 511, 511, 257, 0.8f, 0.6f,
                                   liveGate, pattern, settings, timing(), 511, 511, true);
        rv::mixGatedRenderedRange (partitioned, rendered, 768, 768, sampleCount - 768, 0.8f, 0.6f,
                                   liveGate, pattern, settings, timing(), 768, 768, true);

        for (int sample = 0; sample < sampleCount; ++sample)
            expectNear (partitioned.getSample (0, sample), oneShot.getSample (0, sample));
    }
};

GateIntegrationTests gateIntegrationTests;
}
