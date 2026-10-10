// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "Theme.h"


juce::Colour RVColours::swellColour (float toneHz, float bassCutHz)
{
    const float nt = juce::jlimit (0.0f, 1.0f, std::log (toneHz / 500.0f) / std::log (40.0f));      // 0 dark .. 1 bright
    const float nb = juce::jlimit (0.0f, 1.0f, std::log (bassCutHz / 20.0f) / std::log (100.0f));   // 0 none .. 1 full cut
    const float hue = 0.78f - 0.27f * nt;                                                          // violet -> cyan
    const juce::Colour toneCol = juce::Colour::fromHSV (hue, 0.85f, 0.55f + 0.45f * nt, 1.0f);
    const juce::Colour red = juce::Colour::fromHSV (0.02f, 0.95f, 1.0f, 1.0f);
    return toneCol.interpolatedWith (red, nb * 0.9f);
}
