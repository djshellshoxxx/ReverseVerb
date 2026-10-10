// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "dsp/RenderEngine.h"

struct Voice
{
    bool active = false;
    double pos = 0.0, rate = 1.0;
    float gain = 1.0f;
    juce::uint32 id = 0;
    int delay = 0;                                      // samples of silence before the voice starts (keytrack PDC alignment)
    std::shared_ptr<const RenderedSample> fadeFrom;     // previous buffer while crossfading after a re-render
    double fadePos = 0.0;
    int fadeRemaining = 0;
};

struct VoiceStart { int delay = 0; double startPos = 0.0; };

// Fixed pool of one-shot voices reading a RenderedSample. Audio-thread safe (no allocation).
// Gains are smoothed, buffer swaps crossfade, and voices can play at a pitch ratio (keytrack).
class VoiceBank
{
public:
    void prepare (double sampleRate);
    void setGains (float dry, float wet);                // call once per block
    void start (float gain, double rate = 1.0, VoiceStart vs = {});
    void stopAll();
    // Call when the render buffer changed: active voices crossfade from the old buffer to the new one.
    void onBufferSwap (const std::shared_ptr<const RenderedSample>& oldBuffer, const RenderedSample& newBuffer);
    void render (juce::AudioBuffer<float>& out, const RenderedSample& r, int start, int num);
    int newestPosition() const;                          // playhead of the newest active voice, or -1

    static double rateForNote (int note, int rootNote);  // 2^((note-root)/12), clamped to [0.25, 4]
    // Keeps the dry hit landing on the note when "hit on note" PDC is on and the voice is pitched.
    static VoiceStart alignStart (int hitIndex, double rate);

private:
    std::array<Voice, 8> voices;
    juce::uint32 counter = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> drySm, wetSm;
    bool gainsInitialised = false;
    int fadeLen = 441;
};
