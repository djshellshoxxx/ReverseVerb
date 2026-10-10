// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "TensionBox.h"

using namespace RVColours;

// ---------------- Tension box ----------------

void TensionBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    auto in = r.reduced (6.0f);
    g.setColour (outline.withAlpha (0.5f));
    g.drawLine (in.getX(), in.getCentreY(), in.getRight(), in.getCentreY(), 0.5f);
    g.drawLine (in.getCentreX(), in.getY(), in.getCentreX(), in.getBottom(), 0.5f);
    const float t = proc.param (paramId);
    juce::Path curve;
    for (int i = 0; i <= 40; ++i)
    {
        const float x = (float) i / 40.0f;
        const juce::Point<float> pt (in.getX() + in.getWidth() * x, in.getBottom() - in.getHeight() * tensionCurve (x, t));
        if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
    }
    g.setColour (accent.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (4.0f));
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (1.6f));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.setColour (textDim);
    g.drawText ("CURVE", getLocalBounds().withTrimmedTop (getHeight() - 13), juce::Justification::centred);
}
