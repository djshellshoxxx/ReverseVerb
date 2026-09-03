#include "FxEditor.h"

using namespace RVColours;

static const char* kFxHelp = R"(REVERSE VERB FX - what everything does

HOW IT WORKS
  Put it on a drum track or bus. Every hit that passes the trigger is captured, run through the reverb, reversed and played
  BEFORE that hit so a swell rises into it. To play something before it happens the plugin delays the whole track by the
  swell length and reports that as latency. Your DAW's plugin delay compensation (PDC) hides it, so everything stays in time.
  It cannot work on live input (mic, live playing): the dry sound is delayed by the swell length. Use it on recorded/sequenced audio.
  Changing LENGTH, DELAY, HIT LENGTH, PITCH or SYNC changes the latency; the DAW re-syncs, so tweak those while stopped if you hear a jump.

TRIGGER
  THRESHOLD: minimum input level to fire.  SENSITIVITY: how much louder than the recent average a hit must be (higher = fewer, harder hits).
  HOLD: minimum time between triggers (raise it for fast hat patterns you do not want swells on).
  HIT LENGTH: how much audio after the transient is captured and reverbed.  GATE HIT: stop capturing early when the hit decays.
  TRIGGER: Audio (transient detection), MIDI (note-ons from the piano roll only), or Both.
  MAX SWELLS: how many swells may overlap; oldest is cut when exceeded.  TRIGGER NOW: fire manually.
  The scope shows the input level (bright line), the threshold (dashed) and each trigger (red tick). Drag in the scope to set the threshold.

FREEZE / LISTEN / FOLLOW
  FREEZE: keeps the last rendered swell and reuses it for every trigger (consistent swell, no re-render).
  LISTEN: mutes the dry signal so you hear only swells.  FOLLOW LEVEL: swell volume follows the hit volume; off = every swell same level.
  BOOST: extra swell gain in dB.

WAVEFORM
  Shows the last captured hit (orange) and its swell. Click to audition. Drag to trim the swell (drag right = shorter, left = longer, left edge = trim start).
  Volume line: drag the dots and middle square. Red lines are beats when SYNC is on. Colour follows COLOR and BASS CUT.
  EXPORT WAV / DRAG TO DAW give you the last swell + hit as a sample.

REVERB / SWELL / PITCH / VOLUME
  Same as the instrument: SIZE, DECAY, DAMP, DIFFUSION, EARLY REF, SEPARATION, WIDTH, DELAY (gap before the hit),
  LENGTH, SHAPE, COLOR (low pass), BASS CUT (high pass), PITCH sweep with RANGE and CURVE, START/END/TENSION volume envelope.

STRETCH / FX / PAN / GATE / OUTPUT
  STRETCH time-stretches each captured hit into a long even body before the reverb (smooth rise).
  FX: Echo, Reverse Echo, Chorus, Reverse Chorus on the swell. PAN line (pink) on the waveform or the PAN knobs.
  GATE (SYNC on): click blocks under the waveform to mute beats, shape Hard/Smooth/LFO, SHUFFLE plays beats in random order. Sync up to 32 bars.
  BASS / TREBLE shelves, NORMALIZE, SWAP L/R, INVERT PHASE on the final swell. Mouse wheel over the waveform nudges volume. Right-click any knob for exact value / DAW automation.

PRESETS
  Factory presets are built in. SAVE stores your own in Documents/ReverseVerb/FX (the ... button opens that folder).

STATUS
  Top right shows current latency, how many hits were captured, how many swells could not be rendered in time (missed) or
  had no free slot (dropped). Missed usually means the swell is longer than the latency budget: raise HOLD or lower LENGTH.
)";

// ---------------- Trigger scope ----------------

