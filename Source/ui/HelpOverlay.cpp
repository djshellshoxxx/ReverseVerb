// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "HelpOverlay.h"

using namespace RVColours;

// ---------------- Help ----------------

static const char* kHelpText = R"(REVERSE VERB - what everything does

WORKFLOW
  LOAD (or drop a file on the window) picks a hit. < > steps through every sample in that folder and auto-plays it with your current settings.
  Notes in the piano roll trigger the sound (velocity = volume). Click the waveform or PLAY to audition.
  EXPORT WAV saves the rendered sample. DRAG TO DAW: drag the pad straight into the channel rack / playlist.
  Hit on note (PDC): reports the swell length as latency so the DRY HIT lands exactly on the note and the swell starts early. Turn off if you'd rather place notes early yourself.
  RESET EDITS clears trim, pitch and volume envelope. RANDOM rolls new reverb settings.

WAVEFORM
  Colour follows the COLOR knob (violet = dark, cyan = bright) and turns red as BASS CUT rises.
  Drag anywhere to trim: drag right = shorter, drag left = longer. Drag near the left edge to trim the start. Trim the hit off entirely for a pure swell.
  Volume line: drag the left / right dots up or down (bottom = -inf dB, top = 0 dB). Drag the middle square to bend the curve (tension). Double-click a dot to reset it.
  Red lines are beats (bright = bar) when SYNC is on. Dashed line = where the dry hit begins.
  Bottom row shows length, hit position, BPM, and live time / pitch / volume while playing.

REVERB
  SIZE: room dimensions.  DECAY: how long the tail rings.  DAMP: high-frequency absorption.
  DIFFUSION: smearing of echoes (smooth vs grainy). The SPACE panel shows more faces as diffusion rises.
  EARLY REF: level of first reflections.  SEPARATION: how different left and right are.  WIDTH: stereo spread of the mix.
  DELAY: silence inserted between the end of the swell and the hit.

SWELL
  LENGTH: seconds of reverb tail (disabled when SYNC is on).  SHAPE: bends the swell envelope (negative = fuller early, positive = late rush).
  COLOR: low-pass filter on the swell.  BASS CUT: high-pass filter on the swell, keeps sub out of your break.

SYNC
  SYNC locks the total length (swell + hit) to the host tempo. Pick 1/2/4/8 beats or 1/2/4 bars. Re-renders automatically when BPM changes.

PITCH
  PITCH sweeps the pitch from 0 at the start to the knob amount at the end. Range chooses 1, 2 or 4 octaves. CURVE box: drag up/down to change how fast the sweep happens.

MIX
  HIT: level of the dry hit.  SWELL: level of the reversed reverb.
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
