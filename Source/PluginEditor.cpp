#include "PluginEditor.h"
#include "GatePatternClipboard.h"

#include <array>

using namespace RVColours;

juce::Colour RVColours::swellColour (float toneHz, float bassCutHz)
{
    const float nt = juce::jlimit (0.0f, 1.0f, std::log (toneHz / 500.0f) / std::log (40.0f));      // 0 dark .. 1 bright
    const float nb = juce::jlimit (0.0f, 1.0f, std::log (bassCutHz / 20.0f) / std::log (100.0f));   // 0 none .. 1 full cut
    const float hue = 0.78f - 0.27f * nt;                                                          // violet -> cyan
    const juce::Colour toneCol = juce::Colour::fromHSV (hue, 0.85f, 0.55f + 0.45f * nt, 1.0f);
    const juce::Colour red = juce::Colour::fromHSV (0.02f, 0.95f, 1.0f, 1.0f);
    return toneCol.interpolatedWith (red, nb * 0.9f);
}

// ---------------- Host parameter context menu ----------------
//
// Shared by every Host* control below plus the waveform's trim handles and the
// pitch tension box, so any control bound to a host parameter can expose
// whatever the host's own parameter context-menu extension provides (VST3
// hosts that implement it may add commands such as "Create automation clip";
// note this is entirely host-controlled and is always empty in Standalone,
// since there's no host to provide it), plus MIDI Learn (works everywhere,
// regardless of host support) and a plain Reset to default.

namespace
{
void showHostParameterMenu (juce::Component& target,
                            juce::AudioProcessorEditor* editor,
                            juce::AudioProcessorParameter* parameter,
                            ReverseVerbProcessor* proc)
{
    juce::PopupMenu menu;

    if (editor != nullptr && parameter != nullptr)
        if (auto* hostContext = editor->getHostContext())
            if (auto hostMenu = hostContext->getContextMenuForParameter (parameter))
                menu = hostMenu->getEquivalentPopupMenu();

    if (menu.getNumItems() > 0)
        menu.addSeparator();

    if (proc != nullptr && parameter != nullptr)
    {
        const auto paramIndex = parameter->getParameterIndex();
        const auto armed = proc->getMidiLearnArmedParamIndex() == paramIndex;
        const auto mappedCC = proc->getMidiCCForParameter (paramIndex);

        menu.addItem (armed ? "Waiting for a MIDI CC..." : "MIDI Learn", ! armed, false,
                      [proc, paramIndex] { proc->armMidiLearn (paramIndex); });
        if (mappedCC >= 0)
            menu.addItem ("Clear MIDI Mapping (CC " + juce::String (mappedCC) + ")", true, false,
                          [proc, paramIndex] { proc->clearMidiMapping (paramIndex); });
        menu.addSeparator();
    }

    menu.addItem ("Reset to default", parameter != nullptr, false,
                  [safeTarget = juce::Component::SafePointer<juce::Component> (&target), parameter]
                  {
                      if (safeTarget == nullptr || parameter == nullptr)
                          return;

                      parameter->beginChangeGesture();
                      parameter->setValueNotifyingHost (parameter->getDefaultValue());
                      parameter->endChangeGesture();
                  });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target).withMousePosition());
}
}

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
    const bool bipolar = s.getMinimum() < 0.0;
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

// ---------------- Diffusion shape (Reeverb-2 style wireframe) ----------------

void DiffusionShape::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const juce::Colour col = swellColour (proc.param (IDs::tone), proc.param (IDs::basscut));
    juce::ColourGradient bgGrad (col.withAlpha (0.16f), r.getCentreX(), r.getCentreY(), panel, r.getX(), r.getY(), true);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    const float diff = proc.param (IDs::diff), size = proc.param (IDs::size), decay = proc.param (IDs::decay), sep = proc.param (IDs::sep), er = proc.param (IDs::er);
    const int sides = 3 + juce::roundToInt (diff * 6.0f);
    const float cx = r.getCentreX(), cy = r.getCentreY();
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * (0.18f + 0.17f * size);
    const float halfH = juce::jmin (r.getWidth(), r.getHeight()) * (0.12f + 0.24f * decay);
    const float tilt = 0.55f;

    auto project = [&] (float x, float y, float z, float scale)
    {
        const float xr = x * std::cos (angle) - z * std::sin (angle);
        const float zr = x * std::sin (angle) + z * std::cos (angle);
        const float yr = y * std::cos (tilt) - zr * std::sin (tilt);
        const float depth = 1.0f + zr / (radius * 6.0f);
        return juce::Point<float> (cx + xr * scale * depth, cy + yr * scale * depth);
    };
    auto drawPrism = [&] (float scale, float alpha, float thick, float xOff)
    {
        std::vector<juce::Point<float>> top, bot;
        for (int i = 0; i < sides; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) sides;
            top.push_back (project (radius * std::cos (a) + xOff, -halfH, radius * std::sin (a), scale));
            bot.push_back (project (radius * std::cos (a) + xOff,  halfH, radius * std::sin (a), scale));
        }
        juce::Path p;
        for (int i = 0; i < sides; ++i)
        {
            const int j = (i + 1) % sides;
            p.startNewSubPath (top[(size_t) i]); p.lineTo (top[(size_t) j]);
            p.startNewSubPath (bot[(size_t) i]); p.lineTo (bot[(size_t) j]);
            p.startNewSubPath (top[(size_t) i]); p.lineTo (bot[(size_t) i]);
        }
        g.setColour (col.withAlpha (alpha * 0.35f));
        g.strokePath (p, juce::PathStrokeType (thick + 2.5f));
        g.setColour (col.brighter (0.4f).withAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (thick));
    };
    drawPrism (1.0f, 0.95f, 1.4f, 0.0f);
    if (sep > 0.02f) drawPrism (0.7f, 0.25f + 0.5f * sep, 1.0f, radius * sep * 0.8f);

    // early reflection sparks
    if (er > 0.02f)
    {
        juce::Random rnd (42);
        g.setColour (col.brighter (0.6f).withAlpha (er * 0.8f));
        for (int i = 0; i < 8; ++i)
        {
            const float a = angle * 0.7f + (float) i * 0.8f;
            const float rr = radius * (1.15f + 0.35f * rnd.nextFloat());
            auto pt = project (rr * std::cos (a), (rnd.nextFloat() - 0.5f) * halfH * 2.0f, rr * std::sin (a), 1.0f);
            g.fillEllipse (pt.x - 1.5f, pt.y - 1.5f, 3.0f, 3.0f);
        }
    }

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.setColour (textDim);
    g.drawText ("SPACE", r.reduced (10.0f, 8.0f).toNearestInt(), juce::Justification::bottomLeft);
    g.drawText (juce::String (sides) + " faces", r.reduced (10.0f, 8.0f).toNearestInt(), juce::Justification::bottomRight);
}

// ---------------- Waveform ----------------

WaveformDisplay::WaveformDisplay (ReverseVerbProcessor& p) : proc (p)
{
    startTimerHz (30);
}

juce::Rectangle<float> WaveformDisplay::plot() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedBottom (20.0f).withTrimmedTop (14.0f);
}

float WaveformDisplay::volY (float level) const
{
    auto p = plot();
    return p.getBottom() - level * p.getHeight();
}

float WaveformDisplay::panY (float pan) const
{
    auto p = plot();
    return p.getBottom() - (pan * 0.5f + 0.5f) * p.getHeight();
}

float WaveformDisplay::volLevelAt (float t) const
{
    const auto env = proc.getVolumeEnvelope();
    const rv::Envelope empty;
    return rv::envelopeValueAt (env != nullptr ? *env : empty, t,
                                 proc.param (IDs::volStart), proc.param (IDs::volEnd), proc.param (IDs::volTension));
}

float WaveformDisplay::panLevelAt (float t) const
{
    const auto env = proc.getPanEnvelope();
    const rv::Envelope empty;
    return rv::envelopeValueAt (env != nullptr ? *env : empty, t,
                                 proc.param (IDs::panStart), proc.param (IDs::panEnd), proc.param (IDs::panTension));
}

void WaveformDisplay::timerCallback()
{
    auto r = proc.getRendered();
    bool dirty = false;
    if (r != cached) { cached = r; rebuild(); dirty = true; }
    const int ph = proc.getPlayheadPosition();
    if (ph != lastPlayhead) { lastPlayhead = ph; dirty = true; }
    const float tone = proc.param (IDs::tone), bass = proc.param (IDs::basscut);
    const float v0 = proc.param (IDs::volStart), v1 = proc.param (IDs::volEnd), vt = proc.param (IDs::volTension);
    if (tone != lastTone || bass != lastBass || v0 != lastV0 || v1 != lastV1 || vt != lastVT) { lastTone = tone; lastBass = bass; lastV0 = v0; lastV1 = v1; lastVT = vt; dirty = true; }
    const double bpm = proc.getHostBpm();
    if (std::abs (bpm - lastBpm) > 0.001) { lastBpm = bpm; dirty = true; }
    if (dirty) repaint();
}