void TriggerScope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    auto p = r.reduced (10.0f, 8.0f);
    proc.copyHistory (db, trig);
    const int n = (int) db.size();
    if (n < 2) return;
    auto yFor = [&] (float d) { return p.getBottom() - p.getHeight() * juce::jlimit (0.0f, 1.0f, (d + 60.0f) / 60.0f); };
    g.setColour (outline.withAlpha (0.5f));
    for (float d = -50.0f; d < 0.0f; d += 10.0f) g.drawHorizontalLine ((int) yFor (d), p.getX(), p.getRight());

    juce::Path line, fill;
    for (int i = 0; i < n; ++i)
    {
        const float x = p.getX() + p.getWidth() * (float) i / (float) (n - 1);
        const float y = yFor (db[(size_t) i]);
        if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, p.getBottom()); fill.lineTo (x, y); }
        else { line.lineTo (x, y); fill.lineTo (x, y); }
        if (trig[(size_t) i]) { g.setColour (alert.withAlpha (0.8f)); g.drawLine (x, p.getY(), x, p.getBottom(), 1.2f); }
    }
    fill.lineTo (p.getRight(), p.getBottom());
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (line, juce::PathStrokeType (1.2f));

    const float ty = yFor (proc.param (IDs::threshold));
    const float dash[] = { 4.0f, 4.0f };
    juce::Path tl; tl.startNewSubPath (p.getX(), ty); tl.lineTo (p.getRight(), ty);
    juce::Path td; juce::PathStrokeType (1.2f).createDashedStroke (td, tl, dash, 2);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillPath (td);

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.setColour (textDim);
    g.drawText ("TRIGGER SCOPE   in " + juce::String (proc.getStats().inputDb, 1) + " dB   thr " + juce::String (proc.param (IDs::threshold), 1) + " dB   (drag to set threshold)",
                p.toNearestInt().withHeight (12), juce::Justification::topLeft);
}

void TriggerScope::mouseDown (const juce::MouseEvent& e)
{
    auto p = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    const float f = 1.0f - juce::jlimit (0.0f, 1.0f, ((float) e.y - p.getY()) / p.getHeight());
    proc.setParam (IDs::threshold, -60.0f + 60.0f * f);
}

// ---------------- Editor ----------------

