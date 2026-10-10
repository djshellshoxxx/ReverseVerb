// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>

// FL-style tension curve: x in 0..1 -> 0..1. t>0 = slow start, t<0 = fast start.
inline float tensionCurve (float x, float t)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    if (std::abs (t) < 0.001f) return x;
    const float k = 1.0f + 5.0f * std::abs (t);
    return t > 0.0f ? std::pow (x, k) : 1.0f - std::pow (1.0f - x, k);
}

struct RenderedSample
{
    juce::AudioBuffer<float> audio;     // final playable buffer (stereo)
    int hitIndex = -1;                  // sample where the dry hit starts, -1 if trimmed out
    double sampleRate = 44100.0;
    int beats = 0;                      // >0 when synced: draw this many beat lines
    int beatsPerBar = 4;
    double fullLengthSec = 0.0;         // untrimmed swell+hit length
    double trimStartSec = 0.0, trimEndSec = 0.0;
    std::vector<float> pitchSemi;       // one entry per envStep samples
    std::vector<float> gainLin;         // one entry per envStep samples
    static constexpr int envStep = 256;
};

// Plain snapshot of every parameter that affects rendering. No access to the processor or APVTS,
// so it can be built on any thread (live render, batch export, tests).
struct RenderSettings
{
    double sampleRate = 44100.0, bpm = 120.0;
    float size = 0.7f, decay = 0.8f, damp = 0.4f, diff = 0.6f, er = 0.3f, sep = 0.4f, width = 1.0f;
    float gap = 0.0f, tail = 1.2f, shape = 0.0f, tone = 20000.0f, basscut = 20.0f;
    float trimStart = 0.0f, trimEnd = 1.0f;
    float pitch = 0.0f, pitchTension = 0.0f, volStart = 1.0f, volEnd = 1.0f, volTension = 0.0f;
    int syncLen = 2, pitchRange = 0;
    bool sync = false;
};

struct SourceAudio
{
    std::shared_ptr<const juce::AudioBuffer<float>> buffer;
    double sampleRate = 0.0;
    int version = 0;                    // bump whenever the audio changes (cache key)
};

// Cached stage 1-7 (resample, reverb, reverse, filters, shape, combine). Trim / pitch / volume edits reuse it.
struct RenderCache
{
    std::array<double, 17> key {};
    juce::AudioBuffer<float> full;
    int hitLen = 0, swellLen = 0, beats = 0;
    bool valid = false;
};

// Pure function: same inputs -> same output. Pass a cache to make repeated renders cheap.
std::shared_ptr<RenderedSample> renderSample (const RenderSettings&, const SourceAudio&, RenderCache* cache = nullptr);
