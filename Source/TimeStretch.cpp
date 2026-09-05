#include "TimeStretch.h"

#include <cmath>
#include <vector>

namespace rv
{
juce::AudioBuffer<float> timeStretch (const juce::AudioBuffer<float>& source, double sampleRate, double ratio, int maxOutputSamples)
{
    ratio = juce::jlimit (1.0, 64.0, ratio);
    const int numChannels = source.getNumChannels();
    const int srcLen = source.getNumSamples();
    if (numChannels <= 0 || srcLen < 8 || ratio <= 1.0 + 1.0e-6 || sampleRate <= 0.0)
        return source;

    // ~50ms grains, 50% overlap on the output side. Halving the analysis hop
    // relative to the synthesis hop is what makes the material stretch: the
    // same grain gets re-used (and cross-faded) more times the longer the
    // requested ratio, which is the classic granular time-stretch technique.
    const int windowSize = juce::jlimit (64, juce::jmax (64, srcLen), (int) std::lround (sampleRate * 0.05));
    const int synHop = juce::jmax (1, windowSize / 2);
    const double anaHop = (double) synHop / ratio;

    // Capped BEFORE allocating anything - an uncapped ratio*srcLen on a long
    // source could otherwise attempt a multi-gigabyte allocation.
    const int dstLen = juce::jlimit (1, juce::jmax (1, maxOutputSamples),
                                     (int) std::lround ((double) srcLen * ratio));
    juce::AudioBuffer<float> dst (numChannels, dstLen + windowSize);
    dst.clear();
    std::vector<float> weight ((size_t) (dstLen + windowSize), 0.0f);

    std::vector<float> window ((size_t) windowSize);
    for (int i = 0; i < windowSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (windowSize - 1));

    double inputPos = 0.0;
    int outputPos = 0;
    const int maxSrcOffset = juce::jmax (0, srcLen - windowSize);

    while (outputPos < dstLen)
    {
        const int srcOffset = juce::jlimit (0, maxSrcOffset, (int) std::lround (inputPos));
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* s = source.getReadPointer (ch);
            auto* d = dst.getWritePointer (ch);
            for (int i = 0; i < windowSize; ++i)
            {
                const int si = srcOffset + i;
                const float sample = si < srcLen ? s[si] : 0.0f;
                d[outputPos + i] += sample * window[(size_t) i];
            }
        }
        for (int i = 0; i < windowSize; ++i)
            weight[(size_t) (outputPos + i)] += window[(size_t) i];

        inputPos += anaHop;
        outputPos += synHop;
    }

    // 50%-overlap Hann sums to a near-constant envelope (COLA), but normalise
    // anyway so the buffer's start/end tapers don't dip in level.
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* d = dst.getWritePointer (ch);
        for (int i = 0; i < dstLen; ++i)
            if (weight[(size_t) i] > 0.0001f)
                d[i] /= weight[(size_t) i];
    }

    dst.setSize (numChannels, dstLen, true, true, true);
    return dst;
}
}
