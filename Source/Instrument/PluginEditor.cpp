#include "PluginEditor.h"

using namespace RVColours;

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

MODE / GENERATE
  MODE: Reverse Reverb (swell rises into the hit), Forward Reverb (hit then normal tail = a fall), Dry (no reverb, just the hit with all edits).
  GENERATE: synthesises a closed hat, snare, clap, kick or rim so you can work without a sample.

STRETCH
  Time-stretches the hit (1x to 32x) into a long even body before the reverb. In Reverse mode that becomes a smooth rise, in Forward mode a fall.
  The envelope is flattened automatically; use SHAPE to curve it.

FX
  Echo, Reverse Echo (echoes before the reverse, so repeats lead into the hit), Chorus, Reverse Chorus. TIME sets delay time or chorus rate, FEEDBACK for echoes, DEPTH for chorus, MIX for level.

PAN LINE (pink)
  Same as the volume line: drag the end dots (top = right, bottom = left) and the middle square for tension. Knobs in the PAN group do the same.

GATE (needs SYNC on)
  Blocks under the waveform: click to mute/unmute a beat, drag across several, right-click to enable all. Muted beats are shaded in the waveform.
  Shape: Hard (on/off), Smooth (each beat fades in and out), LFO (smooth wave between on and off beats). DEPTH: how far muted beats drop.
  SHUFFLE plays the beats in a random order; UNSHUFFLE restores it. Sync length goes up to 32 bars.

OUTPUT
  BASS / TREBLE: shelf EQ on the final result. NORMALIZE: peak to 0 dB. SWAP L/R, INVERT PHASE.
  Mouse wheel over the waveform nudges the volume line (over a dot: that dot only, Shift = fine).
  Right-click any knob: set an exact value, reset, or open your DAW's automation menu.

PRESETS
  Factory presets are built in. SAVE stores your own in Documents/ReverseVerb/Instrument (the ... button opens that folder).
  Presets store settings only, not the sample.
)";



// ---------------- Editor ----------------

