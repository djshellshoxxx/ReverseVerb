#pragma once
#include <JuceHeader.h>
#include "FxProcessor.h"
#include "../Common/Widgets.h"

class TriggerScope : public juce::Component, private juce::Timer
{
public:
    explicit TriggerScope (FxProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override { mouseDown (e); }
private:
    void timerCallback() override { repaint(); }
    FxProcessor& proc;
    std::vector<float> db;
    std::vector<unsigned char> trig;
};

class FxEditor : public juce::AudioProcessorEditor,
                 public juce::DragAndDropContainer,
                 private juce::Timer
{
public:
    explicit FxEditor (FxProcessor&);
    ~FxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override { if (k == juce::KeyPress::spaceKey) { proc.triggerPreview(); return true; } return false; }

private:
    struct Group { juce::String name; juce::Rectangle<int> bounds; };
    void timerCallback() override;
    Knob& knob (const juce::String& id, const juce::String& text, juce::Colour fill = RVColours::accent);

    FxProcessor& proc;
    RVLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Label title, subtitle, statusLabel, rangeLabel, modeLabel, led, fxLabel, gateLabel;
    juce::TextButton auditionButton { "AUDITION" }, triggerButton { "TRIGGER NOW" }, exportButton { "EXPORT WAV" },
                     resetButton { "RESET EDITS" }, randomButton { "RANDOM" }, helpButton { "?" }, shuffleButton { "SHUFFLE" }, unshuffleButton { "UNSHUFFLE" }, allOnButton { "ALL ON" };
    juce::ToggleButton syncToggle { "SYNC" }, gateToggle { "GATE HIT" }, followToggle { "FOLLOW LEVEL" }, freezeToggle { "FREEZE" }, listenToggle { "LISTEN" },
                       normToggle { "NORMALIZE" }, swapToggle { "SWAP L/R" }, phaseToggle { "INVERT PHASE" };
    juce::ComboBox syncCombo, rangeCombo, modeCombo, fxCombo, gateCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt, gateAtt, followAtt, freezeAtt, listenAtt, normAtt, swapAtt, phaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncComboAtt, rangeComboAtt, modeComboAtt, fxComboAtt, gateComboAtt;
    PresetBar presetBar;
    WaveformDisplay waveform;
    DiffusionShape shape;
    TriggerScope scope;
    DragOutPad dragPad;
    TensionBox pitchTension;
    HelpOverlay help;
    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *kThr, *kSens, *kHold, *kHitLen, *kMaxV, *kSize, *kDecay, *kDamp, *kDiff, *kEr, *kSep, *kWidth, *kGap,
         *kTail, *kShape, *kTone, *kBass, *kStretch, *kDry, *kWet, *kBoost, *kBassB, *kTrebB, *kPitch, *kVolStart, *kVolEnd, *kVolTension,
         *kFxTime, *kFxFb, *kFxDepth, *kFxMix, *kPanStart, *kPanEnd, *kPanTension, *kGateDepth;
    std::vector<Group> groups;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxEditor)
};
