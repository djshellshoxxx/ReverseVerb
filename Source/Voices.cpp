// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "Voices.h"

void VoiceBank::start (float gain)
{
    Voice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr) { target = &voices[0]; for (auto& v : voices) if (v.id < target->id) target = &v; }
    target->active = true; target->pos = 0; target->gain = gain; target->id = ++counter;
}

void VoiceBank::render (juce::AudioBuffer<float>& out, const RenderedSample& r, int start, int num, float dry, float wet)
{
    if (num <= 0) return;
    const int total = r.audio.getNumSamples();
    const int hitAt = r.hitIndex >= 0 ? r.hitIndex : total;
    const int numCh = out.getNumChannels();
    for (auto& v : voices)
    {
        if (! v.active) continue;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* o = out.getWritePointer (ch) + start;
            const float* s = r.audio.getReadPointer (juce::jmin (ch, 1));
            int pos = v.pos;
            for (int i = 0; i < num && pos < total; ++i, ++pos)
                o[i] += s[pos] * (pos < hitAt ? wet : dry) * v.gain;
        }
        v.pos += num;
        if (v.pos >= total) v.active = false;
    }
}

void VoiceBank::stopAll() { for (auto& v : voices) v.active = false; }

int VoiceBank::newestPosition() const
{
    const Voice* newest = nullptr;
    for (auto& v : voices) if (v.active && (newest == nullptr || v.id > newest->id)) newest = &v;
    return newest != nullptr ? newest->pos : -1;
}