ReverseVerbEditor::ReverseVerbEditor (ReverseVerbProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (p.presets), waveform (p), shape (p), dragPad (p), pitchTension (p, IDs::pitchTension), help (kHelpText)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    title.setText ("REVERSE VERB", juce::dontSendNotification);
    styleLabel (title, 24.0f, true, text, juce::Justification::centredLeft);
    addAndMakeVisible (title);
    subtitle.setText ("reverse reverb swell for hits", juce::dontSendNotification);
    styleLabel (subtitle, 12.0f, false, textDim, juce::Justification::centredLeft);
    addAndMakeVisible (subtitle);
    styleLabel (fileLabel, 13.0f, false, text);
    fileLabel.setColour (juce::Label::backgroundColourId, panel);
    fileLabel.setColour (juce::Label::outlineColourId, outline);
    addAndMakeVisible (fileLabel);
    styleLabel (countLabel, 11.0f, false, textDim);
    addAndMakeVisible (countLabel);

    for (auto* b : { &prevButton, &nextButton, &loadButton, &generateButton, &playButton, &exportButton, &resetButton, &randomButton, &helpButton, &shuffleButton, &unshuffleButton, &allOnButton })
        addAndMakeVisible (b);
    for (auto* t : { &alignToggle, &syncToggle, &normToggle, &swapToggle, &phaseToggle }) addAndMakeVisible (t);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (waveform);
    addAndMakeVisible (shape);
    addAndMakeVisible (dragPad);
    addAndMakeVisible (pitchTension);

    combo (syncCombo, gateLabel, IDs::syncLen, "", syncChoiceNames(), syncComboAtt);
    combo (rangeCombo, rangeLabel, IDs::pitchRange, "RANGE", { "1 oct", "2 oct", "4 oct" }, rangeComboAtt);
    combo (modeCombo, modeLabel, IDs::mode, "MODE", { "Reverse Reverb", "Forward Reverb", "Dry" }, modeComboAtt);
    combo (fxCombo, fxLabel, IDs::fxType, "TYPE", { "Off", "Echo", "Reverse Echo", "Chorus", "Reverse Chorus" }, fxComboAtt);
    combo (gateCombo, gateLabel, IDs::gateShape, "SHAPE", { "Hard", "Smooth", "LFO" }, gateComboAtt);
    alignAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::align, alignToggle);
    syncAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::sync, syncToggle);
    normAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::normalize, normToggle);
    swapAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::swapStereo, swapToggle);
    phaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::invertPhase, phaseToggle);

    prevButton.onClick      = [this] { proc.prevSample(); };
    nextButton.onClick      = [this] { proc.nextSample(); };
    playButton.onClick      = [this] { proc.triggerPreview(); };
    resetButton.onClick     = [this] { proc.resetEdits(); };
    randomButton.onClick    = [this] { proc.randomizeReverb(); };
    shuffleButton.onClick   = [this] { proc.shuffleBeats (false); };
    unshuffleButton.onClick = [this] { proc.shuffleBeats (true); };
    allOnButton.onClick     = [this] { proc.setAllBeats (true); };
    helpButton.onClick      = [this] { help.setVisible (true); help.toFront (true); };
    generateButton.onClick  = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Closed hat"); m.addItem (2, "Snare"); m.addItem (3, "Clap"); m.addItem (4, "Kick"); m.addItem (5, "Rim");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&generateButton), [this] (int r) { if (r > 0) proc.generate (r - 1); });
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
        auto base = proc.exportBaseName();
        if (base.isEmpty()) return;
        auto dir = proc.getCurrentFile().existsAsFile() ? proc.getCurrentFile().getParentDirectory() : juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
        chooser = std::make_unique<juce::FileChooser> ("Export sample", dir.getChildFile (base + "_reverse.wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) proc.exportWav (f.withFileExtension ("wav")); });
    };

    kSize = &knob (IDs::size, "SIZE");       kDecay = &knob (IDs::decay, "DECAY");   kDamp = &knob (IDs::damp, "DAMP");
    kDiff = &knob (IDs::diff, "DIFFUSION");  kEr = &knob (IDs::er, "EARLY REF");     kSep = &knob (IDs::sep, "SEPARATION");
    kWidth = &knob (IDs::width, "WIDTH");    kGap = &knob (IDs::gap, "DELAY");
    kTail = &knob (IDs::tail, "LENGTH");     kShape = &knob (IDs::shape, "SHAPE");   kTone = &knob (IDs::tone, "COLOR");
    kBass = &knob (IDs::basscut, "BASS CUT"); kStretch = &knob (IDs::stretch, "STRETCH");
    kDry = &knob (IDs::dry, "HIT", hitCol);  kWet = &knob (IDs::wet, "SWELL");
    kBassB = &knob (IDs::bassBoost, "BASS"); kTrebB = &knob (IDs::trebleBoost, "TREBLE");
    kPitch = &knob (IDs::pitch, "PITCH");
    kVolStart = &knob (IDs::volStart, "START"); kVolEnd = &knob (IDs::volEnd, "END"); kVolTension = &knob (IDs::volTension, "TENSION");
    kFxTime = &knob (IDs::fxTime, "TIME / RATE"); kFxFb = &knob (IDs::fxFeedback, "FEEDBACK"); kFxDepth = &knob (IDs::fxDepth, "DEPTH"); kFxMix = &knob (IDs::fxMix, "MIX");
    kPanStart = &knob (IDs::panStart, "START", panCol); kPanEnd = &knob (IDs::panEnd, "END", panCol); kPanTension = &knob (IDs::panTension, "TENSION", panCol);
    kGateDepth = &knob (IDs::gateDepth, "DEPTH", alert);

    addChildComponent (help);
    setSize (1120, 940);
    startTimerHz (10);
    timerCallback();
}

ReverseVerbEditor::~ReverseVerbEditor() { setLookAndFeel (nullptr); }

Knob& ReverseVerbEditor::knob (const juce::String& id, const juce::String& textName, juce::Colour fill)
{
    knobs.push_back (makeKnob (*this, proc.apvts, id, textName, fill));
    return *knobs.back();
}

