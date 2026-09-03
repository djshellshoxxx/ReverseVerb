#include "Widgets.h"

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
    const juce::Colour col = swellColour (ps.param (IDs::tone), ps.param (IDs::basscut));
    juce::ColourGradient bgGrad (col.withAlpha (0.16f), r.getCentreX(), r.getCentreY(), panel, r.getX(), r.getY(), true);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    const float diff = ps.param (IDs::diff), size = ps.param (IDs::size), decay = ps.param (IDs::decay), sep = ps.param (IDs::sep), er = ps.param (IDs::er);
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

WaveformDisplay::WaveformDisplay (RVHost& h) : host (h) { startTimerHz (30); }

juce::Rectangle<float> WaveformDisplay::overview() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedTop (12.0f).withHeight (22.0f);
}

juce::Rectangle<float> WaveformDisplay::plot() const
{
    const bool gate = cached != nullptr && cached->beats > 0;
    return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedTop (12.0f + 22.0f + 6.0f).withTrimmedBottom (20.0f + (gate ? 18.0f : 0.0f));
}

juce::Rectangle<float> WaveformDisplay::gateStrip() const
{
    auto p = plot();
    return juce::Rectangle<float> (p.getX(), p.getBottom() + 2.0f, p.getWidth(), 14.0f);
}

float WaveformDisplay::volY (float level) const { auto p = plot(); return p.getBottom() - level * p.getHeight(); }
float WaveformDisplay::panY (float pan) const { auto p = plot(); return p.getCentreY() - pan * p.getHeight() * 0.45f; }

void WaveformDisplay::timerCallback()
{
    auto r = host.getRendered();
    bool dirty = false;
    if (r != cached) { cached = r; rebuild(); dirty = true; }
    const int ph = host.getPlayheadPosition();
    if (ph != lastPlayhead) { lastPlayhead = ph; dirty = true; }
    const float vals[] = { host.param (IDs::tone), host.param (IDs::basscut), host.param (IDs::volStart), host.param (IDs::volEnd), host.param (IDs::volTension),
                           host.param (IDs::panStart), host.param (IDs::panEnd), host.param (IDs::panTension), host.param (IDs::trimStart), host.param (IDs::trimEnd) };
    float* last[] = { &lastTone, &lastBass, &lastV0, &lastV1, &lastVT, &lastP0, &lastP1, &lastPT, &lastTS, &lastTE };
    for (int i = 0; i < 10; ++i) if (vals[i] != *last[i]) { *last[i] = vals[i]; dirty = true; }
    if (dirty) repaint();
}