FxEditor::FxEditor (FxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (p.presets), waveform (p), shape (p), scope (p), dragPad (p),
      pitchTension (p, IDs::pitchTension), help (kFxHelp)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);
    waveform.emptyText = "Play audio through the plugin: the first hit that passes the trigger shows up here";

    title.setText ("REVERSE VERB FX", juce::dontSendNotification);
    styleLabel (title, 24.0f, true, text, juce::Justification::centredLeft);
    addAndMakeVisible (title);
    subtitle.setText ("reverse reverb swells on every hit, in real time", juce::dontSendNotification);
    styleLabel (subtitle, 12.0f, false, textDim, juce::Justification::centredLeft);
    addAndMakeVisible (subtitle);
    styleLabel (statusLabel, 11.0f, false, textDim, juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);
    styleLabel (led, 10.0f, true, bg);
    led.setText ("TRIG", juce::dontSendNotification);
    led.setColour (juce::Label::backgroundColourId, panel2);
    addAndMakeVisible (led);

    for (auto* b : { &auditionButton, &triggerButton, &exportButton, &resetButton, &randomButton, &helpButton, &shuffleButton, &unshuffleButton, &allOnButton }) addAndMakeVisible (b);
    for (auto* t : { &syncToggle, &gateToggle, &followToggle, &freezeToggle, &listenToggle, &normToggle, &swapToggle, &phaseToggle }) addAndMakeVisible (t);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (waveform);
    addAndMakeVisible (shape);
    addAndMakeVisible (scope);
    addAndMakeVisible (dragPad);
    addAndMakeVisible (pitchTension);

    syncCombo.addItemList (syncChoiceNames(), 1);
    rangeCombo.addItemList ({ "1 oct", "2 oct", "4 oct" }, 1);
    modeCombo.addItemList ({ "Audio", "MIDI", "Both" }, 1);
    fxCombo.addItemList ({ "Off", "Echo", "Reverse Echo", "Chorus", "Reverse Chorus" }, 1);
    gateCombo.addItemList ({ "Hard", "Smooth", "LFO" }, 1);
    for (auto* c : { &syncCombo, &rangeCombo, &modeCombo, &fxCombo, &gateCombo }) addAndMakeVisible (c);
    fxComboAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::fxType, fxCombo);
    gateComboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::gateShape, gateCombo);
    normAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::normalize, normToggle);
    swapAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::swapStereo, swapToggle);
    phaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::invertPhase, phaseToggle);
    fxLabel.setText ("TYPE", juce::dontSendNotification);    styleLabel (fxLabel, 10.0f, true, textDim);   addAndMakeVisible (fxLabel);
    gateLabel.setText ("SHAPE", juce::dontSendNotification); styleLabel (gateLabel, 10.0f, true, textDim); addAndMakeVisible (gateLabel);
    shuffleButton.onClick   = [this] { proc.shuffleBeats (false); };
    unshuffleButton.onClick = [this] { proc.shuffleBeats (true); };
    allOnButton.onClick     = [this] { proc.setAllBeats (true); };
    syncComboAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::syncLen, syncCombo);
    rangeComboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::pitchRange, rangeCombo);
    modeComboAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::trigMode, modeCombo);
    syncAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::sync, syncToggle);
    gateAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::gateHit, gateToggle);
    followAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::followLevel, followToggle);
    freezeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::freeze, freezeToggle);
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::listen, listenToggle);

    rangeLabel.setText ("RANGE", juce::dontSendNotification); styleLabel (rangeLabel, 10.0f, true, textDim); addAndMakeVisible (rangeLabel);
    modeLabel.setText ("TRIGGER", juce::dontSendNotification);  styleLabel (modeLabel, 10.0f, true, textDim);  addAndMakeVisible (modeLabel);

    auditionButton.onClick = [this] { proc.triggerPreview(); };
    triggerButton.onClick  = [this] { proc.manualTrigger(); };
    resetButton.onClick    = [this] { proc.resetEdits(); };
    randomButton.onClick   = [this] { proc.randomizeReverb(); };
    helpButton.onClick     = [this] { help.setVisible (true); help.toFront (true); };
    exportButton.onClick = [this]
    {
        auto def = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("reverseverb_swell.wav");
        chooser = std::make_unique<juce::FileChooser> ("Export last swell + hit", def, "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) proc.exportWav (f.withFileExtension ("wav")); });
    };

    kThr = &knob (IDs::threshold, "THRESHOLD", alert); kSens = &knob (IDs::sens, "SENSITIVITY", alert); kHold = &knob (IDs::hold, "HOLD", alert);
    kHitLen = &knob (IDs::hitLen, "HIT LENGTH", hitCol); kMaxV = &knob (IDs::maxVoices, "MAX SWELLS", alert);
    kSize = &knob (IDs::size, "SIZE");  kDecay = &knob (IDs::decay, "DECAY"); kDamp = &knob (IDs::damp, "DAMP"); kDiff = &knob (IDs::diff, "DIFFUSION");
    kEr = &knob (IDs::er, "EARLY REF"); kSep = &knob (IDs::sep, "SEPARATION"); kWidth = &knob (IDs::width, "WIDTH"); kGap = &knob (IDs::gap, "DELAY");
    kTail = &knob (IDs::tail, "LENGTH"); kShape = &knob (IDs::shape, "SHAPE"); kTone = &knob (IDs::tone, "COLOR"); kBass = &knob (IDs::basscut, "BASS CUT"); kStretch = &knob (IDs::stretch, "STRETCH");
    kDry = &knob (IDs::dry, "DRY", hitCol); kWet = &knob (IDs::wet, "SWELL"); kBoost = &knob (IDs::swellGain, "BOOST");
    kBassB = &knob (IDs::bassBoost, "BASS"); kTrebB = &knob (IDs::trebleBoost, "TREBLE");
    kFxTime = &knob (IDs::fxTime, "TIME / RATE"); kFxFb = &knob (IDs::fxFeedback, "FEEDBACK"); kFxDepth = &knob (IDs::fxDepth, "DEPTH"); kFxMix = &knob (IDs::fxMix, "MIX");
    kPanStart = &knob (IDs::panStart, "START", panCol); kPanEnd = &knob (IDs::panEnd, "END", panCol); kPanTension = &knob (IDs::panTension, "TENSION", panCol);
    kGateDepth = &knob (IDs::gateDepth, "DEPTH", alert);
    kPitch = &knob (IDs::pitch, "PITCH");
    kVolStart = &knob (IDs::volStart, "START"); kVolEnd = &knob (IDs::volEnd, "END"); kVolTension = &knob (IDs::volTension, "TENSION");

    addChildComponent (help);
    setSize (1140, 1010);
    startTimerHz (15);
    timerCallback();
}

FxEditor::~FxEditor() { setLookAndFeel (nullptr); }

Knob& FxEditor::knob (const juce::String& id, const juce::String& textName, juce::Colour fill)
{
    knobs.push_back (makeKnob (*this, proc.apvts, id, textName, fill));
    return *knobs.back();
}