void WaveformDisplay::rebuild()
{
    swellPath.clear(); hitPath.clear();
    total = 0; hitIndex = -1;
    if (cached == nullptr) return;
    total = cached->displayAudio.getNumSamples();
    hitIndex = cached->dryHitIndex;
    auto p = plot();
    if (total <= 0 || p.getWidth() <= 2.0f) return;

    auto build = [&] (juce::Path& path, const juce::AudioBuffer<float>& audio, int from, int to)
    {
        if (to <= from) return;
        const float x0 = p.getX() + p.getWidth() * (float) from / (float) total;
        const float x1 = p.getX() + p.getWidth() * (float) to / (float) total;
        const int cols = juce::jmax (1, (int) (x1 - x0));
        const float mid = p.getCentreY();
        std::vector<float> mins ((size_t) cols, 0.0f), maxs ((size_t) cols, 0.0f);
        if (audio.getNumChannels() <= 0 || audio.getNumSamples() < total) return;
        const float* d = audio.getReadPointer (0);
        for (int c = 0; c < cols; ++c)
        {
            const int a = from + (int) ((juce::int64) (to - from) * c / cols);
            const int b = juce::jmax (a + 1, from + (int) ((juce::int64) (to - from) * (c + 1) / cols));
            float mn = 0.0f, mx = 0.0f;
            for (int i = a; i < b && i < total; ++i) { mn = juce::jmin (mn, d[i]); mx = juce::jmax (mx, d[i]); }
            mins[(size_t) c] = mn; maxs[(size_t) c] = mx;
        }
        const float amp = p.getHeight() * 0.47f;
        path.startNewSubPath (x0, mid);
        for (int c = 0; c < cols; ++c) path.lineTo (x0 + (float) c, mid - maxs[(size_t) c] * amp);
        for (int c = cols - 1; c >= 0; --c) path.lineTo (x0 + (float) c, mid - mins[(size_t) c] * amp);
        path.closeSubPath();
    };
    build (swellPath, cached->wetAudio,
           juce::jmax (0, cached->wetStart), juce::jmin (total, cached->wetEnd));
    build (hitPath, cached->dryAudio,
           juce::jmax (0, cached->dryStart), juce::jmin (total, cached->dryEnd));
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto p = plot();
    const juce::Colour col = swellColour (proc.param (IDs::tone), proc.param (IDs::basscut));

    juce::ColourGradient bgGrad (panel.brighter (0.06f), 0, r.getY(), panel.darker (0.3f), 0, r.getBottom(), false);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (col.withAlpha (0.07f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    // grid
    g.setColour (outline.withAlpha (0.35f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (p.getY() + p.getHeight() * i / 4.0f), p.getX(), p.getRight());
    g.setColour (outline.withAlpha (0.8f));
    g.drawHorizontalLine ((int) p.getCentreY(), p.getX(), p.getRight());

    if (total <= 0 || cached == nullptr)
    {
        g.setColour (textDim);
        g.setFont (juce::Font (juce::FontOptions (15.0f)));
        g.drawText ("Drop a snare / hat / clap here, or hit LOAD", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const double sr = cached->sampleRate;
    const double lenSec = total / sr;

    // Musical grid uses the same rational duration model as rendering.
    if (cached->musicalQuarterNotes > 0.0 && cached->bpm > 0.0)
    {
        const double span = cached->trimEndSec - cached->trimStartSec;
        const double barQuarterNotes = cached->timeSignatureNumerator * 4.0
                                     / cached->timeSignatureDenominator;
        const int lineCount = (int) std::ceil (cached->musicalQuarterNotes / cached->gridQuarterNotes);
        for (int line = 0; line <= lineCount; ++line)
        {
            const double quarterNote = juce::jmin (cached->musicalQuarterNotes,
                                                   line * cached->gridQuarterNotes);
            const double t = quarterNote * 60.0 / cached->bpm - cached->trimStartSec;
            if (t < -0.0005 || t > span + 0.0005) continue;
            const float x = p.getX() + p.getWidth() * (float) (t / span);
            const double remainder = std::fmod (quarterNote, barQuarterNotes);
            const bool bar = remainder < 1.0e-8 || barQuarterNotes - remainder < 1.0e-8;
            g.setColour (bar ? juce::Colour (0xffff4d4d).withAlpha (0.9f) : juce::Colour (0xffff4d4d).withAlpha (0.45f));
            g.drawLine (x, p.getY(), x, p.getBottom(), bar ? 1.5f : 1.0f);
        }
    }

    // waveform with depth: shadow, gradient body, glow, top highlight
    auto drawWave = [&] (const juce::Path& path, juce::Colour c)
    {
        if (path.isEmpty()) return;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillPath (path, juce::AffineTransform::translation (3.0f, 4.0f));
        juce::ColourGradient body (c.brighter (0.5f), 0, p.getY(), c.darker (0.7f), 0, p.getBottom(), false);
        body.addColour (0.5, c);
        g.setGradientFill (body);
        g.fillPath (path);
        g.setColour (c.withAlpha (0.28f));
        g.strokePath (path, juce::PathStrokeType (3.0f));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (path, juce::PathStrokeType (0.8f));
    };
    drawWave (swellPath, col);
    drawWave (hitPath, hitCol);

    if (hitIndex >= 0)
    {
        const float sx = p.getX() + p.getWidth() * (float) hitIndex / (float) total;
        g.setColour (text.withAlpha (0.5f));
        const float dash[] = { 3.0f, 3.0f };
        juce::Path l; l.startNewSubPath (sx, p.getY()); l.lineTo (sx, p.getBottom());
        juce::Path dl; juce::PathStrokeType (1.0f).createDashedStroke (dl, l, dash, 2);
        g.fillPath (dl);
    }

    // volume envelope line (may carry extra hand-placed points beyond Start/End)
    const float v0 = proc.param (IDs::volStart), v1 = proc.param (IDs::volEnd), vt = proc.param (IDs::volTension);
    const auto volEnvPtr = proc.getVolumeEnvelope();
    const rv::Envelope emptyEnvelope;
    const auto& volE = volEnvPtr != nullptr ? *volEnvPtr : emptyEnvelope;
    juce::Path vol;
    const int steps = 64;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        const float lvl = rv::envelopeValueAt (volE, x, v0, v1, vt);
        const juce::Point<float> pt (p.getX() + p.getWidth() * x, volY (lvl));
        if (i == 0) vol.startNewSubPath (pt); else vol.lineTo (pt);
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.strokePath (vol, juce::PathStrokeType (3.0f));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.strokePath (vol, juce::PathStrokeType (1.2f));
    auto handle = [&] (juce::Point<float> c, bool hot, bool square)
    {
        const float s = hot ? 6.0f : 4.5f;
        g.setColour (hot ? accent : juce::Colours::white);
        if (square) g.fillRect (c.x - s, c.y - s, s * 2.0f, s * 2.0f); else g.fillEllipse (c.x - s, c.y - s, s * 2.0f, s * 2.0f);
        g.setColour (bg);
        if (square) g.drawRect (c.x - s, c.y - s, s * 2.0f, s * 2.0f, 1.0f); else g.drawEllipse (c.x - s, c.y - s, s * 2.0f, s * 2.0f, 1.0f);
    };
    handle ({ p.getX(), volY (v0) }, hover == Drag::volStart || drag == Drag::volStart, false);
    handle ({ p.getRight(), volY (v1) }, hover == Drag::volEnd || drag == Drag::volEnd, false);
    handle ({ p.getCentreX(), volY (v0 + (v1 - v0) * tensionCurve (0.5f, vt)) }, hover == Drag::volTension || drag == Drag::volTension, true);
    for (int i = 0; i < volE.numInterior; ++i)
    {
        const auto& pt = volE.interior[(size_t) i];
        handle ({ p.getX() + p.getWidth() * pt.pos, volY (pt.value) },
                (hover == Drag::volPoint && hoverPointIndex == i) || (drag == Drag::volPoint && dragPointIndex == i), false);
    }

    // pan envelope line - a second curve through the middle of the waveform,
    // -1 (hard left) at the bottom, 0 (centre) at the vertical middle, +1 (hard right) at the top
    const float pan0 = proc.param (IDs::panStart), pan1 = proc.param (IDs::panEnd), pant = proc.param (IDs::panTension);
    const auto panEnvPtr = proc.getPanEnvelope();
    const auto& panE = panEnvPtr != nullptr ? *panEnvPtr : emptyEnvelope;
    juce::Path pan;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        const float pv = rv::envelopeValueAt (panE, x, pan0, pan1, pant);
        const juce::Point<float> pt (p.getX() + p.getWidth() * x, panY (pv));
        if (i == 0) pan.startNewSubPath (pt); else pan.lineTo (pt);
    }
    g.setColour (juce::Colour (0xffff6bd6).withAlpha (0.22f));
    g.strokePath (pan, juce::PathStrokeType (3.0f));
    g.setColour (juce::Colour (0xffff6bd6).withAlpha (0.85f));
    juce::PathStrokeType panStroke (1.2f);
    float panDash[] = { 4.0f, 3.0f };
    juce::Path dashedPan;
    panStroke.createDashedStroke (dashedPan, pan, panDash, 2);
    g.fillPath (dashedPan);
    auto panHandle = [&] (juce::Point<float> c, bool hot, bool square)
    {
        const float s = hot ? 6.0f : 4.5f;
        g.setColour (hot ? juce::Colour (0xffff6bd6) : juce::Colour (0xffff6bd6).withAlpha (0.85f));
        if (square) g.fillRect (c.x - s, c.y - s, s * 2.0f, s * 2.0f); else { juce::Path d; d.addPolygon (c, 4, s, juce::MathConstants<float>::pi * 0.25f); g.fillPath (d); }
        g.setColour (bg);
        if (square) g.drawRect (c.x - s, c.y - s, s * 2.0f, s * 2.0f, 1.0f); else { juce::Path d; d.addPolygon (c, 4, s, juce::MathConstants<float>::pi * 0.25f); g.strokePath (d, juce::PathStrokeType (1.0f)); }
    };
    panHandle ({ p.getX(), panY (pan0) }, hover == Drag::panStart || drag == Drag::panStart, false);
    panHandle ({ p.getRight(), panY (pan1) }, hover == Drag::panEnd || drag == Drag::panEnd, false);
    panHandle ({ p.getCentreX(), panY (pan0 + (pan1 - pan0) * tensionCurve (0.5f, pant)) }, hover == Drag::panTension || drag == Drag::panTension, true);
    for (int i = 0; i < panE.numInterior; ++i)
    {
        const auto& pt = panE.interior[(size_t) i];
        panHandle ({ p.getX() + p.getWidth() * pt.pos, panY (pt.value) },
                   (hover == Drag::panPoint && hoverPointIndex == i) || (drag == Drag::panPoint && dragPointIndex == i), false);
    }

    // trim handles
    g.setColour (hover == Drag::trimStart || drag == Drag::trimStart ? accent : textDim);
    juce::Path ts; ts.addTriangle (p.getX(), p.getY() - 12.0f, p.getX() + 10.0f, p.getY() - 12.0f, p.getX(), p.getY() - 2.0f); g.fillPath (ts);
    g.setColour (hover == Drag::trimEnd || drag == Drag::trimEnd ? accent : textDim);
    juce::Path te; te.addTriangle (p.getRight(), p.getY() - 12.0f, p.getRight() - 10.0f, p.getY() - 12.0f, p.getRight(), p.getY() - 2.0f); g.fillPath (te);

    // playhead
    if (lastPlayhead >= 0)
    {
        const float px = p.getX() + p.getWidth() * (float) juce::jmin (lastPlayhead, total) / (float) total;
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRect (p.getX(), p.getY(), px - p.getX(), p.getHeight());
        g.setColour (juce::Colours::white);
        g.drawLine (px, p.getY(), px, p.getBottom(), 1.5f);
    }

    // readouts
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    const int by = getHeight() - 18, bh = 14;
    juce::String left, mid, right;
    if (lastPlayhead >= 0)
    {
        const int e = juce::jmin ((int) cached->gainLin.size() - 1, lastPlayhead / RenderedSample::envStep);
        const float gl = e >= 0 ? cached->gainLin[(size_t) e] : 1.0f;
        const float st = e >= 0 ? cached->pitchSemi[(size_t) e] : 0.0f;
        left = juce::String (lastPlayhead / sr, 3) + " s";
        mid = "PITCH " + juce::String (st >= 0 ? "+" : "") + juce::String (st, 1) + " st    VOL " + (gl > 0.0001f ? juce::String (20.0f * std::log10 (gl), 1) + " dB" : "-inf dB");
    }
    else
    {
        left = "LEN " + juce::String (lenSec, 3) + " s";
        if (std::abs (cached->trimEndSec - cached->trimStartSec - cached->fullLengthSec) > 0.001)
            left += "   (trim " + juce::String (cached->trimStartSec, 2) + " - " + juce::String (cached->trimEndSec, 2) + " of " + juce::String (cached->fullLengthSec, 2) + " s)";
        mid = hitIndex >= 0 ? "HIT @ " + juce::String (hitIndex / sr, 3) + " s" : "HIT TRIMMED OUT";
    }
    right = juce::String (proc.getHostBpm(), 1) + " BPM";
    if (cached->beats > 0) right += "   " + juce::String (cached->beats) + " beats";
    g.setColour (col.brighter (0.5f));
    g.drawText (left, 12, by, getWidth() / 2, bh, juce::Justification::centredLeft);
    g.setColour (text);
    g.drawText (mid, 0, by, getWidth(), bh, juce::Justification::centred);
    g.setColour (textDim);
    g.drawText (right, getWidth() / 2, by, getWidth() / 2 - 12, bh, juce::Justification::centredRight);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (textDim.withAlpha (0.7f));
    g.drawText ("click = play    drag = trim    dots = volume    diamonds = pan    drag the line to add a point    right-click = automation", 0, 2, getWidth(), 12, juce::Justification::centred);
}

void WaveformDisplay::mouseMove (const juce::MouseEvent& e)
{
    Drag h = Drag::none;
    int pointIndex = -1;
    if (total > 0)
    {
        auto p = plot();
        const float v0 = proc.param (IDs::volStart), v1 = proc.param (IDs::volEnd), vt = proc.param (IDs::volTension);
        const float pan0 = proc.param (IDs::panStart), pan1 = proc.param (IDs::panEnd), pant = proc.param (IDs::panTension);
        const auto volEnvPtr = proc.getVolumeEnvelope();
        const auto panEnvPtr = proc.getPanEnvelope();
        const rv::Envelope empty;
        const auto& volE = volEnvPtr != nullptr ? *volEnvPtr : empty;
        const auto& panE = panEnvPtr != nullptr ? *panEnvPtr : empty;
        const juce::Point<float> vs (p.getX(), volY (v0)), ve (p.getRight(), volY (v1)), vm (p.getCentreX(), volY (v0 + (v1 - v0) * tensionCurve (0.5f, vt)));
        const juce::Point<float> ps (p.getX(), panY (pan0)), pe (p.getRight(), panY (pan1)), pm (p.getCentreX(), panY (pan0 + (pan1 - pan0) * tensionCurve (0.5f, pant)));

        for (int i = 0; i < volE.numInterior && h == Drag::none; ++i)
            if (e.position.getDistanceFrom ({ p.getX() + p.getWidth() * volE.interior[(size_t) i].pos, volY (volE.interior[(size_t) i].value) }) < 9.0f)
            { h = Drag::volPoint; pointIndex = i; }
        for (int i = 0; i < panE.numInterior && h == Drag::none; ++i)
            if (e.position.getDistanceFrom ({ p.getX() + p.getWidth() * panE.interior[(size_t) i].pos, panY (panE.interior[(size_t) i].value) }) < 9.0f)
            { h = Drag::panPoint; pointIndex = i; }

        if (h == Drag::none && e.position.getDistanceFrom (vs) < 10.0f) h = Drag::volStart;
        else if (h == Drag::none && e.position.getDistanceFrom (ve) < 10.0f) h = Drag::volEnd;
        else if (h == Drag::none && e.position.getDistanceFrom (vm) < 10.0f) h = Drag::volTension;
        else if (h == Drag::none && e.position.getDistanceFrom (ps) < 10.0f) h = Drag::panStart;
        else if (h == Drag::none && e.position.getDistanceFrom (pe) < 10.0f) h = Drag::panEnd;
        else if (h == Drag::none && e.position.getDistanceFrom (pm) < 10.0f) h = Drag::panTension;
        else if (h == Drag::none && e.x > p.getX() + 16.0f && e.x < p.getRight() - 16.0f)
        {
            const float t = (e.x - p.getX()) / p.getWidth();
            if (std::abs (e.y - volY (volLevelAt (t))) < 8.0f) h = Drag::volPoint;
            else if (std::abs (e.y - panY (panLevelAt (t))) < 8.0f) h = Drag::panPoint;
        }
        if (h == Drag::none)
            h = e.x < p.getX() + 14.0f ? Drag::trimStart : Drag::trimEnd;
    }
    if (h != hover || pointIndex != hoverPointIndex) { hover = h; hoverPointIndex = pointIndex; repaint(); }
    setMouseCursor (h == Drag::volStart || h == Drag::volEnd || h == Drag::volTension
                    || h == Drag::panStart || h == Drag::panEnd || h == Drag::panTension
                    || h == Drag::volPoint || h == Drag::panPoint ? juce::MouseCursor::UpDownResizeCursor
                    : (h == Drag::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor));
}

const juce::String* WaveformDisplay::paramIdFor (Drag d) const noexcept
{
    switch (d)
    {
        case Drag::trimStart:  return &IDs::trimStart;
        case Drag::trimEnd:    return &IDs::trimEnd;
        case Drag::volStart:   return &IDs::volStart;
        case Drag::volEnd:     return &IDs::volEnd;
        case Drag::volTension: return &IDs::volTension;
        case Drag::panStart:   return &IDs::panStart;
        case Drag::panEnd:     return &IDs::panEnd;
        case Drag::panTension: return &IDs::panTension;
        case Drag::volPoint:
        case Drag::panPoint:
        case Drag::none:       break;
    }
    return nullptr;
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    mouseMove (e);
    if (e.mods.isPopupMenu())
    {
        if (const auto* paramId = paramIdFor (hover))
            if (auto* parameter = proc.apvts.getParameter (*paramId))
            {
                showHostParameterMenu (*this, editor, parameter, &proc);
                return;
            }
    }
    drag = hover;
    dragPointIndex = hoverPointIndex;
    moved = false;
    downPos = e.position;
    auto p = plot();
    switch (drag)
    {
        case Drag::trimEnd:    downA = proc.param (IDs::trimEnd); downB = proc.param (IDs::trimStart); break;
        case Drag::trimStart:  downA = proc.param (IDs::trimStart); downB = proc.param (IDs::trimEnd); break;
        case Drag::volStart:   downA = proc.param (IDs::volStart); break;
        case Drag::volEnd:     downA = proc.param (IDs::volEnd); break;
        case Drag::volTension: downA = proc.param (IDs::volTension); break;
        case Drag::panStart:   downA = proc.param (IDs::panStart); break;
        case Drag::panEnd:     downA = proc.param (IDs::panEnd); break;
        case Drag::panTension: downA = proc.param (IDs::panTension); break;
        case Drag::volPoint:
        {
            const auto current = proc.getVolumeEnvelope();
            const rv::Envelope base = current != nullptr ? *current : rv::Envelope {};
            if (dragPointIndex >= 0)
            {
                dragBaseVolEnvelope = base;
                if (e.getNumberOfClicks() > 1)
                {
                    const float pos = base.interior[(size_t) dragPointIndex].pos;
                    proc.replaceVolumeEnvelope (rv::withoutNearestInteriorPoint (base, pos), "Remove volume point");
                    drag = Drag::none;
                    dragPointIndex = -1;
                    break;
                }
                downA = base.interior[(size_t) dragPointIndex].pos;
                downB = base.interior[(size_t) dragPointIndex].value;
            }
            else
            {
                const float pos = juce::jlimit (0.01f, 0.99f, (downPos.x - p.getX()) / p.getWidth());
                const float value = juce::jlimit (0.0f, 1.0f, (p.getBottom() - downPos.y) / p.getHeight());
                const auto added = rv::withInteriorPoint (base, pos, value, 0.0f, 1.0f);
                if (added.numInterior <= base.numInterior)
                {
                    // Already at the point cap - nothing was added, so don't let
                    // the nearest-point search below latch onto some unrelated
                    // existing point and drag it by accident.
                    drag = Drag::none;
                    break;
                }
                proc.replaceVolumeEnvelope (added, "Add volume point");
                dragBaseVolEnvelope = added;
                dragPointIndex = rv::nearestInteriorPoint (added, pos, value, 1.0f, 0.2f);
                downA = pos; downB = value;
            }
            break;
        }
        case Drag::panPoint:
        {
            const auto current = proc.getPanEnvelope();
            const rv::Envelope base = current != nullptr ? *current : rv::Envelope {};
            if (dragPointIndex >= 0)
            {
                dragBasePanEnvelope = base;
                if (e.getNumberOfClicks() > 1)
                {
                    const float pos = base.interior[(size_t) dragPointIndex].pos;
                    proc.replacePanEnvelope (rv::withoutNearestInteriorPoint (base, pos), "Remove pan point");
                    drag = Drag::none;
                    dragPointIndex = -1;
                    break;
                }
                downA = base.interior[(size_t) dragPointIndex].pos;
                downB = base.interior[(size_t) dragPointIndex].value;
            }
            else
            {
                const float pos = juce::jlimit (0.01f, 0.99f, (downPos.x - p.getX()) / p.getWidth());
                const float value = juce::jlimit (-1.0f, 1.0f, ((p.getBottom() - downPos.y) / p.getHeight() - 0.5f) * 2.0f);
                const auto added = rv::withInteriorPoint (base, pos, value, -1.0f, 1.0f);
                if (added.numInterior <= base.numInterior)
                {
                    drag = Drag::none;
                    break;
                }
                proc.replacePanEnvelope (added, "Add pan point");
                dragBasePanEnvelope = added;
                dragPointIndex = rv::nearestInteriorPoint (added, pos, value, 2.0f, 0.2f);
                downA = pos; downB = value;
            }
            break;
        }
        default: break;
    }
    downSpan = juce::jmax (0.01f, std::abs (downB - downA));
    if (e.getNumberOfClicks() > 1)
    {
        if (drag == Drag::volStart) proc.setParam (IDs::volStart, 1.0f);
        if (drag == Drag::volEnd) proc.setParam (IDs::volEnd, 1.0f);
        if (drag == Drag::volTension) proc.setParam (IDs::volTension, 0.0f);
        if (drag == Drag::panStart) proc.setParam (IDs::panStart, 0.0f);
        if (drag == Drag::panEnd) proc.setParam (IDs::panEnd, 0.0f);
        if (drag == Drag::panTension) proc.setParam (IDs::panTension, 0.0f);
        if (drag == Drag::volStart || drag == Drag::volEnd || drag == Drag::volTension
            || drag == Drag::panStart || drag == Drag::panEnd || drag == Drag::panTension)
            drag = Drag::none;
    }
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const float dx = e.position.x - downPos.x, dy = e.position.y - downPos.y;
    if (std::abs (dx) > 2.0f || std::abs (dy) > 2.0f) moved = true;
    if (! moved) return;
    auto p = plot();
    const float nx = dx / p.getWidth(), ny = -dy / p.getHeight();
    switch (drag)
    {
        case Drag::trimEnd:    proc.setParam (IDs::trimEnd,   juce::jlimit (downB + 0.02f, 1.0f, downA - nx * downSpan)); break;   // drag right = shorter
        case Drag::trimStart:  proc.setParam (IDs::trimStart, juce::jlimit (0.0f, downB - 0.02f, downA + nx * downSpan)); break;
        case Drag::volStart:   proc.setParam (IDs::volStart, juce::jlimit (0.0f, 1.0f, downA + ny)); break;
        case Drag::volEnd:     proc.setParam (IDs::volEnd,   juce::jlimit (0.0f, 1.0f, downA + ny)); break;
        case Drag::volTension: proc.setParam (IDs::volTension, juce::jlimit (-1.0f, 1.0f, downA + ny * 3.0f * (proc.param (IDs::volEnd) >= proc.param (IDs::volStart) ? -1.0f : 1.0f))); break;
        case Drag::panStart:   proc.setParam (IDs::panStart, juce::jlimit (-1.0f, 1.0f, downA + ny * 2.0f)); break;
        case Drag::panEnd:     proc.setParam (IDs::panEnd,   juce::jlimit (-1.0f, 1.0f, downA + ny * 2.0f)); break;
        case Drag::panTension: proc.setParam (IDs::panTension, juce::jlimit (-1.0f, 1.0f, downA + ny * 3.0f * (proc.param (IDs::panEnd) >= proc.param (IDs::panStart) ? -1.0f : 1.0f))); break;
        case Drag::volPoint:
            if (dragPointIndex >= 0 && dragPointIndex < dragBaseVolEnvelope.numInterior)
            {
                auto working = dragBaseVolEnvelope;
                working.interior[(size_t) dragPointIndex] = { juce::jlimit (0.01f, 0.99f, downA + nx),
                                                               juce::jlimit (0.0f, 1.0f, downB + ny) };
                proc.replaceVolumeEnvelope (working, "Edit volume point");
            }
            break;
        case Drag::panPoint:
            if (dragPointIndex >= 0 && dragPointIndex < dragBasePanEnvelope.numInterior)
            {
                auto working = dragBasePanEnvelope;
                working.interior[(size_t) dragPointIndex] = { juce::jlimit (0.01f, 0.99f, downA + nx),
                                                               juce::jlimit (-1.0f, 1.0f, downB + ny * 2.0f) };
                proc.replacePanEnvelope (working, "Edit pan point");
            }
            break;
        default: break;
    }
}

void WaveformDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! moved && drag != Drag::none && drag != Drag::volStart && drag != Drag::volEnd && drag != Drag::volTension
        && drag != Drag::panStart && drag != Drag::panEnd && drag != Drag::panTension
        && drag != Drag::volPoint && drag != Drag::panPoint)
        proc.triggerPreview();
    drag = Drag::none;
    dragPointIndex = -1;
    repaint();
}

// ---------------- Tension box ----------------

void TensionBox::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (auto* parameter = proc.apvts.getParameter (paramId))
        {
            showHostParameterMenu (*this, editor, parameter, &proc);
            return;
        }
    }
    downT = proc.param (paramId);
    downY = e.y;
}

void TensionBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    auto in = r.reduced (6.0f);
    g.setColour (outline.withAlpha (0.5f));
    g.drawLine (in.getX(), in.getCentreY(), in.getRight(), in.getCentreY(), 0.5f);
    g.drawLine (in.getCentreX(), in.getY(), in.getCentreX(), in.getBottom(), 0.5f);
    const float t = proc.param (paramId);
    juce::Path curve;
    for (int i = 0; i <= 40; ++i)
    {
        const float x = (float) i / 40.0f;
        const juce::Point<float> pt (in.getX() + in.getWidth() * x, in.getBottom() - in.getHeight() * tensionCurve (x, t));
        if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
    }
    g.setColour (accent.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (4.0f));
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (1.6f));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.setColour (textDim);
    g.drawText ("CURVE", getLocalBounds().withTrimmedTop (getHeight() - 13), juce::Justification::centred);
}

// ---------------- Drag out ----------------

void DragOutPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (over ? accent.withAlpha (0.18f) : panel2);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (over ? accent : outline.brighter (0.2f));
    const float dash[] = { 4.0f, 3.0f };
    juce::Path p; p.addRoundedRectangle (r, 7.0f);
    juce::Path dashed;
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, p, dash, 2);
    g.fillPath (dashed);
    g.setColour (over ? text : textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText ("DRAG TO DAW", getLocalBounds(), juce::Justification::centred);
}

