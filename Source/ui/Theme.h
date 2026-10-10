// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>

namespace RVColours
{
    const juce::Colour bg      { 0xff0c0e12 };
    const juce::Colour panel   { 0xff161920 };
    const juce::Colour panel2  { 0xff1e222b };
    const juce::Colour outline { 0xff2b303b };
    const juce::Colour text    { 0xffe6e9ef };
    const juce::Colour textDim { 0xff8a92a3 };
    const juce::Colour accent  { 0xff2ee6d6 };
    const juce::Colour hitCol  { 0xffffb347 };

    // waveform / shape colour driven by Color (tone) and Bass Cut knobs
    juce::Colour swellColour (float toneHz, float bassCutHz);
}
