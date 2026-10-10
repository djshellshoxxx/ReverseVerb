// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Theme.h"

// [ < ] [ preset name v ] [ > ] [ SAVE ] [ A ]   (preset browser, save, A/B compare)
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (ReverseVerbProcessor&);
    void resized() override;
private:
    void timerCallback() override;
    void showMenu();
    void promptSave();
    void save (const juce::String& name, const juce::String& category, bool overwrite);
    void step (int direction);
    void showError (const juce::String&);

    ReverseVerbProcessor& proc;
    juce::TextButton prevButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9")) }, nextButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xba")) },
                     nameButton, saveButton { "SAVE" }, abButton { "A" };
    juce::String shown;
};
