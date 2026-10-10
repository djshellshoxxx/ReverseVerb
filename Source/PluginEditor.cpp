// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "PluginEditor.h"

using namespace RVColours;

// ---------------- Editor ----------------

RVContent::RVContent (ReverseVerbProcessor& p)
    : proc (p), waveform (p), shape (p), dragPad (p), pitchTension (p, IDs::pitchTension), meter (p), presetBar (p)
{
    setLookAndFeel (&lnf);
    applyTooltipSetting();

    title.setText ("REVERSE VERB", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    addAndMakeVisible (title);
    subtitle.setText ("reverse reverb swell for hits", juce::dontSendNotification);
    subtitle.setFont (juce::Font (juce::FontOptions (12.0f)));
    subtitle.setColour (juce::Label::textColourId, textDim);

    fileLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    fileLabel.setJustificationType (juce::Justification::centred);
    fileLabel.setColour (juce::Label::backgroundColourId, panel);
    fileLabel.setColour (juce::Label::outlineColourId, outline);
    addAndMakeVisible (fileLabel);
    countLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    countLabel.setJustificationType (juce::Justification::centred);
    countLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (countLabel);

    for (auto* b : { &prevButton, &nextButton, &loadButton, &playButton, &exportButton, &resetButton, &randomButton, &optionsButton, &helpButton })
        addAndMakeVisible (b);
    for (auto* t : { &alignToggle, &syncToggle, &keytrackToggle, &limiterToggle }) addAndMakeVisible (t);
    addAndMakeVisible (meter);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (rootCombo);
    addAndMakeVisible (waveform);
    addAndMakeVisible (shape);
    addAndMakeVisible (dragPad);
    addAndMakeVisible (pitchTension);

    syncCombo.addItemList ({ "1 beat", "2 beats", "4 beats", "8 beats", "1 bar", "2 bars", "4 bars" }, 1);
    rangeCombo.addItemList ({ "1 oct", "2 oct", "4 oct" }, 1);
    addAndMakeVisible (syncCombo);
    addAndMakeVisible (rangeCombo);
    syncComboAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::syncLen, syncCombo);
    rangeComboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::pitchRange, rangeCombo);
    for (int n = 0; n < 128; ++n) rootCombo.addItem (juce::MidiMessage::getMidiNoteName (n, true, true, 4), n + 1);
    rootComboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::rootNote, rootCombo);
    keytrackAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::keytrack, keytrackToggle);
    limiterAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::limiter, limiterToggle);
    alignAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::align, alignToggle);
    syncAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::sync, syncToggle);

    rangeLabel.setText ("RANGE", juce::dontSendNotification);
    rangeLabel.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    rangeLabel.setJustificationType (juce::Justification::centred);
    rangeLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (rangeLabel);

    prevButton.onClick   = [this] { proc.prevSample(); };
    nextButton.onClick   = [this] { proc.nextSample(); };
    playButton.onClick   = [this] { proc.triggerPreview(); };
    resetButton.onClick  = [this] { proc.resetEdits(); };
    randomButton.onClick = [this] { proc.randomizeReverb(); };
    optionsButton.onClick = [this] { showOptionsMenu(); };
    helpButton.onClick   = [this] { help.setVisible (true); help.toFront (true); };

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
    kDry = &makeKnob (IDs::dry, "HIT");          kWet = &makeKnob (IDs::wet, "SWELL");
    kPitch = &makeKnob (IDs::pitch, "PITCH");
    kOut = &makeKnob (IDs::outGain, "OUT");
    kOut->slider.getProperties().set ("unipolar", true);
    kVolStart = &makeKnob (IDs::volStart, "START"); kVolEnd = &makeKnob (IDs::volEnd, "END"); kVolTension = &makeKnob (IDs::volTension, "TENSION");
    kDry->slider.setColour (juce::Slider::rotarySliderFillColourId, hitCol);

    prevButton.setTooltip ("Load the previous supported audio file in the current folder.");
    nextButton.setTooltip ("Load the next supported audio file in the current folder.");
    loadButton.setTooltip ("Load an audio sample. WAV, AIFF, FLAC, MP3 and OGG are supported.");
    playButton.setTooltip ("Preview the current reverse-reverb result.");
    exportButton.setTooltip ("Export the current processed result as a WAV file.");
    resetButton.setTooltip ("Reset editable sound controls to their defaults.");
    randomButton.setTooltip ("Randomize the reverse-reverb sound-design controls.");
    optionsButton.setTooltip ("Open interface options, including the global tooltip switch.");
    helpButton.setTooltip ("Open the complete ReverseVerb help guide.");
    alignToggle.setTooltip ("Align the hit to the host timeline using plugin delay compensation.");
    syncToggle.setTooltip ("Use host tempo divisions for reverse-swell length.");
    syncCombo.setTooltip ("Choose the host-synced reverse-swell duration.");
    rangeCombo.setTooltip ("Choose the available pitch-bend range.");
    waveform.setTooltip ("Edit sample trim and volume-envelope points directly on the waveform.");
    dragPad.setTooltip ("Drag the rendered result out to a DAW or file destination.");
    keytrackToggle.setTooltip ("Play the swell chromatically: notes above or below the root note change its pitch and length.");
    rootCombo.setTooltip ("Root note: the MIDI note that plays the sound at its original pitch (C4 = MIDI 60).");
    limiterToggle.setTooltip ("Safety soft limiter on the output. Keeps peaks below 0 dBFS. Also applied to exported WAVs.");
    meter.setTooltip ("Output peak meter. The light latches when the output reaches the ceiling; click to clear.");
    pitchTension.setTooltip ("Adjust pitch-envelope tension. Double-click to reset.");


    addChildComponent (help);
    sendLookAndFeelChange();       // rebuild slider value boxes with the custom look-and-feel (consistent text boxes)
    setSize (1060, 720);
    startTimerHz (10);
    timerCallback();
}