void WaveformDisplay::rebuild()
{
    swellPath.clear(); hitPath.clear(); ghostPath.clear();
    total = 0; hitIndex = -1;
    if (cached == nullptr) return;
    total = cached->audio.getNumSamples();
    hitIndex = cached->hitIndex;
    auto p = plot();
    if (total <= 0 || p.getWidth() <= 2.0f) return;

    auto build = [&] (juce::Path& path, int from, int to)
    {
        if (to <= from) return;
        const float x0 = p.getX() + p.getWidth() * (float) from / (float) total;
        const float x1 = p.getX() + p.getWidth() * (float) to / (float) total;
        const int cols = juce::jmax (1, (int) (x1 - x0));
        const float mid = p.getCentreY();
        std::vector<float> mins ((size_t) cols, 0.0f), maxs ((size_t) cols, 0.0f);
        const float* d = cached->audio.getReadPointer (0);
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
    const int split = hitIndex >= 0 ? hitIndex : total;
    build (swellPath, 0, split);
    build (hitPath, split, total);

    // overview ghost of the untrimmed material
    auto o = overview();
    if (! cached->ghostMax.empty())
    {
        const int res = (int) cached->ghostMax.size();
        const float mid = o.getCentreY(), amp = o.getHeight() * 0.48f;
        ghostPath.startNewSubPath (o.getX(), mid);
        for (int i = 0; i < res; ++i) ghostPath.lineTo (o.getX() + o.getWidth() * (float) i / (float) (res - 1), mid - cached->ghostMax[(size_t) i] * amp);
        for (int i = res - 1; i >= 0; --i) ghostPath.lineTo (o.getX() + o.getWidth() * (float) i / (float) (res - 1), mid - cached->ghostMin[(size_t) i] * amp);
        ghostPath.closeSubPath();
    }
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto p = plot();
    const juce::Colour col = swellColour (host.param (IDs::tone), host.param (IDs::basscut));

    juce::ColourGradient bgGrad (panel.brighter (0.06f), 0, r.getY(), panel.darker (0.3f), 0, r.getBottom(), false);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (col.withAlpha (0.07f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (outline.withAlpha (0.35f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (p.getY() + p.getHeight() * i / 4.0f), p.getX(), p.getRight());
    g.setColour (outline.withAlpha (0.8f));
    g.drawHorizontalLine ((int) p.getCentreY(), p.getX(), p.getRight());

    if (total <= 0 || cached == nullptr)
    {
        g.setColour (textDim);
        g.setFont (juce::Font (juce::FontOptions (15.0f)));
        g.drawText (emptyText, getLocalBounds(), juce::Justification::centred);
        return;
    }

    const double sr = cached->sampleRate;
    const double lenSec = total / sr;
    const double span = juce::jmax (0.001, cached->trimEndSec - cached->trimStartSec);

    // ---- overview strip with trim window ----
    auto o = overview();
    g.setColour (bg.withAlpha (0.6f));
    g.fillRoundedRectangle (o, 4.0f);
    g.setColour (textDim.withAlpha (0.45f));
    g.fillPath (ghostPath);
    const float fullSec = (float) juce::jmax (0.001, cached->fullLengthSec);
    const float wx0 = o.getX() + o.getWidth() * (float) (cached->trimStartSec / fullSec);
    const float wx1 = o.getX() + o.getWidth() * (float) (cached->trimEndSec / fullSec);
    g.setColour (col.withAlpha (0.25f));
    g.fillRect (wx0, o.getY(), wx1 - wx0, o.getHeight());
    g.setColour (bg.withAlpha (0.55f));
    g.fillRect (o.getX(), o.getY(), wx0 - o.getX(), o.getHeight());
    g.fillRect (wx1, o.getY(), o.getRight() - wx1, o.getHeight());
    g.setColour (hover == Drag::trimStart || drag == Drag::trimStart ? accent : col.brighter (0.4f));
    g.fillRect (wx0 - 2.0f, o.getY(), 4.0f, o.getHeight());
    g.setColour (hover == Drag::trimEnd || drag == Drag::trimEnd ? accent : col.brighter (0.4f));
    g.fillRect (wx1 - 2.0f, o.getY(), 4.0f, o.getHeight());
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.setColour (textDim);
    g.drawText ("TRIM", o.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft);

    // ---- beat grid + gate ----
    if (cached->beats > 0)
    {
        const double beatSec = cached->fullLengthSec / cached->beats;
        auto xAt = [&] (double t) { return p.getX() + p.getWidth() * (float) ((t - cached->trimStartSec) / span); };
        auto gs = gateStrip();
        for (int b = 0; b < cached->beats; ++b)
        {
            const float xa = juce::jlimit (p.getX(), p.getRight(), xAt (b * beatSec)), xb = juce::jlimit (p.getX(), p.getRight(), xAt ((b + 1) * beatSec));
            const bool on = b < (int) cached->gateMask.size() ? cached->gateMask[(size_t) b] != 0 : true;
            if (! on && xb > xa) { g.setColour (bg.withAlpha (0.55f)); g.fillRect (xa, p.getY(), xb - xa, p.getHeight()); }
            // gate strip block
            const float bx0 = gs.getX() + gs.getWidth() * (float) b / (float) cached->beats, bx1 = gs.getX() + gs.getWidth() * (float) (b + 1) / (float) cached->beats;
            g.setColour (on ? col.withAlpha (0.8f) : panel2);
            g.fillRoundedRectangle (bx0 + 0.5f, gs.getY(), juce::jmax (1.0f, bx1 - bx0 - 1.0f), gs.getHeight(), 2.0f);
        }
        for (int b = 0; b <= cached->beats; ++b)
        {
            const double t = b * beatSec - cached->trimStartSec;
            if (t < -0.0005 || t > span + 0.0005) continue;
            const float x = p.getX() + p.getWidth() * (float) (t / span);
            const bool bar = (b % cached->beatsPerBar) == 0;
            g.setColour (alert.withAlpha (bar ? 0.9f : 0.45f));
            g.drawLine (x, p.getY(), x, p.getBottom(), bar ? 1.5f : 1.0f);
        }
        g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
        g.setColour (textDim);
        g.drawText ("GATE  (click a block to mute/unmute)", gs.toNearestInt().translated (0, 0).withTrimmedLeft (0), juce::Justification::centredRight);
    }

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

    if (hitIndex > 0)
    {
        const float sx = p.getX() + p.getWidth() * (float) hitIndex / (float) total;
        const float dash[] = { 3.0f, 3.0f };
        juce::Path l; l.startNewSubPath (sx, p.getY()); l.lineTo (sx, p.getBottom());
        juce::Path dl; juce::PathStrokeType (1.0f).createDashedStroke (dl, l, dash, 2);
        g.setColour (text.withAlpha (0.5f));
        g.fillPath (dl);
    }

    auto handle = [&] (juce::Point<float> c, bool hot, bool square, juce::Colour base)
    {
        const float s = hot ? 6.0f : 4.5f;
        g.setColour (hot ? accent : base);
        if (square) g.fillRect (c.x - s, c.y - s, s * 2.0f, s * 2.0f); else g.fillEllipse (c.x - s, c.y - s, s * 2.0f, s * 2.0f);
        g.setColour (bg);
        if (square) g.drawRect (c.x - s, c.y - s, s * 2.0f, s * 2.0f, 1.0f); else g.drawEllipse (c.x - s, c.y - s, s * 2.0f, s * 2.0f, 1.0f);
    };

    // ---- pan line ----
    {
        const float p0 = host.param (IDs::panStart), p1 = host.param (IDs::panEnd), pt = host.param (IDs::panTension);
        juce::Path line;
        for (int i = 0; i <= 64; ++i)
        {
            const float x = (float) i / 64.0f;
            const juce::Point<float> pt2 (p.getX() + p.getWidth() * x, panY (p0 + (p1 - p0) * tensionCurve (x, pt)));
            if (i == 0) line.startNewSubPath (pt2); else line.lineTo (pt2);
        }
        g.setColour (panCol.withAlpha (0.25f)); g.strokePath (line, juce::PathStrokeType (3.0f));
        g.setColour (panCol.withAlpha (0.9f));  g.strokePath (line, juce::PathStrokeType (1.2f));
        handle ({ p.getX(), panY (p0) }, hover == Drag::panStart || drag == Drag::panStart, false, panCol);
        handle ({ p.getRight(), panY (p1) }, hover == Drag::panEnd || drag == Drag::panEnd, false, panCol);
        handle ({ p.getCentreX(), panY (p0 + (p1 - p0) * tensionCurve (0.5f, pt)) }, hover == Drag::panTension || drag == Drag::panTension, true, panCol);
        g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
        g.setColour (panCol.withAlpha (0.8f));
        g.drawText ("R", (int) p.getX() + 4, (int) p.getY() + 2, 12, 10, juce::Justification::centredLeft);
        g.drawText ("L", (int) p.getX() + 4, (int) p.getBottom() - 12, 12, 10, juce::Justification::centredLeft);
    }

    // ---- volume line ----
    {
        const float v0 = host.param (IDs::volStart), v1 = host.param (IDs::volEnd), vt = host.param (IDs::volTension);
        juce::Path vol;
        for (int i = 0; i <= 64; ++i)
        {
            const float x = (float) i / 64.0f;
            const juce::Point<float> pt2 (p.getX() + p.getWidth() * x, volY (v0 + (v1 - v0) * tensionCurve (x, vt)));
            if (i == 0) vol.startNewSubPath (pt2); else vol.lineTo (pt2);
        }
        g.setColour (juce::Colours::white.withAlpha (0.25f)); g.strokePath (vol, juce::PathStrokeType (3.0f));
        g.setColour (juce::Colours::white.withAlpha (0.9f));  g.strokePath (vol, juce::PathStrokeType (1.2f));
        handle ({ p.getX(), volY (v0) }, hover == Drag::volStart || drag == Drag::volStart, false, juce::Colours::white);
        handle ({ p.getRight(), volY (v1) }, hover == Drag::volEnd || drag == Drag::volEnd, false, juce::Colours::white);
        handle ({ p.getCentreX(), volY (v0 + (v1 - v0) * tensionCurve (0.5f, vt)) }, hover == Drag::volTension || drag == Drag::volTension, true, juce::Colours::white);
    }

    if (lastPlayhead >= 0)
    {
        const float px = p.getX() + p.getWidth() * (float) juce::jmin (lastPlayhead, total) / (float) total;
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRect (p.getX(), p.getY(), px - p.getX(), p.getHeight());
        g.setColour (juce::Colours::white);
        g.drawLine (px, p.getY(), px, p.getBottom(), 1.5f);
    }

    // ---- readouts ----
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    const int by = getHeight() - 18, bh = 14;
    juce::String left, mid, right;
    if (lastPlayhead >= 0)
    {
        const int e = juce::jmin ((int) cached->gainLin.size() - 1, lastPlayhead / RenderedSample::envStep);
        const float gl = e >= 0 ? cached->gainLin[(size_t) e] : 1.0f;
        const float st = e >= 0 ? cached->pitchSemi[(size_t) e] : 0.0f;
        const float pn = e >= 0 && e < (int) cached->panPos.size() ? cached->panPos[(size_t) e] : 0.0f;
        left = juce::String (lastPlayhead / sr, 3) + " s";
        mid = "PITCH " + juce::String (st >= 0 ? "+" : "") + juce::String (st, 1) + " st    VOL " + (gl > 0.0001f ? juce::String (20.0f * std::log10 (gl), 1) + " dB" : "-inf dB")
              + "    PAN " + (std::abs (pn) < 0.005f ? juce::String ("C") : (pn < 0 ? juce::String ((int) (-pn * 100)) + "L" : juce::String ((int) (pn * 100)) + "R"));
    }
    else
    {
        left = "LEN " + juce::String (lenSec, 3) + " s";
        if (std::abs (cached->trimEndSec - cached->trimStartSec - cached->fullLengthSec) > 0.001)
            left += "   (trim " + juce::String (cached->trimStartSec, 2) + " - " + juce::String (cached->trimEndSec, 2) + " of " + juce::String (cached->fullLengthSec, 2) + " s)";
        mid = hitIndex > 0 ? "HIT @ " + juce::String (hitIndex / sr, 3) + " s" : (hitIndex == 0 ? "HIT" : "SWELL ONLY");
    }
    right = juce::String (host.getHostBpm(), 1) + " BPM";
    if (cached->beats > 0) right += "   " + juce::String (cached->beats) + " beats" + (host.isShuffled() ? " (shuffled)" : "");
    g.setColour (col.brighter (0.5f));
    g.drawText (left, 12, by, getWidth() / 2, bh, juce::Justification::centredLeft);
    g.setColour (text);
    g.drawText (mid, 0, by, getWidth(), bh, juce::Justification::centred);
    g.setColour (textDim);
    g.drawText (right, getWidth() / 2, by, getWidth() / 2 - 12, bh, juce::Justification::centredRight);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (textDim.withAlpha (0.7f));
    g.drawText ("click = play   drag = trim end   top strip = trim window   white = volume   pink = pan   wheel = volume", 0, 2, getWidth(), 12, juce::Justification::centred);
}

WaveformDisplay::Drag WaveformDisplay::hitTest (juce::Point<float> m) const
{
    if (total <= 0 || cached == nullptr) return Drag::none;
    auto p = plot(); auto o = overview();
    const float v0 = host.param (IDs::volStart), v1 = host.param (IDs::volEnd), vt = host.param (IDs::volTension);
    const float p0 = host.param (IDs::panStart), p1 = host.param (IDs::panEnd), pt = host.param (IDs::panTension);
    struct H { Drag d; juce::Point<float> c; };
    const H hs[] = { { Drag::volStart, { p.getX(), volY (v0) } }, { Drag::volEnd, { p.getRight(), volY (v1) } }, { Drag::volTension, { p.getCentreX(), volY (v0 + (v1 - v0) * tensionCurve (0.5f, vt)) } },
                     { Drag::panStart, { p.getX(), panY (p0) } }, { Drag::panEnd, { p.getRight(), panY (p1) } }, { Drag::panTension, { p.getCentreX(), panY (p0 + (p1 - p0) * tensionCurve (0.5f, pt)) } } };
    Drag best = Drag::none; float bd = 11.0f;
    for (auto& h : hs) { const float d = m.getDistanceFrom (h.c); if (d < bd) { bd = d; best = h.d; } }
    if (best != Drag::none) return best;
    if (o.contains (m))
    {
        const float fullSec = (float) juce::jmax (0.001, cached->fullLengthSec);
        const float wx0 = o.getX() + o.getWidth() * (float) (cached->trimStartSec / fullSec), wx1 = o.getX() + o.getWidth() * (float) (cached->trimEndSec / fullSec);
        if (std::abs (m.x - wx0) < 7.0f) return Drag::trimStart;
        if (std::abs (m.x - wx1) < 7.0f) return Drag::trimEnd;
        if (m.x > wx0 && m.x < wx1) return Drag::trimWindow;
        return m.x < wx0 ? Drag::trimStart : Drag::trimEnd;
    }
    if (cached->beats > 0 && gateStrip().contains (m)) return Drag::gate;
    if (p.contains (m)) return m.x < p.getX() + 14.0f ? Drag::trimStart : Drag::trimEnd;
    return Drag::none;
}

void WaveformDisplay::mouseMove (const juce::MouseEvent& e)
{
    const Drag h = hitTest (e.position);
    if (h != hover) { hover = h; repaint(); }
    const bool vertical = h == Drag::volStart || h == Drag::volEnd || h == Drag::volTension || h == Drag::panStart || h == Drag::panEnd || h == Drag::panTension;
    setMouseCursor (vertical ? juce::MouseCursor::UpDownResizeCursor : h == Drag::gate ? juce::MouseCursor::PointingHandCursor
                    : h == Drag::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    mouseMove (e);
    drag = hover;
    moved = false;
    downPos = e.position;
    lastGateBeat = -1;
    switch (drag)
    {
        case Drag::trimEnd:    downA = host.param (IDs::trimEnd); downB = host.param (IDs::trimStart); break;
        case Drag::trimStart:  downA = host.param (IDs::trimStart); downB = host.param (IDs::trimEnd); break;
        case Drag::trimWindow: downA = host.param (IDs::trimStart); downB = host.param (IDs::trimEnd); break;
        case Drag::volStart:   downA = host.param (IDs::volStart); break;
        case Drag::volEnd:     downA = host.param (IDs::volEnd); break;
        case Drag::volTension: downA = host.param (IDs::volTension); break;
        case Drag::panStart:   downA = host.param (IDs::panStart); break;
        case Drag::panEnd:     downA = host.param (IDs::panEnd); break;
        case Drag::panTension: downA = host.param (IDs::panTension); break;
        case Drag::gate:
        {
            if (cached == nullptr || cached->beats <= 0) break;
            auto gs = gateStrip();
            const int b = juce::jlimit (0, cached->beats - 1, (int) ((e.position.x - gs.getX()) / gs.getWidth() * (float) cached->beats));
            host.toggleBeat (b); lastGateBeat = b; moved = true;
            break;
        }
        default: break;
    }
    downSpan = juce::jmax (0.01f, std::abs (downB - downA));
    gestureIds.clear();
    switch (drag)
    {
        case Drag::trimEnd: case Drag::trimStart: case Drag::trimWindow: gestureIds = { IDs::trimStart, IDs::trimEnd }; break;
        case Drag::volStart: gestureIds = { IDs::volStart }; break;
        case Drag::volEnd: gestureIds = { IDs::volEnd }; break;
        case Drag::volTension: gestureIds = { IDs::volTension }; break;
        case Drag::panStart: gestureIds = { IDs::panStart }; break;
        case Drag::panEnd: gestureIds = { IDs::panEnd }; break;
        case Drag::panTension: gestureIds = { IDs::panTension }; break;
        default: break;
    }
    for (auto& id : gestureIds) host.beginGesture (id);
    if (e.mods.isRightButtonDown() || e.getNumberOfClicks() > 1)
    {
        if (drag == Drag::volStart) host.setParam (IDs::volStart, 1.0f);
        if (drag == Drag::volEnd) host.setParam (IDs::volEnd, 1.0f);
        if (drag == Drag::volTension) host.setParam (IDs::volTension, 0.0f);
        if (drag == Drag::panStart) host.setParam (IDs::panStart, 0.0f);
        if (drag == Drag::panEnd) host.setParam (IDs::panEnd, 0.0f);
        if (drag == Drag::panTension) host.setParam (IDs::panTension, 0.0f);
        if (drag == Drag::trimStart || drag == Drag::trimEnd || drag == Drag::trimWindow) { host.setParam (IDs::trimStart, 0.0f); host.setParam (IDs::trimEnd, 1.0f); }
        if (drag == Drag::gate && e.mods.isRightButtonDown()) host.setAllBeats (true);
        drag = Drag::none;
        moved = true;
    }
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const float dx = e.position.x - downPos.x, dy = e.position.y - downPos.y;
    if (std::abs (dx) > 2.0f || std::abs (dy) > 2.0f) moved = true;
    if (! moved) return;
    auto p = plot(); auto o = overview();
    const float nx = dx / p.getWidth(), ny = -dy / p.getHeight();
    const float ox = dx / o.getWidth();
    const bool inOverview = o.contains (downPos);
    switch (drag)
    {
        case Drag::trimEnd:
            if (inOverview) host.setParamValue (IDs::trimEnd, juce::jlimit (downB + 0.02f, 1.0f, downA + ox));
            else host.setParamValue (IDs::trimEnd, juce::jlimit (downB + 0.02f, 1.0f, downA - nx * downSpan));
            break;
        case Drag::trimStart:
            if (inOverview) host.setParamValue (IDs::trimStart, juce::jlimit (0.0f, downB - 0.02f, downA + ox));
            else host.setParamValue (IDs::trimStart, juce::jlimit (0.0f, downB - 0.02f, downA + nx * downSpan));
            break;
        case Drag::trimWindow:
        {
            const float w = downB - downA;
            const float s = juce::jlimit (0.0f, 1.0f - w, downA + ox);
            host.setParamValue (IDs::trimStart, s); host.setParamValue (IDs::trimEnd, s + w);
            break;
        }
        case Drag::volStart:   host.setParamValue (IDs::volStart, juce::jlimit (0.0f, 1.0f, downA + ny)); break;
        case Drag::volEnd:     host.setParamValue (IDs::volEnd,   juce::jlimit (0.0f, 1.0f, downA + ny)); break;
        case Drag::volTension: host.setParamValue (IDs::volTension, juce::jlimit (-1.0f, 1.0f, downA + ny * 3.0f * (host.param (IDs::volEnd) >= host.param (IDs::volStart) ? -1.0f : 1.0f))); break;
        case Drag::panStart:   host.setParamValue (IDs::panStart, juce::jlimit (-1.0f, 1.0f, downA + ny * 2.2f)); break;
        case Drag::panEnd:     host.setParamValue (IDs::panEnd,   juce::jlimit (-1.0f, 1.0f, downA + ny * 2.2f)); break;
        case Drag::panTension: host.setParamValue (IDs::panTension, juce::jlimit (-1.0f, 1.0f, downA + ny * 3.0f * (host.param (IDs::panEnd) >= host.param (IDs::panStart) ? -1.0f : 1.0f))); break;
        case Drag::gate:
        {
            if (cached == nullptr || cached->beats <= 0) break;
            auto gs = gateStrip();
            const int b = juce::jlimit (0, cached->beats - 1, (int) ((e.position.x - gs.getX()) / gs.getWidth() * (float) cached->beats));
            if (b != lastGateBeat) { host.toggleBeat (b); lastGateBeat = b; }
            break;
        }
        default: break;
    }
}

void WaveformDisplay::mouseUp (const juce::MouseEvent&)
{
    for (auto& id : gestureIds) host.endGesture (id);
    gestureIds.clear();
    if (! moved && (drag == Drag::trimEnd || drag == Drag::trimStart) && ! overview().contains (downPos))
        host.triggerPreview();
    drag = Drag::none;
    repaint();
}

void WaveformDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (total <= 0) return;
    const float step = (w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.01f : 0.05f);
    const Drag h = hitTest (e.position);
    if (h == Drag::volStart) host.setParam (IDs::volStart, juce::jlimit (0.0f, 1.0f, host.param (IDs::volStart) + step));
    else if (h == Drag::volEnd) host.setParam (IDs::volEnd, juce::jlimit (0.0f, 1.0f, host.param (IDs::volEnd) + step));
    else
    {
        host.setParam (IDs::volStart, juce::jlimit (0.0f, 1.0f, host.param (IDs::volStart) + step));
        host.setParam (IDs::volEnd, juce::jlimit (0.0f, 1.0f, host.param (IDs::volEnd) + step));
    }
}

// ---------------- Knob right-click ----------------

double RVSlider::clipboard = 0.0;
bool RVSlider::clipboardValid = false;

void RVSlider::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || parameter == nullptr) { juce::Slider::mouseDown (e); return; }
    juce::PopupMenu m;
    m.addSectionHeader (parameter->getName (64));
    m.addItem (1, "Set value...");
    m.addItem (2, "Reset to default");
    m.addItem (5, "Copy value");
    m.addItem (6, "Paste value", clipboardValid);
    m.addItem (5, "Copy value");
    m.addItem (6, "Paste value", juce::SystemClipboard::getTextFromClipboard().trim().containsOnly ("0123456789.-+eE") && juce::SystemClipboard::getTextFromClipboard().trim().isNotEmpty());
    m.addItem (5, "Copy value");
    m.addItem (6, "Paste value", juce::SystemClipboard::getTextFromClipboard().trim().containsOnly ("0123456789.-+eE") && juce::SystemClipboard::getTextFromClipboard().trim().isNotEmpty());
    m.addItem (7, juce::String ("Fine adjust: hold Ctrl/Cmd while dragging"), false);
    m.addItem (8, juce::String ("Double-click the value box to type"), false);
    std::unique_ptr<juce::HostProvidedContextMenu> hostMenu;
    if (editor != nullptr)
        if (auto ctx = editor->getHostContext())
            hostMenu = ctx->getContextMenuForParameter (parameter);
    if (hostMenu != nullptr) { m.addSeparator(); m.addItem (3, "DAW menu (automation, MIDI learn)..."); }
    m.addSeparator();
    m.addItem (4, "How to automate this knob in your DAW...");
    juce::Component::SafePointer<RVSlider> safe (this);
    auto* hm = hostMenu.release();
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, hm] (int r)
    {
        std::unique_ptr<juce::HostProvidedContextMenu> hostM (hm);
        if (safe == nullptr) return;
        auto* self = safe.getComponent();
        if (r == 1)
        {
            auto* aw = new juce::AlertWindow ("Set " + self->parameter->getName (64), "Value (" + self->parameter->getLabel() + "):", juce::MessageBoxIconType::NoIcon, self);
            aw->addTextEditor ("v", juce::String (self->getValue(), 3));
            aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            juce::Component::SafePointer<RVSlider> safe2 (self);
            aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe2, aw] (int rr)
            {
                if (rr == 1 && safe2 != nullptr) safe2->setValue (aw->getTextEditorContents ("v").getDoubleValue(), juce::sendNotificationSync);
            }), true);
        }
        else if (r == 2) self->setValue (self->parameter->convertFrom0to1 (self->parameter->getDefaultValue()), juce::sendNotificationSync);
        else if (r == 5) { clipboard = self->getValue(); clipboardValid = true; }
        else if (r == 6 && clipboardValid) self->setValue (juce::jlimit (self->getMinimum(), self->getMaximum(), clipboard), juce::sendNotificationSync);
        else if (r == 5) juce::SystemClipboard::copyTextToClipboard (juce::String (self->getValue(), 4));
        else if (r == 6) self->setValue (juce::SystemClipboard::getTextFromClipboard().getDoubleValue(), juce::sendNotificationSync);
        else if (r == 5) juce::SystemClipboard::copyTextToClipboard (juce::String (self->getValue(), 4));
        else if (r == 6) self->setValue (juce::SystemClipboard::getTextFromClipboard().trim().getDoubleValue(), juce::sendNotificationSync);
        else if (r == 3 && hostM != nullptr) hostM->showNativeMenu (self->getScreenBounds().getCentre());
        else if (r == 4)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Automating " + self->parameter->getName (64),
                "Every knob is a plugin parameter, so your DAW can automate it.\n\n"
                "FL Studio: move this knob, then in the plugin wrapper (top-left arrow menu) choose 'Create automation clip' for the last tweaked parameter, "
                "or right-click the wrapper's parameter list (Browse parameters) and pick '" + self->parameter->getName (64) + "'. You can also 'Link to controller' the same way.\n\n"
                "Ableton: click Configure in the device, move the knob. Bitwig/Reaper/Studio One/Logic: pick '" + self->parameter->getName (64) + "' from the plugin's parameter list in the automation lane.", juce::String(), self);
    });
}

