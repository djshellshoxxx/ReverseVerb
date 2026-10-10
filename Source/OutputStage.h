// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>

// Transparent below ~-1 dBFS, smooth saturation toward a 0.99 ceiling above. Stateless, zero latency.
inline float softClip (float x)
{
    constexpr float t = 0.89f, c = 0.99f, k = c - t;
    const float a = std::abs (x);
    if (a <= t) return x;
    const float y = t + k * std::tanh ((a - t) / k);
    return x < 0.0f ? -y : y;
}

// Output gain (smoothed) + safety soft-clip + peak metering. Audio-thread safe.
class OutputStage
{
public:
    void prepare (double sampleRate) { gainSm.reset (sampleRate, 0.005); initialised = false; }

    void process (juce::AudioBuffer<float>& buf, float gainDb, bool limiter)
    {
        const float target = juce::Decibels::decibelsToGain (gainDb, -60.0f);
        if (! initialised) { gainSm.setCurrentAndTargetValue (target); initialised = true; }
        else               gainSm.setTargetValue (target);

        const int n = buf.getNumSamples(), numCh = juce::jmin (buf.getNumChannels(), 2);
        if (gainSm.isSmoothing() || gainSm.getTargetValue() != 1.0f || limiter)
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = gainSm.getNextValue();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* d = buf.getWritePointer (ch);
                    d[i] = limiter ? softClip (d[i] * g) : d[i] * g;
                }
            }
        }
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float p = buf.getMagnitude (ch, 0, n);
            if (p > peak[(size_t) ch].load()) peak[(size_t) ch].store (p);
            if (p > 0.985f) clipped = true;
        }
    }

    float takePeak (int ch) { return peak[(size_t) juce::jlimit (0, 1, ch)].exchange (0.0f); }
    bool clipLatched() const { return clipped.load(); }
    void clearClip() { clipped = false; }

private:
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSm;
    bool initialised = false;
    std::array<std::atomic<float>, 2> peak {};
    std::atomic<bool> clipped { false };
};