void FxEditor::timerCallback()
{
    auto st = proc.getStats();
    const double sr = 44100.0;
    juce::String s = "latency " + juce::String (st.latency / juce::jmax (1.0, proc.getSampleRate() > 0 ? proc.getSampleRate() : sr), 2) + " s";
    s += "   captured " + juce::String (st.captured) + "   active " + juce::String (st.active);
    if (st.missed > 0) s += "   missed " + juce::String (st.missed);
    if (st.dropped > 0) s += "   dropped " + juce::String (st.dropped);
    statusLabel.setText (s, juce::dontSendNotification);
    const bool hot = st.lastTrigger >= 0 && (st.clock - st.lastTrigger) < (juce::int64) (proc.getSampleRate() * 0.12);
    led.setColour (juce::Label::backgroundColourId, hot ? alert : panel2);
    led.setColour (juce::Label::textColourId, hot ? bg : textDim);

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
    for (auto* k : { kTone, kBass, kWet, kTail, kShape, kBoost, kStretch })
        if (k->slider.findColour (juce::Slider::rotarySliderFillColourId) != col) { k->slider.setColour (juce::Slider::rotarySliderFillColourId, col); k->slider.repaint(); }
}

void FxEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient grad (bg.brighter (0.07f), 0.0f, 0.0f, bg, 0.0f, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillAll();
    const juce::Colour col = swellColour (proc.param (IDs::tone), proc.param (IDs::basscut));
    g.setColour (col.withAlpha (0.07f));
    g.fillEllipse (-140.0f, -180.0f, 480.0f, 360.0f);
    g.setColour (alert.withAlpha (0.04f));
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

void FxEditor::resized()
{
    help.setBounds (getLocalBounds());
    groups.clear();
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (46);
    auto titleArea = header.removeFromLeft (260);
    title.setBounds (titleArea.removeFromTop (28));
    subtitle.setBounds (titleArea);
    helpButton.setBounds (header.removeFromRight (34).reduced (0, 7));
    header.removeFromRight (8);
    led.setBounds (header.removeFromRight (44).reduced (0, 10));
    header.removeFromRight (8);
    statusLabel.setBounds (header);

    area.removeFromTop (6);
    presetBar.setBounds (area.removeFromTop (26));

    area.removeFromTop (10);
    auto vis = area.removeFromTop (250);
    shape.setBounds (vis.removeFromLeft (220));
    vis.removeFromLeft (10);
    waveform.setBounds (vis);

    area.removeFromTop (8);
    scope.setBounds (area.removeFromTop (72));

    area.removeFromTop (10);
    auto row = area.removeFromTop (34);
    auditionButton.setBounds (row.removeFromLeft (90));  row.removeFromLeft (6);
    triggerButton.setBounds (row.removeFromLeft (100));  row.removeFromLeft (6);
    exportButton.setBounds (row.removeFromLeft (100));   row.removeFromLeft (6);
    dragPad.setBounds (row.removeFromLeft (120));        row.removeFromLeft (6);
    resetButton.setBounds (row.removeFromLeft (100));    row.removeFromLeft (6);
    randomButton.setBounds (row.removeFromLeft (80));    row.removeFromLeft (14);
    freezeToggle.setBounds (row.removeFromLeft (80));
    listenToggle.setBounds (row.removeFromLeft (80));
    syncCombo.setBounds (row.removeFromRight (100));     row.removeFromRight (6);
    syncToggle.setBounds (row.removeFromRight (70));

    area.removeFromTop (12);
    const int rowH = (area.getHeight() - 30) / 4;
    auto rowT = area.removeFromTop (rowH); area.removeFromTop (10);
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

    {
        auto t = group (rowT, rowT.getWidth(), "TRIGGER");
        auto right = t.removeFromRight (260);
        auto c1 = right.removeFromLeft (120);
        modeLabel.setBounds (c1.removeFromTop (14));
        modeCombo.setBounds (c1.removeFromTop (26).reduced (2, 0));
        c1.removeFromTop (6);
        gateToggle.setBounds (c1.removeFromTop (22));
        followToggle.setBounds (c1.removeFromTop (22));
        right.removeFromLeft (10);
        layoutKnobs (right, { kMaxV });
        layoutKnobs (t, { kThr, kSens, kHold, kHitLen });
    }
    layoutKnobs (group (rowA, rowA.getWidth(), "REVERB"), { kSize, kDecay, kDamp, kDiff, kEr, kSep, kWidth, kGap });

    {
        const int unit = (rowB.getWidth() - 8 * 3) / 15;
        layoutKnobs (group (rowB, unit * 5, "SWELL"), { kTail, kShape, kTone, kBass, kStretch });
        layoutKnobs (group (rowB, unit * 5, "MIX  +  TONE"), { kDry, kWet, kBoost, kBassB, kTrebB });
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
