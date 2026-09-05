#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "../Source/SampleGenerator.h"

#include <array>
#include <cmath>

namespace
{
class SampleGeneratorTests final : public juce::UnitTest
{
public:
    SampleGeneratorTests() : juce::UnitTest ("Sample generator", "DSP") {}

    void runTest() override
    {
        testProducesFiniteAudibleAudio();
        testSeedIsReproducible();
        testDifferentSeedsVary();
        testNameForEveryType();
    }

private:
    static constexpr std::array<rv::GeneratedSampleType, 3> allTypes {
        rv::GeneratedSampleType::snare, rv::GeneratedSampleType::hat, rv::GeneratedSampleType::clap
    };

    void testProducesFiniteAudibleAudio()
    {
        beginTest ("Every generated type renders finite, bounded, non-silent stereo audio");

        for (const auto type : allTypes)
        {
            const auto buffer = rv::generateSample (type, 44100.0, 12345u);
            expect (buffer.getNumChannels() == 2);
            expect (buffer.getNumSamples() > 0);

            float peak = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto* data = buffer.getReadPointer (channel);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    expect (std::isfinite (data[i]), "Generated sample contains a non-finite value");
                    peak = juce::jmax (peak, std::abs (data[i]));
                }
            }
            expect (peak > 0.01f, "Generated sample is effectively silent");
            expect (peak <= 1.0f + 1.0e-4f, "Generated sample exceeds unity gain");
        }
    }

    void testSeedIsReproducible()
    {
        beginTest ("The same seed renders identical audio");

        const auto a = rv::generateSample (rv::GeneratedSampleType::snare, 48000.0, 777u);
        const auto b = rv::generateSample (rv::GeneratedSampleType::snare, 48000.0, 777u);
        expect (a.getNumSamples() == b.getNumSamples());
        for (int i = 0; i < a.getNumSamples(); ++i)
            expect (a.getSample (0, i) == b.getSample (0, i));
    }

    void testDifferentSeedsVary()
    {
        beginTest ("Different seeds render audibly different audio");

        for (const auto type : allTypes)
        {
            const auto a = rv::generateSample (type, 44100.0, 1u);
            const auto b = rv::generateSample (type, 44100.0, 2u);
            const auto n = juce::jmin (a.getNumSamples(), b.getNumSamples());
            double diff = 0.0;
            for (int i = 0; i < n; ++i)
                diff += std::abs ((double) a.getSample (0, i) - (double) b.getSample (0, i));
            expect (diff > 0.0, "Two different seeds produced identical audio");
        }
    }

    void testNameForEveryType()
    {
        beginTest ("Every generated type has a human-readable name");

        for (const auto type : allTypes)
            expect (juce::String (rv::generatedSampleName (type)).isNotEmpty());
    }
};

SampleGeneratorTests sampleGeneratorTests;
}