void DragOutPad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! e.mouseWasDraggedSinceMouseDown()) return;
    auto src = proc.getCurrentFile();
    if (! src.existsAsFile()) return;
    dragging = true;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("ReverseVerb").getChildFile (src.getFileNameWithoutExtension() + "_reverse.wav");
    tmp.getParentDirectory().createDirectory();
    if (proc.exportWav (tmp))
        if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dc->performExternalDragDropOfFiles ({ tmp.getFullPathName() }, false, this, [this] { dragging = false; });
            return;
        }
    dragging = false;
}

// ---------------- Help ----------------

static const char* kHelpText = R"(REVERSE VERB - what everything does

PAGES
  MAIN / MOD / FX / GATOR tabs switch which controls are showing, so the window stays small - they don't change anything by themselves.
  MAIN: Reverb, Tempo, Swell, and Mix.  MOD: Pitch, Transpose, Stretch, Volume, Pan, and the LFO.  FX: the delay/chorus/echo effect.  GATOR: the step gate.
  The waveform, transport row, and generate/preset rows above the tabs are always visible regardless of page.

WORKFLOW
  LOAD (or drop a file on the window) picks a hit. < > steps through every sample in that folder and auto-plays it with your current settings.
  GENERATE: pick a hit type from the dropdown (Snare, Hat, Clap, Bass Drum, Horn, String, Pluck, Rimshot, Triangle, Tuba) and click GENERATE to synthesize a fresh one-shot; click again for a new variation.
  HIT (checkbox, next to GENERATE): turn off to play the swell with no dry hit at all - a pure reverse-reverb/riser with nothing underneath it. On restores the dry hit at the HIT knob's level.
  PRESET recalls a full settings snapshot (every knob plus the gator pattern) without touching whichever sample is loaded.
  Factory presets ship built in; SAVE names the current settings as a new preset, DEL removes a selected user preset (factory presets can't be deleted).
  Notes in the piano roll trigger the sound (velocity = volume). Click the waveform or PLAY to audition.
  Right-click any knob, toggle, dropdown, waveform trim handle, or the pitch CURVE box for host automation commands
  (when the DAW provides them) and Reset to default.
  EXPORT WAV saves the rendered sample. DRAG TO DAW: drag the pad straight into the channel rack / playlist.
  RISE puts reversed reverb before the hit. FALL puts the dry hit first and plays the forward reverb decay after the selected DELAY.
  Hit on note (PDC): in RISE, reports the dry-hit offset as latency so the hit lands exactly on the note. FALL needs no lookahead, so PDC is disabled.
  RESET EDITS clears trim, pitch, transpose, stretch, and the volume/pan envelopes (leaves the gator and FX untouched). RANDOM rolls new reverb settings.
  NORMALIZE raises or lowers Output Gain so the current Hit/Swell mix peaks just under 0 dB. UNDO/REDO step back and forward through any change - knobs, toggles, envelope points, or the gator.

WAVEFORM
  Colour follows the COLOR knob (violet = dark, cyan = bright) and turns red as BASS CUT rises.
  Drag anywhere to trim: drag right = shorter, drag left = longer. Drag near the left edge to trim the start. Trim the hit off entirely for a pure swell.
  Volume line (white): drag the left / right dots up or down (bottom = -inf dB, top = 0 dB). Drag the middle square to bend the curve (tension). Double-click a dot to reset it.
  Pan line (pink, dashed): same idea, running through the vertical centre (centre = no pan, bottom = hard left, top = hard right).
  Either line: click-drag anywhere along it to add a new point and move it; double-click an existing point to remove it. Up to 7 extra points each.
  Red lines are beats (bright = bar) when SYNC is on. Dashed white line = where the dry hit begins.
  Bottom row shows length, hit position, BPM, and live time / pitch / volume while playing.

REVERB
  SIZE: room dimensions.  DECAY: how long the tail rings.  DAMP: high-frequency absorption.
  DIFFUSION: smearing of echoes (smooth vs grainy). The SPACE panel shows more faces as diffusion rises.
  EARLY REF: level of first reflections.  SEPARATION: how different left and right are.  WIDTH: stereo spread of the mix.
  DELAY: in RISE, silence between the wet rise and hit; in FALL, time from the hit to the wet fall.

SWELL
  LENGTH: seconds of reverb tail (disabled when SYNC is on).  SHAPE: bends the swell envelope (negative = fuller early, positive = late rush).
  COLOR: low-pass filter on the swell.  BASS CUT: high-pass filter on the swell, keeps sub out of your break.

SYNC
  SYNC locks the total timeline to the host tempo and time signature. Choose straight, triplet (T), dotted (D), or 1-64 bar lengths from 1/64T upward.
  With SYNC off, the free LENGTH knob stretches the tail up to 3 minutes for long, drawn-out rises and falls.
  TEMPO: HOST BPM follows the DAW's tempo. Turn it off to set your own BPM with the knob next to it (also used by Standalone, or to deliberately decouple from the host).

PITCH  (MOD page)
  PITCH sweeps the pitch from 0 at the start to the knob amount at the end. Range chooses 1, 2 or 4 octaves. CURVE box: drag up/down to change how fast the sweep happens.
  TRANSPOSE shifts the pitch of the whole rendered hit and tail by a fixed amount (+/-48 semitones), on top of the sweep above - use it to simply tune the sample up or down.

STRETCH  (MOD page)
  Time-stretches the loaded sample itself, up to 64x, pitch unchanged. Unlike LENGTH on the MAIN page (which only extends the reverb tail), this spreads the actual source material across the whole timeline - the way to build a genuine full-length riser or faller (psytrance/trance style) instead of a short hit with a long reverb hanging off it. 1x = off.

MIX
  HIT: level of the dry hit (silenced entirely when the HIT checkbox above the waveform is off).  SWELL: level of the wet rise or fall.

FX  (delay / chorus / echo, FX page)
  A single modulated delay line that reads as different things depending on its settings: short TIME + high MOD DEPTH sounds like a chorus; longer TIME + FEEDBACK sounds like an echo/delay; settings in between blend the two, or add FEEDBACK on top of a chorus for a combination of all three.
  MIX: dry/wet blend. FEEDBACK: number/length of repeats. MOD RATE / MOD DEPTH: speed and amount of the delay-time wobble that creates the chorus character.
  ORDER: Before Reverse applies it before this plugin's own Rise-mode reversal, so its repeats get flipped backwards too - a true reverse echo/delay/chorus. After Reverse keeps the repeats forward, sitting on top of the already-reversed swell. FALL never reverses, so ORDER has no audible effect in FALL mode.
  SYNC: lock TIME to a tempo division (1/64T up to 1/4D) instead of the free TIME knob, for echoes that land exactly on the beat.
  The FX toggle (top-left of the page) enables/disables the whole effect.

VOLUME
  START/END set the level at the beginning and end of the timeline; TENSION bends the curve between them. Add more points directly on the waveform's volume line for multi-stage swells.

PAN
  START/END/TENSION work just like Volume's, but move the mix left/right over time instead of up/down. Add extra points on the waveform's pink pan line the same way.

LFO
  RATE (Hz) and DEPTH set the modulation speed and amount; SHAPE morphs the wave from round (sine) to square. DEPTH at 0 = off.
  SYNC locks RATE to a tempo division (1/64T up to 64 bars) instead of the free Hz knob - the reliable way to make the modulation exactly as slow or fast as you want in musical terms, and to keep it landing in time as tempo changes.
  TARGET picks what's modulated: VOLUME (tremolo), PAN (auto-pan), PITCH (vibrato, +/-12 semitones at full depth), or WIDTH (pulses the stereo image between mono and extra-wide).

GATOR
  Applied last, live, at playback time - after Stretch, Reverb, FX, Pitch, and the Volume/Pan/LFO envelopes are baked in, so it always gates the final mix.
  Paint 16 or 32 step levels; hold Shift while dragging to draw a straight ramp. RATE sets each step from 1/64T to 1/4D.
  DEPTH blends the gate with the original sound. SMOOTH removes clicks. SWING lengthens odd steps and shortens even steps. PHASE rotates timing continuously.
  NOTE restarts the pattern for every hit. HOST locks it to the DAW PPQ timeline and falls back to NOTE when the host supplies no PPQ.
  TARGET gates the SWELL, HIT, or BOTH layers (defaults to BOTH so the effect is always audible regardless of how loud the dry hit is - switch to SWELL or HIT only for the classic "gate the tail but keep the transient steady" trick).  SHAPE chooses Square, Smooth, Ramp Up/Down, Triangle, Sine, or Curved movement inside each step.
  CLEAR, FILL, INVERT, RANDOM, rotate, COPY/PASTE, and UNDO/REDO edit the pattern without interrupting audio.

MIDI LEARN
  Right-click any knob, toggle, or dropdown for "MIDI Learn", then move a control on your MIDI keyboard/controller to map it - this works in every host and in Standalone, since it doesn't depend on host support.
  A host's own automation commands may also appear in that same right-click menu above MIDI Learn, but only in DAWs that implement that VST3 extension; it is never available in Standalone.
)";

HelpOverlay::HelpOverlay()
{
    body.setMultiLine (true);
    body.setReadOnly (true);
    body.setScrollbarsShown (true);
    body.setCaretVisible (false);
    body.setFont (juce::Font (juce::FontOptions (13.0f)));
    body.setText (kHelpText);
    addAndMakeVisible (body);
    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { setVisible (false); };
}

void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (bg.withAlpha (0.88f));
    auto r = getLocalBounds().reduced (40).toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 12.0f, 1.0f);
}

void HelpOverlay::resized()
{
    auto r = getLocalBounds().reduced (52);
    closeButton.setBounds (r.removeFromBottom (30).withSizeKeepingCentre (100, 30));
    r.removeFromBottom (10);
    body.setBounds (r);
}

void HostContextSlider::setHostParameter (juce::AudioProcessorEditor& owner,
                                          juce::AudioProcessorParameter& hostParameter,
                                          ReverseVerbProcessor& processor)
{
    editor = &owner;
    parameter = &hostParameter;
    proc = &processor;
}

void HostContextSlider::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
    {
        juce::Slider::mouseDown (e);
        return;
    }

    showHostParameterMenu (*this, editor, parameter, proc);
}

void HostContextComboBox::setHostParameter (juce::AudioProcessorEditor& owner,
                                            juce::AudioProcessorParameter& hostParameter,
                                            ReverseVerbProcessor& processor)
{
    editor = &owner;
    parameter = &hostParameter;
    proc = &processor;
}

void HostContextComboBox::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
    {
        juce::ComboBox::mouseDown (e);
        return;
    }

    showHostParameterMenu (*this, editor, parameter, proc);
}

void HostContextToggleButton::setHostParameter (juce::AudioProcessorEditor& owner,
                                                juce::AudioProcessorParameter& hostParameter,
                                                ReverseVerbProcessor& processor)
{
    editor = &owner;
    parameter = &hostParameter;
    proc = &processor;
}

void HostContextToggleButton::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
    {
        juce::ToggleButton::mouseDown (e);
        return;
    }
    showHostParameterMenu (*this, editor, parameter, proc);
}

// ---------------- Gator pattern editor ----------------

GatePatternEditor::GatePatternEditor (ReverseVerbProcessor& processor) : proc (processor)
{
    setTitle ("Gator step levels");
    setDescription ("Paint 16 or 32 tempo-synced gate levels. Hold Shift and drag to draw a line.");
    setWantsKeyboardFocus (true);
    cached = proc.getGatePattern();
    activeSteps = proc.getGateSettings().activeSteps;
    if (cached != nullptr)
        working = *cached;
    startTimerHz (20);
}

