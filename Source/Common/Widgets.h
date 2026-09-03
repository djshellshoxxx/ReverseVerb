#pragma once
#include <JuceHeader.h>
#include "Core.h"

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
    const juce::Colour alert   { 0xffff4d4d };
    const juce::Colour panCol  { 0xffff5ce1 };
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
    explicit DiffusionShape (ParamSource& p) : ps (p) { setInterceptsMouseClicks (false, false); startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { angle += 0.012f; repaint(); }
    ParamSource& ps;
    float angle = 0.0f;
};

class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    explicit WaveformDisplay (RVHost&);
    void paint (juce::Graphics&) override;
    void resized() override { rebuild(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::String emptyText { "Drop a snare / hat / clap here, or hit LOAD" };
private:
    enum class Drag { none, trimEnd, trimStart, trimWindow, volStart, volEnd, volTension, panStart, panEnd, panTension, gate };
    void timerCallback() override;
    void rebuild();
    juce::Rectangle<float> plot() const;
    juce::Rectangle<float> overview() const;
    juce::Rectangle<float> gateStrip() const;
    float volY (float level) const;
    float panY (float pan) const;
    Drag hitTest (juce::Point<float>) const;
    RVHost& host;
    std::shared_ptr<const RenderedSample> cached;
    juce::Path swellPath, hitPath, ghostPath;
    int total = 0, hitIndex = -1, lastPlayhead = -2;
    float lastTone = -1, lastBass = -1, lastV0 = -1, lastV1 = -1, lastVT = -9, lastP0 = -9, lastP1 = -9, lastPT = -9, lastTS = -1, lastTE = -1;
    Drag drag = Drag::none, hover = Drag::none;
    juce::Point<float> downPos;
    float downA = 0, downB = 0, downSpan = 1;
    bool moved = false;
    int lastGateBeat = -1;
    std::vector<juce::String> gestureIds;
};

// Knob with right-click menu: set exact value, reset, host automation menu.
class RVSlider : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent& e) override;
    juce::RangedAudioParameter* parameter = nullptr;
    juce::AudioProcessorEditor* editor = nullptr;
    static double clipboard;
    static bool clipboardValid;
};

class TensionBox : public juce::Component, private juce::Timer
{
public:
    TensionBox (ParamSource& p, const juce::String& id) : ps (p), paramId (id) { startTimerHz (15); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { downT = ps.param (paramId); downY = e.y; ps.beginGesture (paramId); }
    void mouseDrag (const juce::MouseEvent& e) override { ps.setParamValue (paramId, juce::jlimit (-1.0f, 1.0f, downT + (float) (downY - e.y) / 60.0f)); repaint(); }
    void mouseUp (const juce::MouseEvent&) override { ps.endGesture (paramId); }
    void mouseDoubleClick (const juce::MouseEvent&) override { ps.setParam (paramId, 0.0f); repaint(); }
private:
    void timerCallback() override { const float t = ps.param (paramId); if (t != shown) { shown = t; repaint(); } }
    ParamSource& ps;
    juce::String paramId;
    float downT = 0, shown = -9; int downY = 0;
};

class DragOutPad : public juce::Component
{
public:
    bool dragging = false;
    explicit DragOutPad (RVHost& h) : host (h) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
private:
    RVHost& host;
    bool over = false;
};

class HelpOverlay : public juce::Component
{
public:
    explicit HelpOverlay (const juce::String& helpText);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
private:
    juce::TextEditor body;
    juce::TextButton closeButton { "CLOSE" };
};

class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PresetManager&);
    void resized() override;
    void refresh();
private:
    void timerCallback() override;
    void saveDialog();
    PresetManager& pm;
    juce::ComboBox combo;
    juce::TextButton prevB { "<" }, nextB { ">" }, saveB { "SAVE" }, delB { "DEL" }, folderB { "..." };
    juce::Label label;
};

// Knob helper shared by both editors.
struct Knob
{
    RVSlider slider; juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};
std::unique_ptr<Knob> makeKnob (juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& text, juce::Colour fill);
void layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> ks);
void styleLabel (juce::Label&, float size, bool bold, juce::Colour col, juce::Justification j = juce::Justification::centred);