// ---------------- Tension box ----------------

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
    const float t = ps.param (paramId);
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
    auto base = host.exportBaseName();
    if (base.isEmpty()) return;
    dragging = true;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ReverseVerb");
    dir.createDirectory();
    for (auto& old : dir.findChildFiles (juce::File::findFiles, false, "*.wav"))   // tidy files older than an hour
        if (old.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::hours (1)) old.deleteFile();
    auto tmp = dir.getChildFile (base + "_reverse_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt (0x7fffff)) + ".wav");
    if (host.exportWav (tmp))
        if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            juce::Component::SafePointer<DragOutPad> safe (this);
            dc->performExternalDragDropOfFiles ({ tmp.getFullPathName() }, false, this, [safe] { if (safe != nullptr) safe->dragging = false; });
            return;
        }
    dragging = false;
}

// ---------------- Help ----------------

HelpOverlay::HelpOverlay (const juce::String& helpText)
{
    body.setMultiLine (true);
    body.setReadOnly (true);
    body.setScrollbarsShown (true);
    body.setCaretVisible (false);
    body.setFont (juce::Font (juce::FontOptions (13.0f)));
    body.setText (helpText);
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

// ---------------- Preset bar ----------------

PresetBar::PresetBar (PresetManager& m) : pm (m)
{
    styleLabel (label, 10.5f, true, textDim, juce::Justification::centredRight);
    label.setText ("PRESET", juce::dontSendNotification);
    addAndMakeVisible (label);
    addAndMakeVisible (combo);
    for (auto* b : { &prevB, &nextB, &saveB, &delB, &folderB }) addAndMakeVisible (b);
    combo.setTextWhenNothingSelected ("(custom)");
    combo.onChange = [this] { const int i = combo.getSelectedId() - 1; if (i >= 0 && i != pm.getCurrentIndex()) pm.load (i); };
    prevB.onClick = [this] { auto n = pm.getNames().size(); if (n > 0) { pm.load ((pm.getCurrentIndex() - 1 + n) % n); refresh(); } };
    nextB.onClick = [this] { auto n = pm.getNames().size(); if (n > 0) { pm.load ((pm.getCurrentIndex() + 1) % n); refresh(); } };
    saveB.onClick = [this] { saveDialog(); };
    delB.onClick  = [this]
    {
        const int i = pm.getCurrentIndex();
        if (i < pm.getNumFactory()) return;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete preset", "Delete \"" + pm.getCurrentName() + "\"?", "Delete", "Cancel", this,
            juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) { pm.remove (pm.getCurrentIndex()); refresh(); } }));
    };
    folderB.onClick = [this] { pm.getFolder().revealToUser(); };
    refresh();
    startTimer (500);
}

