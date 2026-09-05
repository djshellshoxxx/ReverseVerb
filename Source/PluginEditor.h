#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

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

class RVLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RVLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.0f)); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;
};

class DiffusionShape : public juce::Component, private juce::Timer
{
public:
    explicit DiffusionShape (ReverseVerbProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { angle += 0.012f; repaint(); }
    ReverseVerbProcessor& proc;
    float angle = 0.0f;
};

class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    explicit WaveformDisplay (ReverseVerbProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override { rebuild(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void setEditor (juce::AudioProcessorEditor& e) noexcept { editor = &e; }
private:
    enum class Drag { none, trimEnd, trimStart, volStart, volEnd, volTension, volPoint,
                       panStart, panEnd, panTension, panPoint };
    void timerCallback() override;
    void rebuild();
    juce::Rectangle<float> plot() const;
    float volY (float level) const;
    float panY (float pan) const;
    float volLevelAt (float t) const;
    float panLevelAt (float t) const;
    const juce::String* paramIdFor (Drag) const noexcept;
    ReverseVerbProcessor& proc;
    juce::AudioProcessorEditor* editor = nullptr;
    std::shared_ptr<const RenderedSample> cached;
    juce::Path swellPath, hitPath;
    int total = 0, hitIndex = -1, lastPlayhead = -2;
    float lastTone = -1, lastBass = -1, lastV0 = -1, lastV1 = -1, lastVT = -9;
    double lastBpm = -1.0;
    Drag drag = Drag::none, hover = Drag::none;
    juce::Point<float> downPos;
    float downA = 0, downB = 0, downSpan = 1;
    int dragPointIndex = -1, hoverPointIndex = -1;
    rv::Envelope dragBaseVolEnvelope, dragBasePanEnvelope;
    bool moved = false;
};

class TensionBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    TensionBox (ReverseVerbProcessor& p, const juce::String& id) : proc (p), paramId (id) { startTimerHz (15); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override { proc.setParam (paramId, juce::jlimit (-1.0f, 1.0f, downT + (float) (downY - e.y) / 60.0f)); repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { proc.setParam (paramId, 0.0f); repaint(); }
    void setEditor (juce::AudioProcessorEditor& e) noexcept { editor = &e; }
private:
    juce::AudioProcessorEditor* editor = nullptr;
    void timerCallback() override { const float t = proc.param (paramId); if (t != shown) { shown = t; repaint(); } }
    ReverseVerbProcessor& proc;
    juce::String paramId;
    float downT = 0, shown = -9; int downY = 0;
};

class DragOutPad : public juce::Component
{
public:
    explicit DragOutPad (ReverseVerbProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
private:
    ReverseVerbProcessor& proc;
    bool dragging = false, over = false;
};

class HelpOverlay : public juce::Component
{
public:
    HelpOverlay();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
private:
    juce::TextEditor body;
    juce::TextButton closeButton { "CLOSE" };
};

// Gives every exported parameter knob access to the host's own context menu.
// In hosts that implement the VST3 parameter context-menu extension this can
// expose commands such as "Create automation clip" without hard-coding a DAW.
class HostContextSlider : public juce::Slider
{
public:
    void setHostParameter (juce::AudioProcessorEditor&, juce::AudioProcessorParameter&, ReverseVerbProcessor&);
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::AudioProcessorEditor* editor = nullptr;
    juce::AudioProcessorParameter* parameter = nullptr;
    ReverseVerbProcessor* proc = nullptr;
};

class HostContextComboBox : public juce::ComboBox
{
public:
    void setHostParameter (juce::AudioProcessorEditor&, juce::AudioProcessorParameter&, ReverseVerbProcessor&);
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::AudioProcessorEditor* editor = nullptr;
    juce::AudioProcessorParameter* parameter = nullptr;
    ReverseVerbProcessor* proc = nullptr;
};

class HostContextToggleButton : public juce::ToggleButton
{
public:
    using juce::ToggleButton::ToggleButton;
    void setHostParameter (juce::AudioProcessorEditor&, juce::AudioProcessorParameter&, ReverseVerbProcessor&);
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::AudioProcessorEditor* editor = nullptr;
    juce::AudioProcessorParameter* parameter = nullptr;
    ReverseVerbProcessor* proc = nullptr;
};

class GatePatternEditor : public juce::Component, private juce::Timer
{
public:
    explicit GatePatternEditor (ReverseVerbProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    int stepAt (float x) const noexcept;
    float valueAt (float y) const noexcept;
    void drawLine (int fromStep, float fromValue, int toStep, float toValue);

    ReverseVerbProcessor& proc;
    std::shared_ptr<const rv::GatePattern> cached;
    rv::GatePattern working;
    int activeSteps = 16;
    int firstStep = 0, lastStep = 0;
    float firstValue = 0.0f;
    bool dragging = false;
};

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
        HostContextSlider slider; juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Group { juce::String name; juce::Rectangle<int> bounds; };

    void timerCallback() override;
    Knob& makeKnob (const juce::String& id, const juce::String& text);
    void layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> ks);

    ReverseVerbProcessor& proc;
    RVLookAndFeel lnf;

    juce::Label title, subtitle, fileLabel, countLabel, syncLabel, rangeLabel;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, loadButton { "LOAD" }, playButton { "PLAY" },
                     exportButton { "EXPORT WAV" }, resetButton { "RESET EDITS" }, randomButton { "RANDOM" }, helpButton { "?" };
    juce::TextButton normalizeButton { "NORMALIZE" }, undoButton { "UNDO" }, redoButton { "REDO" };
    juce::Label generateLabel;
    juce::ComboBox generateCombo;
    juce::TextButton generateButton { "GENERATE" };
    HostContextToggleButton hitEnabledToggle { "HIT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hitEnabledAtt;

    juce::Label presetLabel;
    juce::ComboBox presetCombo;
    juce::TextButton presetPrevButton { "<" }, presetNextButton { ">" }, presetSaveButton { "SAVE" }, presetDeleteButton { "DEL" };
    juce::StringArray userPresetNames;
    void rebuildPresetCombo (int itemIdToSelect = -1);
    void loadSelectedPreset();
    void promptSavePreset();
    void promptDeletePreset();
    HostContextToggleButton alignToggle { "Hit on note (PDC)" }, syncToggle { "SYNC" };
    HostContextToggleButton bpmSyncToggle { "HOST BPM" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bpmSyncAtt;
    HostContextToggleButton gateToggle { "GATOR" };
    HostContextComboBox syncCombo, rangeCombo;
    HostContextComboBox directionCombo, gateStepsCombo, gateRateCombo, gateRetriggerCombo, gateTargetCombo, gateShapeCombo;
    HostContextComboBox lfoTargetCombo;
    juce::Label lfoTargetLabel;
    HostContextToggleButton fxEnabledToggle { "FX" };
    HostContextComboBox fxOrderCombo;
    juce::Label fxOrderLabel;
    HostContextToggleButton fxSyncToggle { "SYNC" };
    HostContextComboBox fxSyncDivisionCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> alignAtt, syncAtt, gateEnabledAtt, fxEnabledAtt, fxSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncComboAtt, rangeComboAtt, directionAtt,
                                                                            gateStepsAtt, gateRateAtt, gateRetriggerAtt,
                                                                            gateTargetAtt, gateShapeAtt, lfoTargetAtt, fxOrderAtt, fxSyncDivisionAtt;
    juce::TextButton gateClear { "CLEAR" }, gateFill { "FILL" }, gateInvert { "INVERT" }, gateRandom { "RANDOM" },
                     gateLeft { "<" }, gateRight { ">" }, gateCopy { "COPY" }, gatePaste { "PASTE" },
                     gateUndo { "UNDO" }, gateRedo { "REDO" };

    WaveformDisplay waveform;
    DiffusionShape shape;
    DragOutPad dragPad;
    TensionBox pitchTension;
    GatePatternEditor gatePatternEditor;
    HelpOverlay help;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *kSize, *kDecay, *kDamp, *kDiff, *kEr, *kSep, *kWidth, *kGap, *kTail, *kStretch, *kShape, *kTone, *kBass,
         *kDry, *kWet, *kPitch, *kTranspose, *kVolStart, *kVolEnd, *kVolTension,
         *kPanStart, *kPanEnd, *kPanTension,
         *kLfoRate, *kLfoDepth, *kLfoShape,
         *kFxTime, *kFxFeedback, *kFxModRate, *kFxModDepth, *kFxMix,
         *kGateDepth, *kGateSmooth, *kGateSwing, *kGatePhase, *kBpm;
    std::vector<Group> groups;
    std::unique_ptr<juce::FileChooser> chooser;

    // Everything below the transport row is paged, so only one screen's worth
    // of controls needs to fit at a time - this is what lets the plugin work
    // at a much smaller window size despite how many controls it now has.
    enum class Page { main, mod, fx, gator };
    Page currentPage = Page::main;
    juce::TextButton tabMain { "MAIN" }, tabMod { "MOD" }, tabFx { "FX" }, tabGator { "GATOR" };
    std::vector<juce::Component*> mainPage, modPage, fxPage, gatorPage;
    void showPage (Page);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseVerbEditor)
};
