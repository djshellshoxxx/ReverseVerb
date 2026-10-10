// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>

namespace IDs
{
    static const juce::String dry = "dry", wet = "wet";
    static const juce::String size = "size", decay = "decay", damp = "damp", diff = "diff", er = "er", sep = "sep", width = "width", gap = "gap";
    static const juce::String tail = "tail", shape = "shape", tone = "tone", basscut = "basscut";
    static const juce::String align = "align";
    static const juce::String trimStart = "trimStart", trimEnd = "trimEnd";
    static const juce::String sync = "sync", syncLen = "syncLen";
    static const juce::String pitch = "pitch", pitchRange = "pitchRange", pitchTension = "pitchTension";
    static const juce::String volStart = "volStart", volEnd = "volEnd", volTension = "volTension";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
