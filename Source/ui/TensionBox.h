// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"

class TensionBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    TensionBox (ReverseVerbProcessor& p, const juce::String& id) : proc (p), paramId (id) { startTimerHz (15); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { downT = proc.param (paramId); downY = e.y; }
    void mouseDrag (const juce::MouseEvent& e) override { proc.setParam (paramId, juce::jlimit (-1.0f, 1.0f, downT + (float) (downY - e.y) / 60.0f)); repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { proc.setParam (paramId, 0.0f); repaint(); }
private:
    void timerCallback() override { const float t = proc.param (paramId); if (t != shown) { shown = t; repaint(); } }
    ReverseVerbProcessor& proc;
    juce::String paramId;
    float downT = 0, shown = -9; int downY = 0;
};
