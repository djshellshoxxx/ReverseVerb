// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "RVLookAndFeel.h"

using namespace RVColours;

// ---------------- Look and feel ----------------

RVLookAndFeel::RVLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::ToggleButton::textColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, outline);
    setColour (juce::ScrollBar::thumbColourId, outline.brighter (0.4f));
}

void RVLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius - 3.0f;
    const bool bipolar = s.getMinimum() < 0.0 && ! (bool) s.getProperties()["unipolar"];
    const juce::Colour col = s.findColour (juce::Slider::rotarySliderFillColourId, true);

    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (outline);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    value.addCentredArc (cx, cy, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (col.withAlpha (0.35f));
    g.strokePath (value, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - 9.0f;
    juce::ColourGradient grad (panel2.brighter (0.3f), cx - knobR, cy - knobR, panel.darker (0.5f), cx + knobR, cy + knobR, true);
    g.setGradientFill (grad);
    g.fillEllipse (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (outline.brighter (0.25f));
    g.drawEllipse (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.75f, -knobR + 4.0f, 3.5f, knobR * 0.55f, 1.75f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
    g.setColour (text);
    g.fillPath (pointer);
}

void RVLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? accent : (down ? panel2.brighter (0.3f) : (over ? panel2.brighter (0.15f) : panel2)));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (on ? accent.brighter (0.2f) : outline.brighter (over ? 0.5f : 0.15f));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

juce::Font RVLookAndFeel::getTextButtonFont (juce::TextButton&, int height)
{
    return juce::Font (juce::FontOptions (juce::jmin (12.5f, (float) height * 0.55f), juce::Font::bold));
}

void RVLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (18.0f).withSizeKeepingCentre (16.0f, 16.0f);
    g.setColour (b.getToggleState() ? accent : (over ? panel2.brighter (0.3f) : panel2));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (outline.brighter (0.3f));
    g.drawRoundedRectangle (box, 4.0f, 1.0f);
    if (b.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 4.0f, box.getCentreY());
        tick.lineTo (box.getX() + 7.0f, box.getBottom() - 4.5f);
        tick.lineTo (box.getRight() - 3.5f, box.getY() + 4.0f);
        g.setColour (bg);
        g.strokePath (tick, juce::PathStrokeType (2.0f));
    }
    g.setColour (b.getToggleState() ? text : textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f).toNearestInt(), juce::Justification::centredLeft);
}

juce::Label* RVLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (11.0f)));
    l->setColour (juce::Label::textColourId, textDim);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void RVLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (box.isMouseOver (true) ? panel2.brighter (0.15f) : panel2);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (outline.brighter (0.15f));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (textDim);
    g.fillPath (arrow);
}

void RVLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}