int GatePatternEditor::stepAt (float x) const noexcept
{
    if (getWidth() <= 0)
        return 0;
    return juce::jlimit (0, activeSteps - 1,
                         (int) std::floor (x * activeSteps / (float) getWidth()));
}

float GatePatternEditor::valueAt (float y) const noexcept
{
    return juce::jlimit (0.0f, 1.0f,
                         1.0f - y / (float) juce::jmax (1, getHeight() - 1));
}

void GatePatternEditor::drawLine (int fromStep, float fromValue, int toStep, float toValue)
{
    if (fromStep > toStep)
    {
        std::swap (fromStep, toStep);
        std::swap (fromValue, toValue);
    }
    const auto span = juce::jmax (1, toStep - fromStep);
    for (int step = fromStep; step <= toStep; ++step)
    {
        const auto amount = (float) (step - fromStep) / (float) span;
        working.steps[(size_t) step] = juce::jlimit (0.0f, 1.0f,
                                                     fromValue + amount * (toValue - fromValue));
    }
}

void GatePatternEditor::mouseDown (const juce::MouseEvent& event)
{
    if (const auto snapshot = proc.getGatePattern())
    {
        cached = snapshot;
        working = *snapshot;
    }
    activeSteps = proc.getGateSettings().activeSteps;
    working.activeSteps = activeSteps;
    firstStep = lastStep = stepAt ((float) event.x);
    firstValue = valueAt ((float) event.y);
    working.steps[(size_t) firstStep] = firstValue;
    dragging = true;
    repaint();
}

void GatePatternEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging)
        return;
    const auto currentStep = stepAt ((float) event.x);
    const auto currentValue = valueAt ((float) event.y);
    if (event.mods.isShiftDown())
    {
        if (cached != nullptr)
            working = *cached;
        working.activeSteps = activeSteps;
        drawLine (firstStep, firstValue, currentStep, currentValue);
    }
    else
    {
        const auto from = juce::jmin (lastStep, currentStep);
        const auto to = juce::jmax (lastStep, currentStep);
        for (int step = from; step <= to; ++step)
            working.steps[(size_t) step] = currentValue;
    }
    lastStep = currentStep;
    repaint();
}

void GatePatternEditor::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;
    dragging = false;
    proc.replaceGatePattern (working, "Paint gate pattern");
    cached = proc.getGatePattern();
    repaint();
}

void GatePatternEditor::timerCallback()
{
    const auto steps = proc.getGateSettings().activeSteps;
    const auto snapshot = proc.getGatePattern();
    if (! dragging && (snapshot != cached || steps != activeSteps))
    {
        cached = snapshot;
        activeSteps = steps;
        if (cached != nullptr)
            working = *cached;
        repaint();
    }
}

void GatePatternEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (bg.withAlpha (0.65f));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    const auto& pattern = dragging || cached == nullptr ? working : *cached;
    const auto width = bounds.getWidth() / (float) activeSteps;
    for (int step = 0; step < activeSteps; ++step)
    {
        auto cell = juce::Rectangle<float> (bounds.getX() + step * width, bounds.getY(), width, bounds.getHeight()).reduced (1.0f);
        const auto level = juce::jlimit (0.0f, 1.0f, pattern.steps[(size_t) step]);
        auto bar = cell.withTop (cell.getBottom() - level * cell.getHeight());
        g.setColour ((step % 4 == 0 ? accent.brighter (0.15f) : accent).withAlpha (0.25f + level * 0.75f));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (outline.withAlpha (step % 4 == 0 ? 0.9f : 0.45f));
        g.drawVerticalLine ((int) cell.getX(), cell.getY(), cell.getBottom());
    }
}

// ---------------- Editor ----------------