void ReverseVerbEditor::combo (juce::ComboBox& c, juce::Label& l, const juce::String& id, const juce::String& labelText, const juce::StringArray& items,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    c.addItemList (items, 1);
    addAndMakeVisible (c);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c);
    if (labelText.isNotEmpty()) { l.setText (labelText, juce::dontSendNotification); styleLabel (l, 10.0f, true, textDim); addAndMakeVisible (l); }
}

void ReverseVerbEditor::timerCallback()
{
    auto name = proc.getSourceName();
    fileLabel.setText (name.isNotEmpty() ? name : "no sample loaded", juce::dontSendNotification);
    const int n = proc.getSampleCount();
    countLabel.setText (n > 0 && proc.getCurrentFile().existsAsFile() ? juce::String (proc.getSampleIndex() + 1) + " / " + juce::String (n) : "", juce::dontSendNotification);
    const bool sync = proc.param (IDs::sync) > 0.5f;
    kTail->slider.setEnabled (! sync); kTail->slider.setAlpha (sync ? 0.4f : 1.0f);
    for (auto* c : { &syncCombo, &gateCombo }) { c->setEnabled (sync); c->setAlpha (sync ? 1.0f : 0.5f); }
    for (auto* b : { &shuffleButton, &unshuffleButton, &allOnButton }) { b->setEnabled (sync); b->setAlpha (sync ? 1.0f : 0.5f); }
    kGateDepth->slider.setEnabled (sync); kGateDepth->slider.setAlpha (sync ? 1.0f : 0.5f);
    const int fx = (int) proc.param (IDs::fxType);
    const bool echo = fx == 1 || fx == 2, chorus = fx == 3 || fx == 4;
    kFxFb->slider.setAlpha (echo ? 1.0f : 0.4f); kFxDepth->slider.setAlpha (chorus ? 1.0f : 0.4f);
    kFxTime->slider.setAlpha (fx > 0 ? 1.0f : 0.4f); kFxMix->slider.setAlpha (fx > 0 ? 1.0f : 0.4f);
    const juce::Colour col = swellColour (proc.param (IDs::tone), proc.param (IDs::basscut));
    for (auto* k : { kTone, kBass, kWet, kTail, kShape, kStretch })
        if (k->slider.findColour (juce::Slider::rotarySliderFillColourId) != col) { k->slider.setColour (juce::Slider::rotarySliderFillColourId, col); k->slider.repaint(); }
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

void ReverseVerbEditor::resized()
{
    help.setBounds (getLocalBounds());
    groups.clear();
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (46);
    auto titleArea = header.removeFromLeft (230);
    title.setBounds (titleArea.removeFromTop (28));
    subtitle.setBounds (titleArea);
    helpButton.setBounds (header.removeFromRight (34).reduced (0, 7));
    header.removeFromRight (10);
    auto browser = header.withTrimmedLeft (20);
    generateButton.setBounds (browser.removeFromRight (90).reduced (0, 7)); browser.removeFromRight (6);
    loadButton.setBounds (browser.removeFromRight (70).reduced (0, 7));     browser.removeFromRight (8);
    nextButton.setBounds (browser.removeFromRight (40).reduced (0, 7));     browser.removeFromRight (4);
    prevButton.setBounds (browser.removeFromRight (40).reduced (0, 7));     browser.removeFromRight (8);
    countLabel.setBounds (browser.removeFromRight (56));
    fileLabel.setBounds (browser.reduced (0, 7));

    area.removeFromTop (6);
    presetBar.setBounds (area.removeFromTop (26));

    area.removeFromTop (10);
    auto vis = area.removeFromTop (250);
    shape.setBounds (vis.removeFromLeft (220));
    vis.removeFromLeft (10);
    waveform.setBounds (vis);

    area.removeFromTop (10);
    auto row = area.removeFromTop (34);
    playButton.setBounds (row.removeFromLeft (70));      row.removeFromLeft (6);
    exportButton.setBounds (row.removeFromLeft (100));   row.removeFromLeft (6);
    dragPad.setBounds (row.removeFromLeft (110));        row.removeFromLeft (6);
    resetButton.setBounds (row.removeFromLeft (96));     row.removeFromLeft (6);
    randomButton.setBounds (row.removeFromLeft (76));    row.removeFromLeft (12);
    modeLabel.setBounds (row.removeFromLeft (42));
    modeCombo.setBounds (row.removeFromLeft (130).reduced (0, 4)); row.removeFromLeft (10);
    alignToggle.setBounds (row.removeFromLeft (150));
    syncCombo.setBounds (row.removeFromRight (90).reduced (0, 4)); row.removeFromRight (6);
    syncToggle.setBounds (row.removeFromRight (64));

    area.removeFromTop (12);
    const int rowH = (area.getHeight() - 20) / 3;
    auto rowA = area.removeFromTop (rowH); area.removeFromTop (10);
    auto rowB = area.removeFromTop (rowH); area.removeFromTop (10);
    auto rowC = area;

    auto group = [&] (juce::Rectangle<int>& src, int width, const juce::String& name)
    {
        auto r = src.removeFromLeft (width);
        src.removeFromLeft (8);
        groups.push_back ({ name, r });
        return r.reduced (6).withTrimmedTop (14);
    };

    layoutKnobs (group (rowA, rowA.getWidth(), "REVERB"), { kSize, kDecay, kDamp, kDiff, kEr, kSep, kWidth, kGap });

    {
        const int unit = (rowB.getWidth() - 8 * 3) / 14;
        layoutKnobs (group (rowB, unit * 5, "SWELL"), { kTail, kShape, kTone, kBass, kStretch });
        layoutKnobs (group (rowB, unit * 4, "MIX  +  TONE"), { kDry, kWet, kBassB, kTrebB });
        auto pitchArea = group (rowB, unit * 2 + 30, "PITCH");
        auto right = pitchArea.removeFromRight (74);
        rangeLabel.setBounds (right.removeFromTop (14));
        rangeCombo.setBounds (right.removeFromTop (26).reduced (2, 0));
        right.removeFromTop (6);
        pitchTension.setBounds (right.withSizeKeepingCentre (64, juce::jmin (64, right.getHeight())));
        layoutKnobs (pitchArea, { kPitch });
        layoutKnobs (group (rowB, rowB.getWidth(), "VOLUME  (white line)"), { kVolStart, kVolEnd, kVolTension });
    }
    {
        const int unit = (rowC.getWidth() - 8 * 3) / 14;
        auto fxArea = group (rowC, unit * 5, "FX  (on the swell)");
        auto left = fxArea.removeFromLeft (110);
        fxLabel.setBounds (left.removeFromTop (14));
        fxCombo.setBounds (left.removeFromTop (26).reduced (2, 0));
        layoutKnobs (fxArea, { kFxTime, kFxFb, kFxDepth, kFxMix });
        layoutKnobs (group (rowC, unit * 3, "PAN  (pink line)"), { kPanStart, kPanEnd, kPanTension });
        auto gateArea = group (rowC, unit * 3 + 40, "GATE  (sync on)");
        auto gl = gateArea.removeFromLeft (100);
        gateLabel.setBounds (gl.removeFromTop (14));
        gateCombo.setBounds (gl.removeFromTop (26).reduced (2, 0));
        gl.removeFromTop (4);
        allOnButton.setBounds (gl.removeFromTop (24).reduced (2, 0));
        auto gr = gateArea.removeFromRight (90);
        gr.removeFromTop (14);
        shuffleButton.setBounds (gr.removeFromTop (26).reduced (2, 0)); gr.removeFromTop (4);
        unshuffleButton.setBounds (gr.removeFromTop (26).reduced (2, 0));
        layoutKnobs (gateArea, { kGateDepth });
        auto outArea = group (rowC, rowC.getWidth(), "OUTPUT");
        normToggle.setBounds (outArea.removeFromTop (24));
        swapToggle.setBounds (outArea.removeFromTop (24));
        phaseToggle.setBounds (outArea.removeFromTop (24));
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
