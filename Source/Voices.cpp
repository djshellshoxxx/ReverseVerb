// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "Voices.h"

namespace
{
    constexpr int kChunk = 256;

    inline float hermite (float ym1, float y0, float y1, float y2, float t)
    {
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    inline float readAt (const RenderedSample& r, int ch, double pos, bool interp)
    {
        const int total = r.audio.getNumSamples();
        const int i = (int) pos;
        if (pos < 0.0 || i >= total) return 0.0f;
        const float* s = r.audio.getReadPointer (juce::jmin (ch, 1));
        if (! interp) return s[i];
        auto at = [&] (int k) { return s[juce::jlimit (0, total - 1, k)]; };
        return hermite (at (i - 1), s[i], at (i + 1), at (i + 2), (float) (pos - i));
    }

    inline float gainAt (const RenderedSample& r, double pos, float dry, float wet)
    {
        const int hitAt = r.hitIndex >= 0 ? r.hitIndex : r.audio.getNumSamples();
        return (int) pos < hitAt ? wet : dry;
    }
}

double VoiceBank::rateForNote (int note, int rootNote)
{
    return juce::jlimit (0.25, 4.0, std::exp2 ((double) (note - rootNote) / 12.0));
}

VoiceStart VoiceBank::alignStart (int hitIndex, double rate)
{
    VoiceStart vs;
    if (hitIndex <= 0 || rate <= 0.0) return vs;
    // Reported latency L == hitIndex. The hit plays at hitIndex / rate after the note-on, but should land at L.
    const double d = (double) hitIndex - (double) hitIndex / rate;
    if (d >= 0.0) vs.delay = (int) std::lround (d);        // hit would arrive early: wait
    else          vs.startPos = -d * rate;                 // hit would arrive late: skip into the swell
    return vs;
}

void VoiceBank::prepare (double sampleRate)
{
    drySm.reset (sampleRate, 0.005);
    wetSm.reset (sampleRate, 0.005);
    gainsInitialised = false;
    fadeLen = juce::jmax (32, (int) (0.01 * sampleRate));
}

void VoiceBank::setGains (float dry, float wet)
{
    if (! gainsInitialised) { drySm.setCurrentAndTargetValue (dry); wetSm.setCurrentAndTargetValue (wet); gainsInitialised = true; }
    else                    { drySm.setTargetValue (dry); wetSm.setTargetValue (wet); }
}

void VoiceBank::start (float gain, double rate, VoiceStart vs)
{
    Voice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr) { target = &voices[0]; for (auto& v : voices) if (v.id < target->id) target = &v; }
    target->active = true; target->pos = vs.startPos; target->rate = rate; target->gain = gain; target->id = ++counter;
    target->delay = vs.delay; target->fadeFrom.reset(); target->fadeRemaining = 0;
}

void VoiceBank::stopAll()
{
    for (auto& v : voices) { v.active = false; v.fadeFrom.reset(); v.fadeRemaining = 0; v.delay = 0; }
}

void VoiceBank::onBufferSwap (const std::shared_ptr<const RenderedSample>& oldBuffer, const RenderedSample& newBuffer)
{
    if (oldBuffer == nullptr || oldBuffer->audio.getNumSamples() == 0) return;
    const bool sameRate = std::abs (oldBuffer->sampleRate - newBuffer.sampleRate) < 0.5;
    for (auto& v : voices)
    {
        if (! v.active || v.delay > 0) continue;
        if (! sameRate) { v.active = false; continue; }
        const double posOld = v.pos;
        double posNew = posOld;
        // keep the time-to-hit (or time-since-hit) the same so the hit stays continuous
        if (oldBuffer->hitIndex >= 0 && newBuffer.hitIndex >= 0)
            posNew = (double) newBuffer.hitIndex + (posOld - (double) oldBuffer->hitIndex);
        v.fadeFrom = oldBuffer;
        v.fadePos = posOld;
        v.fadeRemaining = fadeLen;
        v.pos = juce::jmax (0.0, posNew);
    }
}

void VoiceBank::render (juce::AudioBuffer<float>& out, const RenderedSample& r, int start, int num)
{
    if (num <= 0) return;
    const int total = r.audio.getNumSamples();
    const int hitAt = r.hitIndex >= 0 ? r.hitIndex : total;
    const int numCh = out.getNumChannels();
    std::array<float, kChunk> dryG, wetG;

    for (int base = 0; base < num; base += kChunk)
    {
        const int n = juce::jmin (kChunk, num - base);
        for (int i = 0; i < n; ++i) { dryG[(size_t) i] = drySm.getNextValue(); wetG[(size_t) i] = wetSm.getNextValue(); }
        const int o0 = start + base;

        for (auto& v : voices)
        {
            if (! v.active) continue;
            int i0 = 0;
            if (v.delay > 0) { i0 = juce::jmin (v.delay, n); v.delay -= i0; if (i0 >= n) continue; }

            if (v.rate == 1.0 && v.fadeRemaining == 0)
            {
                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* o = out.getWritePointer (ch) + o0;
                    const float* s = r.audio.getReadPointer (juce::jmin (ch, 1));
                    int pos = (int) v.pos;
                    for (int i = i0; i < n && pos < total; ++i, ++pos)
                        o[i] += s[pos] * (pos < hitAt ? wetG[(size_t) i] : dryG[(size_t) i]) * v.gain;
                }
                v.pos += (double) (n - i0);
            }
            else
            {
                const bool interp = v.rate != 1.0;
                for (int i = i0; i < n; ++i)
                {
                    const bool fading = v.fadeRemaining > 0 && v.fadeFrom != nullptr;
                    if (v.pos >= total && ! fading) break;
                    const float g = gainAt (r, v.pos, dryG[(size_t) i], wetG[(size_t) i]);
                    const float t = fading ? 1.0f - (float) v.fadeRemaining / (float) fadeLen : 1.0f;
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        float x = readAt (r, ch, v.pos, interp) * g;
                        if (fading)
                        {
                            const auto& o = *v.fadeFrom;
                            const float y = readAt (o, ch, v.fadePos, interp) * gainAt (o, v.fadePos, dryG[(size_t) i], wetG[(size_t) i]);
                            x = x * t + y * (1.0f - t);
                        }
                        out.getWritePointer (ch)[o0 + i] += x * v.gain;
                    }
                    v.pos += v.rate;
                    if (fading)
                    {
                        v.fadePos += v.rate;
                        if (--v.fadeRemaining <= 0) { v.fadeRemaining = 0; v.fadeFrom.reset(); }
                    }
                }
            }
            if (v.pos >= total && v.fadeRemaining <= 0) { v.active = false; v.fadeFrom.reset(); }
        }
    }
}

int VoiceBank::newestPosition() const
{
    const Voice* newest = nullptr;
    for (auto& v : voices) if (v.active && (newest == nullptr || v.id > newest->id)) newest = &v;
    return newest != nullptr ? (int) newest->pos : -1;
}