RVContent::~RVContent() { setLookAndFeel (nullptr); }

RVContent::Knob& RVContent::makeKnob (const juce::String& id, const juce::String& textName)
{
    auto k = std::make_unique<Knob>();
    auto& s = k->slider;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setTooltip ("Adjust " + textName + ". Double-click the value to type where supported.");
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

void RVContent::timerCallback()
{
    auto f = proc.getCurrentFile();
    fileLabel.setText (f.existsAsFile() ? f.getFileName() : "no sample loaded", juce::dontSendNotification);
    const int n = proc.getSampleCount();
    countLabel.setText (n > 0 ? juce::String (proc.getSampleIndex() + 1) + " / " + juce::String (n) : "", juce::dontSendNotification);
    const bool sync = proc.param (IDs::sync) > 0.5f;
    kTail->slider.setEnabled (! sync);
    kTail->slider.setAlpha (sync ? 0.4f : 1.0f);
    syncCombo.setEnabled (sync);
    syncCombo.setAlpha (sync ? 1.0f : 0.5f);
    const juce::Colour col = swellColour (proc.param (IDs::tone), proc.param (IDs::basscut));
    for (auto* k : { kTone, kBass, kWet, kTail, kShape })
        if (k->slider.findColour (juce::Slider::rotarySliderFillColourId) != col) { k->slider.setColour (juce::Slider::rotarySliderFillColourId, col); k->slider.repaint(); }
}

void RVContent::applyTooltipSetting()
{
    if (tooltipsEnabled)
    {
        if (tooltipWindow == nullptr)
            tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 650);
    }
    else
    {
        tooltipWindow.reset();
    }
}

void RVContent::showOptionsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Interface");
    menu.addItem (1, "Show tooltips", true, tooltipsEnabled);
    menu.addSectionHeader ("Window size");
    const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f };
    for (int i = 0; i < 4; ++i)
        menu.addItem (10 + i, juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", true,
                      std::abs ((float) getWidth() / 1060.0f - scales[i]) < 0.02f);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsButton),
                        [this] (int result)
                        {
                            if (result == 1)
                            {
                                tooltipsEnabled = ! tooltipsEnabled;
                                applyTooltipSetting();
                            }
                            else if (result >= 10 && result <= 13 && onUiScale)
                            {
                                const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f };
                                onUiScale (scales[result - 10]);
                            }
                        });
}

void RVContent::paint (juce::Graphics& g)
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

void RVContent::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> ks)
{
    const int kw = area.getWidth() / (int) ks.size();
    for (auto* k : ks)
    {
        auto cell = area.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (14));
        k->slider.setBounds (cell);
    }
}

