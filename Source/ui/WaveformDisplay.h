// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"

class WaveformDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit WaveformDisplay (ReverseVerbProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override { rebuild(); staticDirty = true; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
private:
    enum class Drag { none, trimEnd, trimStart, volStart, volEnd, volTension };
    void timerCallback() override;
    void rebuild();
    juce::Rectangle<float> plot() const;
    float volY (float level) const;
    void renderStatic (float scale);
    juce::Image staticImage;
    bool staticDirty = true;
    ReverseVerbProcessor& proc;
    std::shared_ptr<const RenderedSample> cached;
    juce::Path swellPath, hitPath;
    int total = 0, hitIndex = -1, lastPlayhead = -2;
    float lastTone = -1, lastBass = -1, lastV0 = -1, lastV1 = -1, lastVT = -9;
    Drag drag = Drag::none, hover = Drag::none;
    juce::Point<float> downPos;
    float downA = 0, downB = 0, downSpan = 1;
    bool moved = false;
};
