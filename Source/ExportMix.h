// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "OutputStage.h"
#include "dsp/RenderEngine.h"

// The exact mix-down used for every export (single WAV and batch): Swell/Hit levels, output gain, soft limiter.
// Shared so that a batch file is identical to the same sample exported on its own.
inline juce::AudioBuffer<float> mixForExport (const RenderedSample& r, float dry, float wet, float outGainDb, bool limiter)
{
    const int n = r.audio.getNumSamples();
    const int hitAt = r.hitIndex >= 0 ? r.hitIndex : n;
    juce::AudioBuffer<float> mix;
    mix.makeCopyOf (r.audio);
    for (int ch = 0; ch < 2; ++ch)
    {
        mix.applyGain (ch, 0, hitAt, wet);
        mix.applyGain (ch, hitAt, n - hitAt, dry);
    }
    const float og = juce::Decibels::decibelsToGain (outGainDb, -60.0f);
    if (og != 1.0f || limiter)
        for (int ch = 0; ch < 2; ++ch)
        {
            float* d = mix.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] = limiter ? softClip (d[i] * og) : d[i] * og;
        }
    return mix;
}