void PresetBar::refresh()
{
    auto names = pm.getNames();
    combo.clear (juce::dontSendNotification);
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == pm.getNumFactory() && names.size() > pm.getNumFactory()) combo.addSeparator();
        combo.addItem (names[i], i + 1);
    }
    combo.setSelectedId (pm.getCurrentIndex() + 1, juce::dontSendNotification);
    delB.setEnabled (pm.getCurrentIndex() >= pm.getNumFactory());
    delB.setAlpha (delB.isEnabled() ? 1.0f : 0.4f);
}

void PresetBar::timerCallback()
{
    if (combo.getSelectedId() != pm.getCurrentIndex() + 1) refresh();
}

void PresetBar::saveDialog()
{
    auto* aw = new juce::AlertWindow ("Save preset", "Name:", juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", pm.getCurrentIndex() >= pm.getNumFactory() ? pm.getCurrentName() : juce::String(), "Preset name");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r == 1) { pm.save (aw->getTextEditorContents ("name")); refresh(); }
    }), true);
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromLeft (52));
    r.removeFromLeft (6);
    folderB.setBounds (r.removeFromRight (36)); r.removeFromRight (4);
    delB.setBounds (r.removeFromRight (44));    r.removeFromRight (4);
    saveB.setBounds (r.removeFromRight (56));   r.removeFromRight (8);
    nextB.setBounds (r.removeFromRight (30));   r.removeFromRight (3);
    prevB.setBounds (r.removeFromRight (30));   r.removeFromRight (6);
    combo.setBounds (r);
}

// ---------------- knob helpers ----------------

void styleLabel (juce::Label& l, float size, bool bold, juce::Colour col, juce::Justification j)
{
    l.setFont (juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)));
    l.setJustificationType (j);
    l.setColour (juce::Label::textColourId, col);
}

std::unique_ptr<Knob> makeKnob (juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& textName, juce::Colour fill)
{
    auto k = std::make_unique<Knob>();
    auto& s = k->slider;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setColour (juce::Slider::rotarySliderFillColourId, fill);
    s.parameter = apvts.getParameter (id);
    s.editor = dynamic_cast<juce::AudioProcessorEditor*> (&parent);
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (s.parameter)) s.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
    s.setTooltip (textName + "  (right-click: set value, copy/paste, reset, DAW automation)");
    parent.addAndMakeVisible (s);
    k->label.setText (textName, juce::dontSendNotification);
    styleLabel (k->label, 10.5f, true, textDim);
    parent.addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, s);
    return k;
}

void layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> ks)
{
    if (ks.size() == 0) return;
    const int kw = area.getWidth() / (int) ks.size();
    for (auto* k : ks)
    {
        auto cell = area.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (14));
        k->slider.setBounds (cell);
    }
}
