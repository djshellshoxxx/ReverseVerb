// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "DragOutPad.h"

using namespace RVColours;

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