ReverseVerbEditor::ReverseVerbEditor (ReverseVerbProcessor& p)
    : AudioProcessorEditor (&p), proc (p), waveform (p), shape (p), dragPad (p), pitchTension (p, IDs::pitchTension),
      gatePatternEditor (p)
{
    setLookAndFeel (&lnf);
    waveform.setEditor (*this);
    pitchTension.setEditor (*this);

    title.setText ("REVERSE VERB", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    addAndMakeVisible (title);
    subtitle.setText ("tempo-shaped reverb rises and falls for hits", juce::dontSendNotification);
    subtitle.setFont (juce::Font (juce::FontOptions (12.0f)));
    subtitle.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (subtitle);

    fileLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    fileLabel.setJustificationType (juce::Justification::centred);
    fileLabel.setColour (juce::Label::backgroundColourId, panel);
    fileLabel.setColour (juce::Label::outlineColourId, outline);
    addAndMakeVisible (fileLabel);
    countLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    countLabel.setJustificationType (juce::Justification::centred);
    countLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (countLabel);

    for (auto* b : { &prevButton, &nextButton, &loadButton, &playButton, &exportButton, &resetButton, &randomButton, &helpButton,
                     &normalizeButton, &undoButton, &redoButton, &generateButton })
        addAndMakeVisible (b);
    generateLabel.setText ("GENERATE", juce::dontSendNotification);
    generateLabel.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    generateLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (generateLabel);
    addAndMakeVisible (generateCombo);
    generateCombo.addItemList ({ "Snare", "Hat", "Clap", "Bass Drum", "Horn", "String", "Pluck", "Rimshot", "Triangle", "Tuba" }, 1);
    generateCombo.setSelectedItemIndex (0, juce::dontSendNotification);
    generateCombo.setTooltip ("Choose a hit type, then GENERATE to synthesize a fresh one-shot to feed the Rise/Fall engine.");
    addAndMakeVisible (hitEnabledToggle);
    hitEnabledToggle.setTooltip ("Off: the swell plays with no dry hit at all. On: the HIT knob sets the dry hit's level as usual.");
    hitEnabledAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::hitEnabled, hitEnabledToggle);
    if (auto* parameter = proc.apvts.getParameter (IDs::hitEnabled))
        hitEnabledToggle.setHostParameter (*this, *parameter, proc);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    presetLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (presetLabel);
    addAndMakeVisible (presetCombo);
    for (auto* b : { &presetPrevButton, &presetNextButton, &presetSaveButton, &presetDeleteButton })
        addAndMakeVisible (b);
    presetCombo.setTextWhenNothingSelected ("(no preset)");
    presetCombo.setTooltip ("Recall a factory or saved preset. Presets change every knob and the gator pattern, not the loaded sample.");
    presetPrevButton.setTooltip ("Previous preset.");
    presetNextButton.setTooltip ("Next preset.");
    presetSaveButton.setTooltip ("Save the current settings as a new preset.");
    presetDeleteButton.setTooltip ("Delete the selected user preset.");
    presetCombo.onChange = [this] { loadSelectedPreset(); };
    presetPrevButton.onClick = [this]
    {
        const auto count = presetCombo.getNumItems();
        if (count <= 0) return;
        auto index = presetCombo.getSelectedItemIndex();
        for (int tries = 0; tries < count; ++tries)
        {
            index = (index - 1 + count) % count;
            if (presetCombo.getItemText (index).isNotEmpty()) break;
        }
        presetCombo.setSelectedItemIndex (index, juce::sendNotificationSync);
    };
    presetNextButton.onClick = [this]
    {
        const auto count = presetCombo.getNumItems();
        if (count <= 0) return;
        auto index = presetCombo.getSelectedItemIndex();
        for (int tries = 0; tries < count; ++tries)
        {
            index = (index + 1) % count;
            if (presetCombo.getItemText (index).isNotEmpty()) break;
        }
        presetCombo.setSelectedItemIndex (index, juce::sendNotificationSync);
    };
    presetSaveButton.onClick = [this] { promptSavePreset(); };
    presetDeleteButton.onClick = [this] { promptDeletePreset(); };
    rebuildPresetCombo();
    if (const auto restoredName = proc.getCurrentPresetName(); restoredName.isNotEmpty())
    {
        const auto& factory = rv::factoryPresets();
        for (int i = 0; i < (int) factory.size(); ++i)
            if (factory[(size_t) i].name == restoredName)
                presetCombo.setSelectedId (i + 1, juce::dontSendNotification);
        const auto userIndex = userPresetNames.indexOf (restoredName);
        if (userIndex >= 0)
            presetCombo.setSelectedId ((int) factory.size() + userIndex + 1, juce::dontSendNotification);
    }
    for (auto* t : { &alignToggle, &syncToggle }) addAndMakeVisible (t);
    addAndMakeVisible (waveform);
    addAndMakeVisible (shape);
    addAndMakeVisible (dragPad);
    addAndMakeVisible (pitchTension);
    addAndMakeVisible (gatePatternEditor);
    addAndMakeVisible (gateToggle);
    for (auto* combo : { &gateStepsCombo, &gateRateCombo, &gateRetriggerCombo, &gateTargetCombo, &gateShapeCombo })
        addAndMakeVisible (combo);
    for (auto* button : { &gateClear, &gateFill, &gateInvert, &gateRandom, &gateLeft, &gateRight,
                          &gateCopy, &gatePaste, &gateUndo, &gateRedo })
        addAndMakeVisible (button);

    for (const auto division : rv::allDivisions)
    {
        const auto label = rv::divisionLabel (division);
        syncCombo.addItem (juce::String::fromUTF8 (label.data(), (int) label.size()), (int) division + 1);
    }
    rangeCombo.addItemList ({ "1 oct", "2 oct", "4 oct" }, 1);
    directionCombo.addItemList ({ "RISE", "FALL" }, 1);
    syncCombo.setTitle ("Sync length");
    syncCombo.setTooltip ("Tempo-locked total length, from 1/64T up to 16 bars. Right-click for host automation.");
    rangeCombo.setTitle ("Pitch range");
    rangeCombo.setTooltip ("Octave range for the PITCH knob. Right-click for host automation.");
    syncToggle.setTooltip ("Lock the total length to host tempo instead of the free LENGTH knob. Right-click for host automation.");
    pitchTension.setTooltip ("Pitch sweep tension curve. Right-click for host automation.");
    addAndMakeVisible (syncCombo);
    addAndMakeVisible (rangeCombo);
    addAndMakeVisible (directionCombo);
    syncComboAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::syncDivisionV2, syncCombo);
    rangeComboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::pitchRange, rangeCombo);
    directionAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::direction, directionCombo);
    alignAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::align, alignToggle);
    syncAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::sync, syncToggle);
    addAndMakeVisible (bpmSyncToggle);
    bpmSyncToggle.setTooltip ("On: follow the host's tempo. Off: use the BPM knob instead. Right-click for host automation.");
    bpmSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::bpmSync, bpmSyncToggle);
    if (auto* parameter = proc.apvts.getParameter (IDs::bpmSync))
        bpmSyncToggle.setHostParameter (*this, *parameter, proc);
    for (const auto& binding : std::initializer_list<std::pair<HostContextComboBox*, const juce::String*>> {
             { &syncCombo, &IDs::syncDivisionV2 }, { &rangeCombo, &IDs::pitchRange } })
        if (auto* parameter = proc.apvts.getParameter (*binding.second))
            binding.first->setHostParameter (*this, *parameter, proc);
    for (const auto& binding : std::initializer_list<std::pair<HostContextToggleButton*, const juce::String*>> {
             { &alignToggle, &IDs::align }, { &syncToggle, &IDs::sync } })
        if (auto* parameter = proc.apvts.getParameter (*binding.second))
            binding.first->setHostParameter (*this, *parameter, proc);

    gateStepsCombo.addItemList ({ "16", "32" }, 1);
    for (const auto division : rv::gateRateDivisions)
    {
        const auto label = rv::divisionLabel (division);
        gateRateCombo.addItem (juce::String::fromUTF8 (label.data(), (int) label.size()),
                               gateRateCombo.getNumItems() + 1);
    }
    gateRetriggerCombo.addItemList ({ "NOTE", "HOST" }, 1);
    gateTargetCombo.addItemList ({ "SWELL", "HIT", "BOTH" }, 1);
    gateShapeCombo.addItemList ({ "SQUARE", "SMOOTH", "RAMP UP", "RAMP DOWN", "TRIANGLE", "SINE", "CURVED" }, 1);
    gateEnabledAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::gateEnabled, gateToggle);
    gateStepsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::gateSteps, gateStepsCombo);
    gateRateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::gateRate, gateRateCombo);
    gateRetriggerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::gateRetrigger, gateRetriggerCombo);
    gateTargetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::gateTarget, gateTargetCombo);
    gateShapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::gateShape, gateShapeCombo);

    gateToggle.setTooltip ("Enable the tempo-synced gator. Right-click for host automation.");
    gateStepsCombo.setTooltip ("Pattern length: 16 or 32 steps.");
    gateRateCombo.setTooltip ("Tempo division for each gate step.");
    gateRetriggerCombo.setTooltip ("NOTE restarts each pattern per hit; HOST locks all notes to the DAW timeline.");
    gateTargetCombo.setTooltip ("Apply the gate to the swell, dry hit, or both layers.");
    gateShapeCombo.setTooltip ("Envelope shape applied inside every open gate step.");
    gateStepsCombo.setTitle ("Gator step count");
    gateRateCombo.setTitle ("Gator rate");
    gateRetriggerCombo.setTitle ("Gator retrigger mode");
    gateTargetCombo.setTitle ("Gator target layer");
    gateShapeCombo.setTitle ("Gator shape");
    gateClear.setTooltip ("Set every gate step to zero.");
    gateFill.setTooltip ("Set every gate step to full level.");
    gateInvert.setTooltip ("Invert all gate levels.");
    gateRandom.setTooltip ("Generate deterministic random gate levels and advance the stored seed.");
    gateLeft.setTooltip ("Rotate active gate steps left.");
    gateRight.setTooltip ("Rotate active gate steps right.");
    gateCopy.setTooltip ("Copy the complete gate pattern to the clipboard.");
    gatePaste.setTooltip ("Paste a validated ReverseVerb gate pattern from the clipboard.");
    gateUndo.setTooltip ("Undo the last gate pattern edit.");
    gateRedo.setTooltip ("Redo the last gate pattern edit.");
    for (const auto& binding : std::initializer_list<std::pair<HostContextComboBox*, const juce::String*>> {
             { &gateStepsCombo, &IDs::gateSteps }, { &gateRateCombo, &IDs::gateRate },
             { &gateRetriggerCombo, &IDs::gateRetrigger }, { &gateTargetCombo, &IDs::gateTarget },
             { &gateShapeCombo, &IDs::gateShape } })
        if (auto* parameter = proc.apvts.getParameter (*binding.second))
            binding.first->setHostParameter (*this, *parameter, proc);
    if (auto* parameter = proc.apvts.getParameter (IDs::gateEnabled))
        gateToggle.setHostParameter (*this, *parameter, proc);

    directionCombo.setTitle ("Reverb direction");
    directionCombo.setTooltip ("RISE: reversed wet swell before the hit. FALL: dry hit followed by forward reverb.");
    if (auto* parameter = proc.apvts.getParameter (IDs::direction))
        directionCombo.setHostParameter (*this, *parameter, proc);

    rangeLabel.setText ("RANGE", juce::dontSendNotification);
    rangeLabel.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    rangeLabel.setJustificationType (juce::Justification::centred);
    rangeLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (rangeLabel);

    prevButton.onClick   = [this] { proc.prevSample(); };
    nextButton.onClick   = [this] { proc.nextSample(); };
    playButton.onClick   = [this] { proc.triggerPreview(); };
    resetButton.onClick  = [this] { proc.resetEdits(); };
    resetButton.setTooltip ("Clears trim, pitch, transpose and the volume envelope. Does not touch the gator.");
    randomButton.onClick = [this] { proc.randomizeReverb(); };
    helpButton.onClick   = [this] { help.setVisible (true); help.toFront (true); };
    normalizeButton.onClick = [this] { proc.normalize(); };
    normalizeButton.setTooltip ("Raises or lowers Output Gain so the current mix peaks just under 0 dB.");
    undoButton.onClick = [this] { proc.getUndoManager().undo(); };
    redoButton.onClick = [this] { proc.getUndoManager().redo(); };
    undoButton.setTooltip ("Undo the last change - any knob, toggle, envelope point, or gator edit.");
    redoButton.setTooltip ("Redo the last undone change.");
    gateClear.onClick = [this] { proc.clearGatePattern(); };
    gateFill.onClick = [this] { proc.fillGatePattern(); };
    gateInvert.onClick = [this] { proc.invertGatePattern(); };
    gateRandom.onClick = [this] { proc.randomizeGatePattern(); };
    gateLeft.onClick = [this] { proc.rotateGatePattern (-1); };
    gateRight.onClick = [this] { proc.rotateGatePattern (1); };
    gateCopy.onClick = [this]
    {
        if (const auto pattern = proc.getGatePattern())
        {
            auto copy = *pattern;
            copy.activeSteps = proc.getGateSettings().activeSteps;
            juce::SystemClipboard::copyTextToClipboard (rv::encodeGatePattern (copy));
        }
    };
    gatePaste.onClick = [this]
    {
        if (const auto pattern = rv::decodeGatePattern (juce::SystemClipboard::getTextFromClipboard()))
            proc.replaceGatePattern (*pattern, "Paste gate pattern");
    };
    gateUndo.onClick = [this]
    {
        if (proc.getUndoManager().undo())
            proc.refreshGatePatternFromState();
    };
    gateRedo.onClick = [this]
    {
        if (proc.getUndoManager().redo())
            proc.refreshGatePatternFromState();
    };

    generateButton.onClick = [this]
    {
        static constexpr std::array<rv::GeneratedSampleType, 10> types {
            rv::GeneratedSampleType::snare, rv::GeneratedSampleType::hat, rv::GeneratedSampleType::clap,
            rv::GeneratedSampleType::bassDrum, rv::GeneratedSampleType::horn, rv::GeneratedSampleType::string,
            rv::GeneratedSampleType::pluck, rv::GeneratedSampleType::rimshot, rv::GeneratedSampleType::triangle,
            rv::GeneratedSampleType::tuba
        };
        const auto index = juce::jlimit (0, (int) types.size() - 1, generateCombo.getSelectedItemIndex());
        proc.generateSample (types[(size_t) index]);
    };

    loadButton.onClick = [this]
    {
        auto start = proc.getCurrentFile().existsAsFile() ? proc.getCurrentFile().getParentDirectory()
                                                          : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        chooser = std::make_unique<juce::FileChooser> ("Pick a sample (browse its folder with < >)", start, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) proc.loadSampleFile (f, true); });
    };
    exportButton.onClick = [this]
    {
        auto src = proc.getCurrentFile();
        if (! src.existsAsFile()) return;
        auto def = src.getParentDirectory().getChildFile (src.getFileNameWithoutExtension() + "_reverse.wav");
        chooser = std::make_unique<juce::FileChooser> ("Export reversed sample", def, "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) proc.exportWav (f.withFileExtension ("wav")); });
    };

    kSize = &makeKnob (IDs::size, "SIZE");       kDecay = &makeKnob (IDs::decay, "DECAY");   kDamp = &makeKnob (IDs::damp, "DAMP");
    kDiff = &makeKnob (IDs::diff, "DIFFUSION");  kEr = &makeKnob (IDs::er, "EARLY REF");     kSep = &makeKnob (IDs::sep, "SEPARATION");
    kWidth = &makeKnob (IDs::width, "WIDTH");    kGap = &makeKnob (IDs::gap, "DELAY");
    kTail = &makeKnob (IDs::tail, "LENGTH");     kShape = &makeKnob (IDs::shape, "SHAPE");   kTone = &makeKnob (IDs::tone, "COLOR");
    kBass = &makeKnob (IDs::basscut, "BASS CUT");
    kStretch = &makeKnob (IDs::stretch, "STRETCH");
    kDry = &makeKnob (IDs::dry, "HIT");          kWet = &makeKnob (IDs::wet, "SWELL");
    kPitch = &makeKnob (IDs::pitch, "PITCH");
    kTranspose = &makeKnob (IDs::transpose, "TRANSPOSE");
    kVolStart = &makeKnob (IDs::volStart, "START"); kVolEnd = &makeKnob (IDs::volEnd, "END"); kVolTension = &makeKnob (IDs::volTension, "TENSION");
    kPanStart = &makeKnob (IDs::panStart, "START"); kPanEnd = &makeKnob (IDs::panEnd, "END"); kPanTension = &makeKnob (IDs::panTension, "TENSION");
    kLfoRate = &makeKnob (IDs::lfoRate, "RATE"); kLfoDepth = &makeKnob (IDs::lfoDepth, "DEPTH"); kLfoShape = &makeKnob (IDs::lfoShape, "SHAPE");
    kGateDepth = &makeKnob (IDs::gateDepth, "DEPTH");
    kGateSmooth = &makeKnob (IDs::gateSmooth, "SMOOTH");
    kGateSwing = &makeKnob (IDs::gateSwing, "SWING");
    kGatePhase = &makeKnob (IDs::gatePhase, "PHASE");
    kBpm = &makeKnob (IDs::manualBpm, "BPM");
    kDry->slider.setColour (juce::Slider::rotarySliderFillColourId, hitCol);
    kTranspose->slider.setTooltip ("Transposes the whole rendered hit and tail by a fixed amount, independent of the PITCH sweep above. Right-click for host automation.");
    kBpm->slider.setTooltip ("Manual tempo used when HOST BPM is off. Right-click for host automation.");
    kStretch->slider.setTooltip ("Time-stretches the loaded sample itself (pitch unchanged) so it can span a whole long riser or faller, not just the reverb tail. Right-click for host automation.");
    kLfoShape->slider.setTooltip ("Morphs the LFO's wave from round (sine) to square.");

    addAndMakeVisible (lfoTargetCombo);
    lfoTargetCombo.addItemList ({ "Volume", "Pan", "Pitch", "Width" }, 1);
    lfoTargetCombo.setTooltip ("What the LFO modulates.");
    lfoTargetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::lfoTarget, lfoTargetCombo);
    if (auto* parameter = proc.apvts.getParameter (IDs::lfoTarget))
        lfoTargetCombo.setHostParameter (*this, *parameter, proc);
    lfoTargetLabel.setText ("TARGET", juce::dontSendNotification);
    lfoTargetLabel.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    lfoTargetLabel.setJustificationType (juce::Justification::centred);
    lfoTargetLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (lfoTargetLabel);

    addAndMakeVisible (lfoSyncToggle);
    lfoSyncToggle.setTooltip ("Lock the LFO RATE to a tempo division instead of the free Hz knob - the easiest way to make the modulation reliably slower or faster in musical terms.");
    lfoSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::lfoSync, lfoSyncToggle);
    if (auto* parameter = proc.apvts.getParameter (IDs::lfoSync))
        lfoSyncToggle.setHostParameter (*this, *parameter, proc);
    addAndMakeVisible (lfoSyncDivisionCombo);
    for (const auto division : rv::allDivisions)
    {
        const auto label = rv::divisionLabel (division);
        lfoSyncDivisionCombo.addItem (juce::String::fromUTF8 (label.data(), (int) label.size()),
                                      lfoSyncDivisionCombo.getNumItems() + 1);
    }
    lfoSyncDivisionCombo.setTooltip ("Tempo division for one full LFO cycle when SYNC is on - anything from 1/64T up to 64 bars.");
    lfoSyncDivisionAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::lfoSyncDivision, lfoSyncDivisionCombo);
    if (auto* parameter = proc.apvts.getParameter (IDs::lfoSyncDivision))
        lfoSyncDivisionCombo.setHostParameter (*this, *parameter, proc);

    kFxTime = &makeKnob (IDs::fxTime, "TIME");
    kFxFeedback = &makeKnob (IDs::fxFeedback, "FEEDBACK");
    kFxModRate = &makeKnob (IDs::fxModRate, "MOD RATE");
    kFxModDepth = &makeKnob (IDs::fxModDepth, "MOD DEPTH");
    kFxMix = &makeKnob (IDs::fxMix, "MIX");
    addAndMakeVisible (fxEnabledToggle);
    fxEnabledToggle.setTooltip ("Enables the delay/chorus/echo effect below. Right-click for host automation.");
    fxEnabledAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::fxEnabled, fxEnabledToggle);
    if (auto* parameter = proc.apvts.getParameter (IDs::fxEnabled))
        fxEnabledToggle.setHostParameter (*this, *parameter, proc);
    addAndMakeVisible (fxOrderCombo);
    fxOrderCombo.addItemList ({ "Before Reverse", "After Reverse" }, 1);
    fxOrderCombo.setTooltip ("Before Reverse: its echoes get flipped backwards too (a true reverse echo/delay/chorus) in RISE mode. After Reverse: normal-direction echoes on top of the swell.");
    fxOrderAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::fxOrder, fxOrderCombo);
    if (auto* parameter = proc.apvts.getParameter (IDs::fxOrder))
        fxOrderCombo.setHostParameter (*this, *parameter, proc);
    fxOrderLabel.setText ("ORDER", juce::dontSendNotification);
    fxOrderLabel.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    fxOrderLabel.setJustificationType (juce::Justification::centred);
    fxOrderLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (fxOrderLabel);
    kFxTime->slider.setTooltip ("Short + high Mod Depth reads as chorus; longer + Feedback reads as echo/delay. Right-click for host automation.");

    addAndMakeVisible (fxSyncToggle);
    fxSyncToggle.setTooltip ("Lock the delay/chorus TIME to the host tempo instead of the free TIME knob. Right-click for host automation.");
    fxSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::fxSync, fxSyncToggle);
    if (auto* parameter = proc.apvts.getParameter (IDs::fxSync))
        fxSyncToggle.setHostParameter (*this, *parameter, proc);
    addAndMakeVisible (fxSyncDivisionCombo);
    for (const auto division : rv::gateRateDivisions)
    {
        const auto label = rv::divisionLabel (division);
        fxSyncDivisionCombo.addItem (juce::String::fromUTF8 (label.data(), (int) label.size()),
                                     fxSyncDivisionCombo.getNumItems() + 1);
    }
    fxSyncDivisionCombo.setTooltip ("Tempo division for the delay/chorus TIME when SYNC is on.");
    fxSyncDivisionAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::fxSyncDivision, fxSyncDivisionCombo);
    if (auto* parameter = proc.apvts.getParameter (IDs::fxSyncDivision))
        fxSyncDivisionCombo.setHostParameter (*this, *parameter, proc);

    // Tabs: everything below the transport row is paged, one screen's worth at
    // a time, so the plugin stays usable at a much smaller window size.
    for (auto* t : { &tabMain, &tabMod, &tabFx, &tabGator })
    {
        addAndMakeVisible (t);
        t->setClickingTogglesState (true);
        t->setRadioGroupId (9001, juce::dontSendNotification);
    }
    tabMain.setToggleState (true, juce::dontSendNotification);
    tabMain.onClick  = [this] { showPage (Page::main); };
    tabMod.onClick   = [this] { showPage (Page::mod); };
    tabFx.onClick    = [this] { showPage (Page::fx); };
    tabGator.onClick = [this] { showPage (Page::gator); };

    mainPage = { &kSize->slider, &kSize->label, &kDecay->slider, &kDecay->label, &kDamp->slider, &kDamp->label,
                 &kDiff->slider, &kDiff->label, &kEr->slider, &kEr->label, &kSep->slider, &kSep->label,
                 &kWidth->slider, &kWidth->label, &kGap->slider, &kGap->label,
                 &bpmSyncToggle, &kBpm->slider, &kBpm->label,
                 &kTail->slider, &kTail->label, &kShape->slider, &kShape->label,
                 &kTone->slider, &kTone->label, &kBass->slider, &kBass->label,
                 &kDry->slider, &kDry->label, &kWet->slider, &kWet->label };
    modPage = { &kPitch->slider, &kPitch->label, &rangeLabel, &rangeCombo, &pitchTension,
                &kTranspose->slider, &kTranspose->label, &kStretch->slider, &kStretch->label,
                &kVolStart->slider, &kVolStart->label, &kVolEnd->slider, &kVolEnd->label, &kVolTension->slider, &kVolTension->label,
                &kPanStart->slider, &kPanStart->label, &kPanEnd->slider, &kPanEnd->label, &kPanTension->slider, &kPanTension->label,
                &kLfoRate->slider, &kLfoRate->label, &kLfoDepth->slider, &kLfoDepth->label, &kLfoShape->slider, &kLfoShape->label,
                &lfoTargetLabel, &lfoTargetCombo, &lfoSyncToggle, &lfoSyncDivisionCombo };
    fxPage = { &fxEnabledToggle, &kFxTime->slider, &kFxTime->label, &kFxFeedback->slider, &kFxFeedback->label,
               &kFxModRate->slider, &kFxModRate->label, &kFxModDepth->slider, &kFxModDepth->label,
               &kFxMix->slider, &kFxMix->label, &fxOrderLabel, &fxOrderCombo,
               &fxSyncToggle, &fxSyncDivisionCombo };
    gatorPage = { &gateToggle, &gateStepsCombo, &gateRateCombo, &gateRetriggerCombo, &gateTargetCombo, &gateShapeCombo,
                  &kGateDepth->slider, &kGateDepth->label, &kGateSmooth->slider, &kGateSmooth->label,
                  &kGateSwing->slider, &kGateSwing->label, &kGatePhase->slider, &kGatePhase->label,
                  &gateClear, &gateFill, &gateInvert, &gateRandom, &gateLeft, &gateRight,
                  &gateCopy, &gatePaste, &gateUndo, &gateRedo, &gatePatternEditor };

    addChildComponent (help);
    setResizable (true, true);
    setResizeLimits (860, 700, 1500, 1150);
    setSize (960, 760);
    showPage (Page::main);
    startTimerHz (10);
    timerCallback();
}

