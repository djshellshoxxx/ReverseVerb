#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "../Source/TimeStretch.h"

#include <cmath>

namespace
{
class TimeStretchTests final : public juce::UnitTest
{
public:
    TimeStretchTests() : juce::UnitTest ("Time stretch", "DSP") {}

    void runTest() override
    {
        testRatioAtOrBelowUnityIsUnchanged();
        testOutputIsCappedRegardlessOfRatio();
        testOutputIsFiniteAndNonSilent();
    }

private:
    static juce::AudioBuffer<float> makeSource (int numSamples, juce::uint32 seed)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        juce::Random rng (seed);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
        return buffer;
    }

    void testRatioAtOrBelowUnityIsUnchanged()
    {
        beginTest ("A ratio of 1 (or below) leaves the buffer's length unchanged");
        const auto src = makeSource (2000, 1u);
        const auto out = rv::timeStretch (src, 44100.0, 1.0, 1'000'000);
        expectEquals (out.getNumSamples(), src.getNumSamples());
    }

    void testOutputIsCappedRegardlessOfRatio()
    {
        // Regression test for a real crash: an extreme ratio combined with a
        // longer source used to allocate the FULL uncapped output before any
        // trimming happened, and the function was noexcept - so a failed
        // allocation aborted the whole process instead of throwing. The cap
        // must apply before the big allocation, not after.
        beginTest ("Output length never exceeds maxOutputSamples, even at the maximum ratio on a long source");
        const auto src = makeSource (48000 * 5, 2u); // 5 seconds at 48kHz
        constexpr int cap = 44100 * 30; // well below 5s * 64x (320s), but above the source itself
        const auto out = rv::timeStretch (src, 48000.0, 64.0, cap);
        expect (out.getNumSamples() <= cap, "Stretched output exceeded the requested cap");
        expect (out.getNumSamples() > src.getNumSamples(), "Output should still be longer than the source");
    }

    void testOutputIsFiniteAndNonSilent()
    {
        beginTest ("Stretched output is finite and non-silent");
        const auto src = makeSource (4000, 3u);
        const auto out = rv::timeStretch (src, 44100.0, 8.0, 44100 * 10);
        expect (out.getNumSamples() > 0, "Expected non-empty output");
        float peak = 0.0f;
        for (int ch = 0; ch < out.getNumChannels(); ++ch)
        {
            const auto* d = out.getReadPointer (ch);
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                expect (std::isfinite (d[i]), "Non-finite sample in stretched output");
                peak = juce::jmax (peak, std::abs (d[i]));
            }
        }
        expect (peak > 0.001f, "Stretched output should not be silent");
    }
};

TimeStretchTests timeStretchTests;
}
