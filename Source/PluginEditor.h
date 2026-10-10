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
#include "ui/LevelMeter.h"

// The whole UI, laid out at a fixed 1060x720 "design size". ReverseVerbEditor scales it to the window.
class RVContent : public juce::Component,
                  public juce::DragAndDropContainer,
                  public juce::FileDragAndDropTarget,
                  private juce::Timer
{
public:
    explicit RVContent (ReverseVerbProcessor&);
    ~RVContent() override;
    std::function<void (float)> onUiScale;      // set by the editor: Options menu asks for a window scale
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
    juce::ToggleButton alignToggle { "Hit on note (PDC)" }, syncToggle { "SYNC" }, keytrackToggle { "KEYTRACK" }, limiterToggle { "LIMIT" };
    juce::ComboBox syncCombo, rangeCombo, rootCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> alignAtt, syncAtt, keytrackAtt, limiterAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncComboAtt, rangeComboAtt, rootComboAtt;

    WaveformDisplay waveform;
    DiffusionShape shape;
    DragOutPad dragPad;
    TensionBox pitchTension;
    LevelMeter meter;
    HelpOverlay help;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    bool tooltipsEnabled = true;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *kSize, *kDecay, *kDamp, *kDiff, *kEr, *kSep, *kWidth, *kGap, *kTail, *kShape, *kTone, *kBass, *kDry, *kWet, *kPitch, *kVolStart, *kVolEnd, *kVolTension, *kOut;
    std::vector<Group> groups;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RVContent)
};

// Resizable plugin window (75%-150%, fixed aspect). Size is remembered per project.
class ReverseVerbEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int kBaseW = 1060, kBaseH = 720, kMinW = 795, kMaxW = 1590;
    explicit ReverseVerbEditor (ReverseVerbProcessor&);
    void resized() override;

private:
    ReverseVerbProcessor& proc;
    juce::ComponentBoundsConstrainer constrainer;
    RVContent content;
};