void ReverseVerbEditor::showPage (Page page)
{
    currentPage = page;
    for (auto* c : mainPage)  c->setVisible (page == Page::main);
    for (auto* c : modPage)   c->setVisible (page == Page::mod);
    for (auto* c : fxPage)    c->setVisible (page == Page::fx);
    for (auto* c : gatorPage) c->setVisible (page == Page::gator);
    resized();
}

ReverseVerbEditor::~ReverseVerbEditor() { setLookAndFeel (nullptr); }

ReverseVerbEditor::Knob& ReverseVerbEditor::makeKnob (const juce::String& id, const juce::String& textName)
{
    auto k = std::make_unique<Knob>();
    auto& s = k->slider;
    if (auto* parameter = proc.apvts.getParameter (id))
        s.setHostParameter (*this, *parameter, proc);
    else
        jassertfalse;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible (s);
    k->label.setText (textName, juce::dontSendNotification);
    k->label.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

// ---------------- Presets ----------------

void ReverseVerbEditor::rebuildPresetCombo (int itemIdToSelect)
{
    const auto previousId = itemIdToSelect >= 0 ? itemIdToSelect : presetCombo.getSelectedId();
    presetCombo.clear (juce::dontSendNotification);

    const auto& factory = rv::factoryPresets();
    presetCombo.addSectionHeading ("FACTORY");
    for (int i = 0; i < (int) factory.size(); ++i)
        presetCombo.addItem (factory[(size_t) i].name, i + 1);

    userPresetNames = rv::listUserPresetNames (rv::getUserPresetDirectory());
    if (! userPresetNames.isEmpty())
    {
        presetCombo.addSeparator();
        presetCombo.addSectionHeading ("USER");
        for (int i = 0; i < userPresetNames.size(); ++i)
            presetCombo.addItem (userPresetNames[i], (int) factory.size() + i + 1);
    }

    if (previousId > 0)
        presetCombo.setSelectedId (previousId, juce::dontSendNotification);
}

void ReverseVerbEditor::loadSelectedPreset()
{
    const auto id = presetCombo.getSelectedId();
    if (id <= 0) return;

    const auto& factory = rv::factoryPresets();
    if (id <= (int) factory.size())
    {
        proc.applyFactoryPreset (factory[(size_t) (id - 1)]);
        return;
    }

    const auto userIndex = id - (int) factory.size() - 1;
    if (userIndex < 0 || userIndex >= userPresetNames.size())
        return;
    const auto name = userPresetNames[userIndex];
    const auto state = rv::loadUserPreset (rv::getUserPresetDirectory(), name);
    if (state.isValid())
        proc.applyPresetState (state, name);
}

void ReverseVerbEditor::promptSavePreset()
{
    auto* dialog = new juce::AlertWindow ("Save preset", "Name this preset:", juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor ("name", proc.getCurrentPresetName(), "Name:");
    dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                const auto name = dialog->getTextEditorContents ("name").trim();
                if (name.isNotEmpty()
                    && rv::saveUserPreset (rv::getUserPresetDirectory(), name, proc.buildPresetState()))
                {
                    rebuildPresetCombo();
                    const auto itemIndex = userPresetNames.indexOf (name);
                    if (itemIndex >= 0)
                        presetCombo.setSelectedId ((int) rv::factoryPresets().size() + itemIndex + 1, juce::dontSendNotification);
                }
            }
        }), true);
}

void ReverseVerbEditor::promptDeletePreset()
{
    const auto id = presetCombo.getSelectedId();
    const auto& factory = rv::factoryPresets();
    if (id <= (int) factory.size()) return; // factory presets can't be deleted

    const auto userIndex = id - (int) factory.size() - 1;
    if (userIndex < 0 || userIndex >= userPresetNames.size()) return;
    const auto name = userPresetNames[userIndex];

    const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
        juce::MessageBoxIconType::WarningIcon, "Delete preset",
        "Delete the preset \"" + name + "\"? This can't be undone.", "Delete", "Cancel");
    juce::AlertWindow::showAsync (options, [this, name] (int result)
    {
        if (result == 1 && rv::deleteUserPreset (rv::getUserPresetDirectory(), name))
            rebuildPresetCombo (0);
    });
}

void ReverseVerbEditor::timerCallback()
{
    const auto displayLabel = proc.getDisplayLabel();
    fileLabel.setText (displayLabel.isNotEmpty() ? displayLabel : "no sample loaded", juce::dontSendNotification);
    const int n = proc.getSampleCount();
    countLabel.setText (n > 0 ? juce::String (proc.getSampleIndex() + 1) + " / " + juce::String (n) : "", juce::dontSendNotification);
    const bool sync = proc.param (IDs::sync) > 0.5f;
    const bool rise = proc.getDirection() == RenderDirection::rise;
    kTail->slider.setEnabled (! sync);
    kTail->slider.setAlpha (sync ? 0.4f : 1.0f);
    syncCombo.setEnabled (sync);
    syncCombo.setAlpha (sync ? 1.0f : 0.5f);
    if (! proc.isUsingV2SyncDivision())
        syncCombo.setSelectedItemIndex ((int) proc.getSyncDivision(), juce::dontSendNotification);
    const auto timing = proc.getHostTiming();
    syncToggle.setButtonText ("SYNC " + juce::String (timing.timeSignature.numerator)
                              + "/" + juce::String (timing.timeSignature.denominator));
    alignToggle.setEnabled (rise);
    alignToggle.setAlpha (rise ? 1.0f : 0.45f);
    alignToggle.setTooltip (rise ? "Align the dry hit to the note using plugin delay compensation."
                                 : "FALL starts at the note and requires no lookahead latency.");
    const juce::Colour col = swellColour (proc.param (IDs::tone), proc.param (IDs::basscut));
    for (auto* k : { kTone, kBass, kWet, kTail, kShape })
        if (k->slider.findColour (juce::Slider::rotarySliderFillColourId) != col) { k->slider.setColour (juce::Slider::rotarySliderFillColourId, col); k->slider.repaint(); }
    gateUndo.setEnabled (proc.getUndoManager().canUndo());
    gateRedo.setEnabled (proc.getUndoManager().canRedo());
    undoButton.setEnabled (proc.getUndoManager().canUndo());
    redoButton.setEnabled (proc.getUndoManager().canRedo());
    presetDeleteButton.setEnabled (presetCombo.getSelectedId() > (int) rv::factoryPresets().size());
    const bool bpmHostSynced = proc.param (IDs::bpmSync) > 0.5f;
    kBpm->slider.setEnabled (! bpmHostSynced);
    kBpm->slider.setAlpha (bpmHostSynced ? 0.4f : 1.0f);
    const bool fxTimeSynced = proc.param (IDs::fxSync) > 0.5f;
    kFxTime->slider.setEnabled (! fxTimeSynced);
    kFxTime->slider.setAlpha (fxTimeSynced ? 0.4f : 1.0f);
    const bool lfoRateSynced = proc.param (IDs::lfoSync) > 0.5f;
    kLfoRate->slider.setEnabled (! lfoRateSynced);
    kLfoRate->slider.setAlpha (lfoRateSynced ? 0.4f : 1.0f);
}

void ReverseVerbEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient grad (bg.brighter (0.07f), 0.0f, 0.0f, bg, 0.0f, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillAll();
    const juce::Colour col = swellColour (proc.param (IDs::tone), proc.param (IDs::basscut));
    g.setColour (col.withAlpha (0.07f));
    g.fillEllipse (-140.0f, -180.0f, 480.0f, 360.0f);
    g.setColour (hitCol.withAlpha (0.05f));
    g.fillEllipse ((float) getWidth() - 320.0f, (float) getHeight() - 280.0f, 460.0f, 340.0f);

    for (auto& gr : groups)
    {
        auto r = gr.bounds.toFloat();
        g.setColour (panel.withAlpha (0.75f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.setColour (textDim);
        g.drawText (gr.name, gr.bounds.withHeight (18).withTrimmedLeft (12), juce::Justification::centredLeft);
    }
}

void ReverseVerbEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> ks)
{
    const int kw = area.getWidth() / (int) ks.size();
    for (auto* k : ks)
    {
        auto cell = area.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (14));
        k->slider.setBounds (cell);
    }
}

void ReverseVerbEditor::resized()
{
    help.setBounds (getLocalBounds());
    groups.clear();
    auto area = getLocalBounds().reduced (16);

    // header
    auto header = area.removeFromTop (46);
    auto titleArea = header.removeFromLeft (230);
    title.setBounds (titleArea.removeFromTop (28));
    subtitle.setBounds (titleArea);
    helpButton.setBounds (header.removeFromRight (34).reduced (0, 7));
    header.removeFromRight (10);
    auto browser = header.withTrimmedLeft (20);
    loadButton.setBounds (browser.removeFromRight (80).reduced (0, 7));
    browser.removeFromRight (8);
    nextButton.setBounds (browser.removeFromRight (40).reduced (0, 7));
    browser.removeFromRight (4);
    prevButton.setBounds (browser.removeFromRight (40).reduced (0, 7));
    browser.removeFromRight (8);
    countLabel.setBounds (browser.removeFromRight (56));
    fileLabel.setBounds (browser.reduced (0, 7));

    // sample generator row (an alternative to LOAD: synthesize a hit instead of browsing for one)
    area.removeFromTop (8);
    auto genRow = area.removeFromTop (26);
    generateLabel.setBounds (genRow.removeFromLeft (64));
    genRow.removeFromLeft (6);
    generateCombo.setBounds (genRow.removeFromLeft (110)); genRow.removeFromLeft (6);
    generateButton.setBounds (genRow.removeFromLeft (84));
    hitEnabledToggle.setBounds (genRow.removeFromRight (60));

    // preset row: recall/save/delete a full settings snapshot (independent of the loaded sample)
    area.removeFromTop (8);
    auto presetRow = area.removeFromTop (26);
    presetLabel.setBounds (presetRow.removeFromLeft (72));
    presetRow.removeFromLeft (6);
    presetDeleteButton.setBounds (presetRow.removeFromRight (48)); presetRow.removeFromRight (6);
    presetSaveButton.setBounds (presetRow.removeFromRight (64));   presetRow.removeFromRight (6);
    presetNextButton.setBounds (presetRow.removeFromRight (32));   presetRow.removeFromRight (4);
    presetPrevButton.setBounds (presetRow.removeFromRight (32));   presetRow.removeFromRight (6);
    presetCombo.setBounds (presetRow);

    // shape + waveform
    area.removeFromTop (10);
    auto vis = area.removeFromTop (236);
    shape.setBounds (vis.removeFromLeft (220));
    vis.removeFromLeft (10);
    waveform.setBounds (vis);

    // transport row
    area.removeFromTop (10);
    auto row = area.removeFromTop (34);
    playButton.setBounds (row.removeFromLeft (80));     row.removeFromLeft (6);
    exportButton.setBounds (row.removeFromLeft (100));  row.removeFromLeft (6);
    dragPad.setBounds (row.removeFromLeft (120));       row.removeFromLeft (6);
    resetButton.setBounds (row.removeFromLeft (100));   row.removeFromLeft (6);
    randomButton.setBounds (row.removeFromLeft (80));   row.removeFromLeft (6);
    normalizeButton.setBounds (row.removeFromLeft (94)); row.removeFromLeft (6);
    undoButton.setBounds (row.removeFromLeft (56));     row.removeFromLeft (4);
    redoButton.setBounds (row.removeFromLeft (56));     row.removeFromLeft (14);
    directionCombo.setBounds (row.removeFromLeft (86)); row.removeFromLeft (8);
    alignToggle.setBounds (row.removeFromLeft (150));   row.removeFromLeft (10);
    syncCombo.setBounds (row.removeFromRight (100));    row.removeFromRight (6);
    syncToggle.setBounds (row.removeFromRight (86));

    // Tab bar: everything below is paged, one screen's worth at a time.
    area.removeFromTop (12);
    auto tabRow = area.removeFromTop (28);
    const int tabWidth = tabRow.getWidth() / 4;
    tabMain.setBounds (tabRow.removeFromLeft (tabWidth).reduced (2, 0));
    tabMod.setBounds (tabRow.removeFromLeft (tabWidth).reduced (2, 0));
    tabFx.setBounds (tabRow.removeFromLeft (tabWidth).reduced (2, 0));
    tabGator.setBounds (tabRow.reduced (2, 0));

    area.removeFromTop (10);
    auto pageArea = area;
    auto group = [&] (juce::Rectangle<int>& src, int width, const juce::String& name)
    {
        auto r = src.removeFromLeft (width);
        src.removeFromLeft (8);
        groups.push_back ({ name, r });
        return r.reduced (6).withTrimmedTop (14);
    };

    switch (currentPage)
    {
        case Page::main:
        {
            const int rowH = (pageArea.getHeight() - 10) / 2;
            auto rowA = pageArea.removeFromTop (rowH);
            pageArea.removeFromTop (10);
            auto rowB = pageArea;

            const int tempoWidth = 130;
            layoutKnobs (group (rowA, rowA.getWidth() - tempoWidth - 8, "REVERB"), { kSize, kDecay, kDamp, kDiff, kEr, kSep, kWidth, kGap });
            auto tempoArea = group (rowA, tempoWidth, "TEMPO");
            bpmSyncToggle.setBounds (tempoArea.removeFromTop (18));
            tempoArea.removeFromTop (4);
            layoutKnobs (tempoArea, { kBpm });

            layoutKnobs (group (rowB, rowB.getWidth() * 2 / 3, "SWELL"), { kTail, kShape, kTone, kBass });
            layoutKnobs (group (rowB, rowB.getWidth(), "MIX"), { kDry, kWet });
            break;
        }
        case Page::mod:
        {
            const int rowH = (pageArea.getHeight() - 10) / 2;
            auto rowA = pageArea.removeFromTop (rowH);
            pageArea.removeFromTop (10);
            auto rowB = pageArea;

            auto pitchArea = group (rowA, rowA.getWidth() * 3 / 5, "PITCH");
            {
                auto right = pitchArea.removeFromRight (74);
                rangeLabel.setBounds (right.removeFromTop (14));
                rangeCombo.setBounds (right.removeFromTop (26).reduced (2, 0));
                right.removeFromTop (6);
                pitchTension.setBounds (right.withSizeKeepingCentre (64, juce::jmin (64, right.getHeight())));
                layoutKnobs (pitchArea, { kPitch, kTranspose });
            }
            layoutKnobs (group (rowA, rowA.getWidth(), "STRETCH  (full-length risers/fallers)"), { kStretch });

            const int totalB = rowB.getWidth() - 8 * 2;
            const int unitB = totalB / 10;
            layoutKnobs (group (rowB, unitB * 3, "VOLUME  (also drag the dots on the waveform)"), { kVolStart, kVolEnd, kVolTension });
            layoutKnobs (group (rowB, unitB * 3, "PAN  (drag the pink diamonds)"), { kPanStart, kPanEnd, kPanTension });
            auto lfoArea = group (rowB, rowB.getWidth(), "LFO  (modulates Volume, Pan, Pitch, or Width)");
            {
                auto right = lfoArea.removeFromRight (140);
                lfoTargetLabel.setBounds (right.removeFromTop (14));
                lfoTargetCombo.setBounds (right.removeFromTop (26).reduced (2, 0));
                right.removeFromTop (6);
                auto syncRow = right.removeFromTop (26);
                lfoSyncToggle.setBounds (syncRow.removeFromLeft (52));
                syncRow.removeFromLeft (4);
                lfoSyncDivisionCombo.setBounds (syncRow);
                layoutKnobs (lfoArea, { kLfoRate, kLfoDepth, kLfoShape });
            }
            break;
        }
        case Page::fx:
        {
            auto fxArea = group (pageArea, pageArea.getWidth(), "DELAY / CHORUS / ECHO");
            auto top = fxArea.removeFromTop (28);
            fxEnabledToggle.setBounds (top.removeFromLeft (70));
            auto orderArea = top.removeFromRight (140);
            fxOrderLabel.setBounds (orderArea.removeFromTop (14));
            fxOrderCombo.setBounds (orderArea);
            top.removeFromRight (10);
            auto syncArea = top.removeFromRight (150);
            fxSyncToggle.setBounds (syncArea.removeFromLeft (60));
            syncArea.removeFromLeft (6);
            fxSyncDivisionCombo.setBounds (syncArea);
            fxArea.removeFromTop (10);
            layoutKnobs (fxArea, { kFxTime, kFxFeedback, kFxModRate, kFxModDepth, kFxMix });
            break;
        }
        case Page::gator:
        {
            groups.push_back ({ "GATOR", pageArea });
            auto gateArea = pageArea.reduced (8).withTrimmedTop (14);
            auto gateKnobs = gateArea.removeFromRight (360);
            gateArea.removeFromRight (8);
            layoutKnobs (gateKnobs, { kGateDepth, kGateSmooth, kGateSwing, kGatePhase });

            auto gateControls = gateArea.removeFromTop (28);
            gateToggle.setBounds (gateControls.removeFromLeft (82)); gateControls.removeFromLeft (4);
            gateStepsCombo.setBounds (gateControls.removeFromLeft (54)); gateControls.removeFromLeft (4);
            gateRateCombo.setBounds (gateControls.removeFromLeft (68)); gateControls.removeFromLeft (4);
            gateRetriggerCombo.setBounds (gateControls.removeFromLeft (82)); gateControls.removeFromLeft (4);
            gateTargetCombo.setBounds (gateControls.removeFromLeft (82)); gateControls.removeFromLeft (4);
            gateShapeCombo.setBounds (gateControls.removeFromLeft (112));
            gateArea.removeFromTop (5);

            auto gateButtons = gateArea.removeFromTop (26);
            const std::array<juce::TextButton*, 10> buttons { &gateClear, &gateFill, &gateInvert, &gateRandom,
                                                             &gateLeft, &gateRight, &gateCopy, &gatePaste,
                                                             &gateUndo, &gateRedo };
            const auto buttonWidth = gateButtons.getWidth() / (int) buttons.size();
            for (auto* button : buttons)
                button->setBounds (gateButtons.removeFromLeft (buttonWidth).reduced (1, 0));
            gateArea.removeFromTop (5);
            gatePatternEditor.setBounds (gateArea);
            break;
        }
    }
}

bool ReverseVerbEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void ReverseVerbEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (proc.loadSampleFile (juce::File (f), true)) return;
}
