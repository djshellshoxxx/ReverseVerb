// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "DiffusionShape.h"

using namespace RVColours;

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
