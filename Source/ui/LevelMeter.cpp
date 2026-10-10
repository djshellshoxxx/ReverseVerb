// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "LevelMeter.h"

using namespace RVColours;

void LevelMeter::timerCallback()
{
    bool changed = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float p = proc.takePeak (ch);
        const float next = p > level[ch] ? p : level[ch] * 0.88f;     // instant attack, ~0.3 s release
        if (std::abs (next - level[ch]) > 1.0e-4f) changed = true;
        level[ch] = next < 1.0e-4f ? 0.0f : next;
    }
    const bool c = proc.clipLatched();
    if (c != clip) { clip = c; changed = true; }
    if (changed) repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto led = r.removeFromRight (10.0f).withSizeKeepingCentre (8.0f, 8.0f);
    g.setColour (clip ? juce::Colour (0xffff4d4d) : outline);
    g.fillEllipse (led);
    r.removeFromRight (4.0f);

    const float barH = (r.getHeight() - 3.0f) * 0.5f;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = juce::Rectangle<float> (r.getX(), r.getY() + (float) ch * (barH + 3.0f), r.getWidth(), barH);
        g.setColour (panel2);
        g.fillRoundedRectangle (bar, 2.0f);
        const float db = level[ch] > 1.0e-4f ? juce::Decibels::gainToDecibels (level[ch]) : -60.0f;
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -3.0f ? juce::Colour (0xffff4d4d) : (db > -12.0f ? hitCol : accent));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 2.0f);
    }
}
