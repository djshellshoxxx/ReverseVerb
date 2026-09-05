#include "DelayChorus.h"

#include <cmath>
#include <vector>

namespace rv
{
void applyDelayChorus (juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate,
                       float timeMs, float feedback, float modRateHz, float modDepth, float mix) noexcept
{
    if (mix <= 0.0005f || numSamples <= 0 || sampleRate <= 0.0)
        return;

    const float sr = (float) sampleRate;
    const float baseDelaySamples = juce::jmax (1.0f, timeMs * 0.001f * sr);
    const float modDepthSamples = juce::jlimit (0.0f, 1.0f, modDepth) * juce::jmin (baseDelaySamples * 0.9f, 0.02f * sr);
    const int delayLen = juce::jmax (4, (int) (baseDelaySamples + modDepthSamples) + 8);
    const float fb = juce::jlimit (0.0f, 0.95f, feedback);
    const float wet = juce::jlimit (0.0f, 1.0f, mix);
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        std::vector<float> line ((size_t) delayLen, 0.0f);
        int writePos = 0;
        float phase = ch % 2 == 1 ? juce::MathConstants<float>::halfPi : 0.0f; // stereo-offset modulation
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float modOffset = std::sin (phase) * modDepthSamples;
            phase += juce::MathConstants<float>::twoPi * modRateHz / sr;

            float rp = (float) writePos - (baseDelaySamples + modOffset);
            while (rp < 0.0f) rp += (float) delayLen;
            const int i0 = (int) rp % delayLen;
            const int i1 = (i0 + 1) % delayLen;
            const float frac = rp - std::floor (rp);
            const float delayed = line[(size_t) i0] + (line[(size_t) i1] - line[(size_t) i0]) * frac;

            const float input = data[i];
            line[(size_t) writePos] = input + delayed * fb;
            data[i] = input * (1.0f - wet) + delayed * wet;

            writePos = (writePos + 1) % delayLen;
        }
    }
}
}
