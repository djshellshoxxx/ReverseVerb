#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../Common/Widgets.h"

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
    bool keyPressed (const juce::KeyPress& k) override { if (k == juce::KeyPress::spaceKey) { proc.triggerPreview(); return true; } return false; }
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    struct Group { juce::String name; juce::Rectangle<int> bounds; };
    void timerCallback() override;
    Knob& knob (const juce::String& id, const juce::String& text, juce::Colour fill = RVColours::accent);
    void combo (juce::ComboBox&, juce::Label&, const juce::String& id, const juce::String& labelText, const juce::StringArray& items,
                std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);

    ReverseVerbProcessor& proc;
    RVLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Label title, subtitle, fileLabel, countLabel, rangeLabel, modeLabel, fxLabel, gateLabel;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, loadButton { "LOAD" }, generateButton { "GENERATE" }, playButton { "PLAY" },
                     exportButton { "EXPORT WAV" }, resetButton { "RESET EDITS" }, randomButton { "RANDOM" }, helpButton { "?" },
                     shuffleButton { "SHUFFLE" }, unshuffleButton { "UNSHUFFLE" }, allOnButton { "ALL ON" };
    juce::ToggleButton alignToggle { "Hit on note (PDC)" }, syncToggle { "SYNC" }, normToggle { "NORMALIZE" }, swapToggle { "SWAP L/R" }, phaseToggle { "INVERT PHASE" };
    juce::ComboBox syncCombo, rangeCombo, modeCombo, fxCombo, gateCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> alignAtt, syncAtt, normAtt, swapAtt, phaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncComboAtt, rangeComboAtt, modeComboAtt, fxComboAtt, gateComboAtt;
    PresetBar presetBar;
    WaveformDisplay waveform;
    DiffusionShape shape;
    DragOutPad dragPad;
    TensionBox pitchTension;
    HelpOverlay help;
    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *kSize, *kDecay, *kDamp, *kDiff, *kEr, *kSep, *kWidth, *kGap, *kTail, *kShape, *kTone, *kBass, *kStretch, *kDry, *kWet, *kBassB, *kTrebB,
         *kPitch, *kVolStart, *kVolEnd, *kVolTension, *kFxTime, *kFxFb, *kFxDepth, *kFxMix, *kPanStart, *kPanEnd, *kPanTension, *kGateDepth;
    std::vector<Group> groups;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseVerbEditor)
};
