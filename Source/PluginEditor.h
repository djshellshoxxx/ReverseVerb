// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/RVLookAndFeel.h"
#include "ui/DiffusionShape.h"
#include "ui/WaveformDisplay.h"
#include "ui/TensionBox.h"
#include "ui/DragOutPad.h"
#include "ui/HelpOverlay.h"

class ReverseVerbEditor : public juce::AudioProcessorEditor,
                          public juce::DragAndDropContainer,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    explicit ReverseVerbEditor (ReverseVerbProcessor&);
    ~ReverseVerbEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    struct Knob
    {
        juce::Slider slider; juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Group { juce::String name; juce::Rectangle<int> bounds; };

    void timerCallback() override;
    Knob& makeKnob (const juce::String& id, const juce::String& text);
    void layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> ks);
    void applyTooltipSetting();
    void showOptionsMenu();

    ReverseVerbProcessor& proc;
    RVLookAndFeel lnf;

    juce::Label title, subtitle, fileLabel, countLabel, syncLabel, rangeLabel;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, loadButton { "LOAD" }, playButton { "PLAY" },
                     exportButton { "EXPORT WAV" }, resetButton { "RESET EDITS" }, randomButton { "RANDOM" }, optionsButton { "OPTIONS" }, helpButton { "?" };
    juce::ToggleButton alignToggle { "Hit on note (PDC)" }, syncToggle { "SYNC" };
    juce::ComboBox syncCombo, rangeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> alignAtt, syncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncComboAtt, rangeComboAtt;

    WaveformDisplay waveform;
    DiffusionShape shape;
    DragOutPad dragPad;
    TensionBox pitchTension;
    HelpOverlay help;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    bool tooltipsEnabled = true;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *kSize, *kDecay, *kDamp, *kDiff, *kEr, *kSep, *kWidth, *kGap, *kTail, *kShape, *kTone, *kBass, *kDry, *kWet, *kPitch, *kVolStart, *kVolEnd, *kVolTension;
    std::vector<Group> groups;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseVerbEditor)
};
