// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>

// Freeverb-style reverb with size / decay / damp / diffusion / separation / width / early reflections.
namespace rvdsp
{
struct Comb
{
    std::vector<float> buf; int idx = 0; float store = 0, fb = 0, d1 = 0, d2 = 1;
    void setup (int n, float feedback, float damp) { buf.assign ((size_t) juce::jmax (1, n), 0.0f); idx = 0; store = 0; fb = feedback; d1 = damp; d2 = 1.0f - damp; }
    float process (float in)
    {
        const float out = buf[(size_t) idx];
        store = out * d2 + store * d1;
        buf[(size_t) idx] = in + store * fb;
        if (++idx >= (int) buf.size()) idx = 0;
        return out;
    }
};
struct Allpass
{
    std::vector<float> buf; int idx = 0; float g = 0.5f;
    void setup (int n, float gain) { buf.assign ((size_t) juce::jmax (1, n), 0.0f); idx = 0; g = gain; }
    float process (float in)
    {
        const float b = buf[(size_t) idx];
        const float out = -in + b;
        buf[(size_t) idx] = in + b * g;
        if (++idx >= (int) buf.size()) idx = 0;
        return out;
    }
};

struct ReverbEngine
{
    std::array<Comb, 8> combL, combR;
    std::array<Allpass, 4> apL, apR;
    std::vector<float> erBufL, erBufR; int erIdx = 0;
    std::array<int, 8> erTaps {};
    std::array<float, 8> erGain {};
    float wet1 = 1, wet2 = 0, erLevel = 0;

    void setup (double sr, float size, float decay, float damp, float diff, float sep, float width, float er)
    {
        const int combBase[8]  = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        const int apBase[4]    = { 556, 441, 341, 225 };
        const float scale      = (float) (sr / 44100.0) * (0.45f + 1.3f * size);
        const int spread       = (int) (sep * 60.0f * sr / 44100.0);
        const float feedback   = 0.62f + 0.36f * decay;
        const float apGain     = 0.15f + 0.6f * diff;
        for (int i = 0; i < 8; ++i)
        {
            combL[(size_t) i].setup ((int) (combBase[i] * scale), feedback, damp * 0.9f);
            combR[(size_t) i].setup ((int) (combBase[i] * scale) + spread, feedback, damp * 0.9f);
        }
        for (int i = 0; i < 4; ++i)
        {
            apL[(size_t) i].setup ((int) (apBase[i] * scale), apGain);
            apR[(size_t) i].setup ((int) (apBase[i] * scale) + spread / 2, apGain);
        }
        wet1 = width * 0.5f + 0.5f;
        wet2 = (1.0f - width) * 0.5f;
        erLevel = er;
        const float erMs[8] = { 7.0f, 11.0f, 17.0f, 23.0f, 29.0f, 37.0f, 43.0f, 53.0f };
        int maxTap = 1;
        for (int i = 0; i < 8; ++i)
        {
            erTaps[(size_t) i] = (int) (erMs[i] * 0.001f * sr * (0.5f + size));
            erGain[(size_t) i] = 0.8f * std::pow (0.78f, (float) i);
            maxTap = juce::jmax (maxTap, erTaps[(size_t) i]);
        }
        erBufL.assign ((size_t) maxTap + 1, 0.0f);
        erBufR.assign ((size_t) maxTap + 1, 0.0f);
        erIdx = 0;
    }

    void process (float* l, float* r, int n)
    {
        const int erLen = (int) erBufL.size();
        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i], inR = r[i];
            const float mono = (inL + inR) * 0.015f;
            float oL = 0, oR = 0;
            for (auto& c : combL) oL += c.process (mono);
            for (auto& c : combR) oR += c.process (mono);
            for (auto& a : apL) oL = a.process (oL);
            for (auto& a : apR) oR = a.process (oR);

            erBufL[(size_t) erIdx] = inL; erBufR[(size_t) erIdx] = inR;
            float eL = 0, eR = 0;
            for (int t = 0; t < 8; ++t)
            {
                int p = erIdx - erTaps[(size_t) t]; if (p < 0) p += erLen;
                const float g = erGain[(size_t) t];
                if ((t & 1) == 0) { eL += erBufL[(size_t) p] * g; eR += erBufR[(size_t) p] * g * 0.6f; }
                else              { eR += erBufR[(size_t) p] * g; eL += erBufL[(size_t) p] * g * 0.6f; }
            }
            if (++erIdx >= erLen) erIdx = 0;

            l[i] = oL * wet1 + oR * wet2 + eL * erLevel * 0.5f;
            r[i] = oR * wet1 + oL * wet2 + eR * erLevel * 0.5f;
        }
    }
};
}
