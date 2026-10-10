// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"

class DiffusionShape : public juce::Component, private juce::Timer
{
public:
    explicit DiffusionShape (ReverseVerbProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); startTimerHz (20); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { angle += 0.018f; repaint(); }
    ReverseVerbProcessor& proc;
    float angle = 0.0f;
};
