#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "../Source/RenderedSample.h"

#include <cmath>

namespace
{
class DirectionTests final : public juce::UnitTest
{
public:
    DirectionTests() : juce::UnitTest ("Rendered layer mixing", "DSP") {}

    void runTest() override
    {
        beginTest ("Wet and dry gains affect only their own layers");

        RenderedSample rendered;
        rendered.wetAudio.setSize (2, 4);
        rendered.dryAudio.setSize (2, 4);
        rendered.wetAudio.clear();
        rendered.dryAudio.clear();
        rendered.wetAudio.setSample (0, 0, 1.0f);
        rendered.wetAudio.setSample (1, 0, 2.0f);
        rendered.dryAudio.setSample (0, 2, 3.0f);
        rendered.dryAudio.setSample (1, 2, 4.0f);

        juce::AudioBuffer<float> output (2, 4);
        output.clear();
        mixRenderedRange (output, rendered, 0, 0, 4, 0.25f, 0.5f);

        expectNear (output.getSample (0, 0), 0.25f);
        expectNear (output.getSample (1, 0), 0.5f);
        expectNear (output.getSample (0, 2), 1.5f);
        expectNear (output.getSample (1, 2), 2.0f);

        beginTest ("Overlapping wet and dry samples sum without cross-coupling");

        rendered.wetAudio.setSample (0, 1, 2.0f);
        rendered.dryAudio.setSample (0, 1, 5.0f);
        output.clear();
        mixRenderedRange (output, rendered, 1, 2, 1, 0.5f, 0.25f);
        expectNear (output.getSample (0, 2), 2.25f);
        expectNear (output.getSample (1, 2), 0.0f);

        beginTest ("Ranges are clipped safely at both buffer boundaries");

        output.clear();
        mixRenderedRange (output, rendered, -2, 0, 6, 1.0f, 1.0f);
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            for (int sample = 0; sample < output.getNumSamples(); ++sample)
                expect (std::isfinite (output.getSample (channel, sample)));

        beginTest ("Rise and Fall place wet and dry layers in direction order");

        const auto rise = calculateLayerTimeline (RenderDirection::rise, 100, 20, 7);
        expectEquals (rise.wetStart, 0);
        expectEquals (rise.dryStart, 107);
        expectEquals (rise.totalLength, 127);

        const auto fall = calculateLayerTimeline (RenderDirection::fall, 100, 20, 7);
        expectEquals (fall.dryStart, 0);
        expectEquals (fall.wetStart, 7);
        expectEquals (fall.totalLength, 107);

        beginTest ("Only aligned Rise rendering reports lookahead latency");

        rendered.direction = RenderDirection::rise;
        rendered.dryHitIndex = 321;
        expectEquals (latencySamplesFor (rendered, false), 0);
        expectEquals (latencySamplesFor (rendered, true), 321);
        rendered.direction = RenderDirection::fall;
        expectEquals (latencySamplesFor (rendered, false), 0);
        expectEquals (latencySamplesFor (rendered, true), 0);
    }

private:
    void expectNear (float actual, float expected)
    {
        expect (std::abs (actual - expected) <= 1.0e-6f,
                "Expected " + juce::String (expected) + ", got " + juce::String (actual));
    }
};

DirectionTests directionTests;
}