void RVContent::resized()
{
    help.setBounds (getLocalBounds());
    groups.clear();
    auto area = getLocalBounds().reduced (16);

    // header
    auto header = area.removeFromTop (54);
    auto titleArea = header.removeFromLeft (310);
    title.setBounds (titleArea.removeFromTop (28));
    presetBar.setBounds (titleArea.reduced (0, 2));
    helpButton.setBounds (header.removeFromRight (34).reduced (0, 11));
    header.removeFromRight (4);
    optionsButton.setBounds (header.removeFromRight (76).reduced (0, 11));
    header.removeFromRight (10);
    limiterToggle.setBounds (header.removeFromRight (62).reduced (0, 16));
    meter.setBounds (header.removeFromRight (74).reduced (0, 13));
    header.removeFromRight (10);
    auto browser = header.withTrimmedLeft (20);
    loadButton.setBounds (browser.removeFromRight (80).reduced (0, 11));
    browser.removeFromRight (8);
    nextButton.setBounds (browser.removeFromRight (40).reduced (0, 11));
    browser.removeFromRight (4);
    prevButton.setBounds (browser.removeFromRight (40).reduced (0, 11));
    browser.removeFromRight (8);
    countLabel.setBounds (browser.removeFromRight (56));
    fileLabel.setBounds (browser.reduced (0, 11));

    // shape + waveform
    area.removeFromTop (10);
    auto vis = area.removeFromTop (228);
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
    randomButton.setBounds (row.removeFromLeft (80));   row.removeFromLeft (14);
    alignToggle.setBounds (row.removeFromLeft (150));   row.removeFromLeft (6);
    keytrackToggle.setBounds (row.removeFromLeft (92)); row.removeFromLeft (4);
    rootCombo.setBounds (row.removeFromLeft (70).reduced (0, 3)); row.removeFromLeft (10);
    syncCombo.setBounds (row.removeFromRight (100));    row.removeFromRight (6);
    syncToggle.setBounds (row.removeFromRight (70));

    // knob rows
    area.removeFromTop (12);
    const int rowH = (area.getHeight() - 10) / 2;
    auto rowA = area.removeFromTop (rowH);
    area.removeFromTop (10);
    auto rowB = area;

    auto group = [&] (juce::Rectangle<int>& src, int width, const juce::String& name)
    {
        auto r = src.removeFromLeft (width);
        src.removeFromLeft (8);
        groups.push_back ({ name, r });
        return r.reduced (6).withTrimmedTop (14);
    };

    const int wA = rowA.getWidth();
    layoutKnobs (group (rowA, wA, "REVERB"), { kSize, kDecay, kDamp, kDiff, kEr, kSep, kWidth, kGap });

    const int total = rowB.getWidth() - 8 * 3;
    const int unit = total / 11;
    layoutKnobs (group (rowB, (int) ((float) unit * 3.5f), "SWELL"), { kTail, kShape, kTone, kBass });
    layoutKnobs (group (rowB, (int) ((float) unit * 2.7f), "MIX"), { kDry, kWet, kOut });
    auto pitchArea = group (rowB, unit * 2 + 30, "PITCH");
    {
        auto right = pitchArea.removeFromRight (74);
        rangeLabel.setBounds (right.removeFromTop (14));
        rangeCombo.setBounds (right.removeFromTop (26).reduced (2, 0));
        right.removeFromTop (6);
        pitchTension.setBounds (right.withSizeKeepingCentre (64, juce::jmin (64, right.getHeight())));
        layoutKnobs (pitchArea, { kPitch });
    }
    layoutKnobs (group (rowB, rowB.getWidth(), "VOLUME  (also drag the dots on the waveform)"), { kVolStart, kVolEnd, kVolTension });
}

bool RVContent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void RVContent::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (proc.loadSampleFile (juce::File (f), true)) return;
}

// ---------------- Resizable editor ----------------

ReverseVerbEditor::ReverseVerbEditor (ReverseVerbProcessor& p)
    : AudioProcessorEditor (&p), proc (p), content (p)
{
    content.onUiScale = [this] (float s) { setSize (juce::roundToInt ((float) kBaseW * s), juce::roundToInt ((float) kBaseH * s)); };
    addAndMakeVisible (content);

    constrainer.setSizeLimits (kMinW, kMinW * kBaseH / kBaseW, kMaxW, kMaxW * kBaseH / kBaseW);
    constrainer.setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setConstrainer (&constrainer);
    setResizable (true, true);

    const int w = juce::jlimit (kMinW, kMaxW, proc.getUiWidth());
    setSize (w, juce::roundToInt ((float) w * (float) kBaseH / (float) kBaseW));
}

void ReverseVerbEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseW;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kBaseW, kBaseH);
    proc.setUiSize (getWidth(), getHeight());
}
