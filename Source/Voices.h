// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "dsp/RenderEngine.h"

struct Voice { bool active = false; int pos = 0; float gain = 1.0f; juce::uint32 id = 0; };

// Fixed pool of one-shot voices reading a RenderedSample. Audio-thread safe (no allocation).
class VoiceBank
{
public:
    void start (float gain);
    void stopAll();
    void render (juce::AudioBuffer<float>& out, const RenderedSample& r, int start, int num, float dry, float wet);
    int newestPosition() const;          // playhead of the most recently started active voice, or -1

private:
    std::array<Voice, 8> voices;
    juce::uint32 counter = 0;
};
