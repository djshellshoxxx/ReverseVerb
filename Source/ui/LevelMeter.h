// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"

// Stereo output peak meter with a latching clip light (click to clear).
class LevelMeter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit LevelMeter (ReverseVerbProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { proc.clearClip(); repaint(); }
private:
    void timerCallback() override;
    ReverseVerbProcessor& proc;
    float level[2] { 0, 0 };
    bool clip = false;
};
