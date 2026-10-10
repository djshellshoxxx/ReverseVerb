// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "WaveformDisplay.h"

using namespace RVColours;

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

void WaveformDisplay::timerCallback()
{
    auto r = proc.getRendered();
    bool dirty = false;
    if (r != cached) { cached = r; rebuild(); staticDirty = true; dirty = true; }
    const int ph = proc.getPlayheadPosition();
    if (ph != lastPlayhead) { lastPlayhead = ph; dirty = true; }
    const float tone = proc.param (IDs::tone), bass = proc.param (IDs::basscut);
    const float v0 = proc.param (IDs::volStart), v1 = proc.param (IDs::volEnd), vt = proc.param (IDs::volTension);
    if (tone != lastTone || bass != lastBass) staticDirty = true;
    if (tone != lastTone || bass != lastBass || v0 != lastV0 || v1 != lastV1 || vt != lastVT) { lastTone = tone; lastBass = bass; lastV0 = v0; lastV1 = v1; lastVT = vt; dirty = true; }
    if (dirty) repaint();
}

void WaveformDisplay::rebuild()
{
    swellPath.clear(); hitPath.clear();
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
}

void WaveformDisplay::renderStatic (float scale)
{
    const int w = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)), h = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));
    staticImage = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (staticImage);
    g.addTransform (juce::AffineTransform::scale ((float) w / (float) juce::jmax (1, getWidth()), (float) h / (float) juce::jmax (1, getHeight())));
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

    g.setColour (outline.withAlpha (0.35f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (p.getY() + p.getHeight() * i / 4.0f), p.getX(), p.getRight());
    g.setColour (outline.withAlpha (0.8f));
    g.drawHorizontalLine ((int) p.getCentreY(), p.getX(), p.getRight());

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
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto p = plot();
    const juce::Colour col = swellColour (proc.param (IDs::tone), proc.param (IDs::basscut));

    // static layer (background, grid, waveform) is cached as an image and only rebuilt when it changes
    const float scale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    const int wantW = juce::roundToInt ((float) getWidth() * scale), wantH = juce::roundToInt ((float) getHeight() * scale);
    if (staticDirty || staticImage.getWidth() != wantW || staticImage.getHeight() != wantH) { renderStatic (scale); staticDirty = false; }
    g.drawImage (staticImage, getLocalBounds().toFloat());

    if (total <= 0 || cached == nullptr)
    {
        g.setColour (textDim);
        g.setFont (juce::Font (juce::FontOptions (15.0f)));
        g.drawText ("Drop a snare / hat / clap here, or hit LOAD", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const double sr = cached->sampleRate;
    const double lenSec = total / sr;

    // beat lines (sync)
    if (cached->beats > 0)
    {
        const double beatSec = cached->fullLengthSec / cached->beats;
        const double span = cached->trimEndSec - cached->trimStartSec;
        for (int b = 0; b <= cached->beats; ++b)
        {
            const double t = b * beatSec - cached->trimStartSec;
            if (t < -0.0005 || t > span + 0.0005) continue;
            const float x = p.getX() + p.getWidth() * (float) (t / span);
            const bool bar = (b % cached->beatsPerBar) == 0;
            g.setColour (bar ? juce::Colour (0xffff4d4d).withAlpha (0.9f) : juce::Colour (0xffff4d4d).withAlpha (0.45f));
            g.drawLine (x, p.getY(), x, p.getBottom(), bar ? 1.5f : 1.0f);
        }
    }

    if (hitIndex >= 0)
    {
        const float sx = p.getX() + p.getWidth() * (float) hitIndex / (float) total;
        g.setColour (text.withAlpha (0.5f));
        const float dash[] = { 3.0f, 3.0f };
        juce::Path l; l.startNewSubPath (sx, p.getY()); l.lineTo (sx, p.getBottom());
        juce::Path dl; juce::PathStrokeType (1.0f).createDashedStroke (dl, l, dash, 2);
        g.fillPath (dl);
    }

    // volume envelope line
    const float v0 = proc.param (IDs::volStart), v1 = proc.param (IDs::volEnd), vt = proc.param (IDs::volTension);
    juce::Path vol;
    const int steps = 64;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        const float lvl = v0 + (v1 - v0) * tensionCurve (x, vt);
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
    g.drawText ("click = play    drag = trim    dots = volume", 0, 2, getWidth(), 12, juce::Justification::centred);
}

void WaveformDisplay::mouseMove (const juce::MouseEvent& e)
{
    Drag h = Drag::none;
    if (total > 0)
    {
        auto p = plot();
        const float v0 = proc.param (IDs::volStart), v1 = proc.param (IDs::volEnd), vt = proc.param (IDs::volTension);
        const juce::Point<float> ps (p.getX(), volY (v0)), pe (p.getRight(), volY (v1)), pm (p.getCentreX(), volY (v0 + (v1 - v0) * tensionCurve (0.5f, vt)));
        if (e.position.getDistanceFrom (ps) < 10.0f) h = Drag::volStart;
        else if (e.position.getDistanceFrom (pe) < 10.0f) h = Drag::volEnd;
        else if (e.position.getDistanceFrom (pm) < 10.0f) h = Drag::volTension;
        else if (e.x < p.getX() + 14.0f) h = Drag::trimStart;
        else h = Drag::trimEnd;
    }
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h == Drag::volStart || h == Drag::volEnd || h == Drag::volTension ? juce::MouseCursor::UpDownResizeCursor
                    : (h == Drag::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor));
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    mouseMove (e);
    drag = hover;
    moved = false;
    downPos = e.position;
    switch (drag)
    {
        case Drag::trimEnd:    downA = proc.param (IDs::trimEnd); downB = proc.param (IDs::trimStart); break;
        case Drag::trimStart:  downA = proc.param (IDs::trimStart); downB = proc.param (IDs::trimEnd); break;
        case Drag::volStart:   downA = proc.param (IDs::volStart); break;
        case Drag::volEnd:     downA = proc.param (IDs::volEnd); break;
        case Drag::volTension: downA = proc.param (IDs::volTension); break;
        default: break;
    }
    downSpan = juce::jmax (0.01f, std::abs (downB - downA));
    if (e.mods.isRightButtonDown() || e.getNumberOfClicks() > 1)
    {
        if (drag == Drag::volStart) proc.setParam (IDs::volStart, 1.0f);
        if (drag == Drag::volEnd) proc.setParam (IDs::volEnd, 1.0f);
        if (drag == Drag::volTension) proc.setParam (IDs::volTension, 0.0f);
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
        default: break;
    }
}

void WaveformDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! moved && drag != Drag::none && drag != Drag::volStart && drag != Drag::volEnd && drag != Drag::volTension)
        proc.triggerPreview();
    drag = Drag::none;
    repaint();
}
