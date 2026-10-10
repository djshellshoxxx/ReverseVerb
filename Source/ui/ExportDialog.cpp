// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "ExportDialog.h"

using namespace RVColours;

ExportDialog::ExportDialog (ReverseVerbProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    outputFolder = proc.getCurrentFile().getParentDirectory().getChildFile ("ReverseVerb Export");

    auto title = [this] (juce::Label& l, const juce::String& t) { l.setText (t, juce::dontSendNotification); l.setColour (juce::Label::textColourId, textDim); l.setFont (juce::Font (juce::FontOptions (12.0f))); addAndMakeVisible (l); };
    title (folderTitle, "Output folder"); title (patternTitle, "File name"); title (collisionTitle, "If a file exists");
    title (depthTitle, "Bit depth"); title (rateTitle, "Sample rate"); title (normTitle, "Normalize");

    folderLabel.setColour (juce::Label::backgroundColourId, panel2);
    folderLabel.setColour (juce::Label::outlineColourId, outline);
    folderLabel.setMinimumHorizontalScale (0.5f);
    addAndMakeVisible (folderLabel);
    addAndMakeVisible (folderButton);
    folderButton.onClick = [this] { chooseFolder(); };

    patternEdit.setText ("{name}_reverse", false);
    patternEdit.setTooltip ("File name pattern. {name} = the sample's name, {index} = its position in the batch.");
    addAndMakeVisible (patternEdit);

    collisionBox.addItemList ({ "Add a number (name_2)", "Skip it", "Overwrite it" }, 1); collisionBox.setSelectedId (1);
    depthBox.addItemList ({ "16-bit", "24-bit", "32-bit float" }, 1); depthBox.setSelectedId (2);
    rateBox.addItemList ({ "Same as host", "Same as each sample", "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz" }, 1); rateBox.setSelectedId (1);
    normBox.addItemList ({ "Off", "Peak at -1 dBFS", "Peak at -0.1 dBFS" }, 1); normBox.setSelectedId (1);
    for (auto* c : { &collisionBox, &depthBox, &rateBox, &normBox }) addAndMakeVisible (c);
    ditherToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (ditherToggle); addAndMakeVisible (subfolderToggle);
    subfolderToggle.onClick = [this] { refreshSummary(); };

    summary.setColour (juce::Label::textColourId, text); addAndMakeVisible (summary);
    status.setColour (juce::Label::textColourId, textDim); status.setFont (juce::Font (juce::FontOptions (12.0f))); addAndMakeVisible (status);
    addAndMakeVisible (progressBar);
    addAndMakeVisible (startButton); addAndMakeVisible (closeButton); addAndMakeVisible (revealButton);
    revealButton.setVisible (false);
    startButton.onClick = [this] { if (running) { if (exporter != nullptr) exporter->signalThreadShouldExit(); status.setText ("Cancelling...", juce::dontSendNotification); } else start(); };
    closeButton.onClick = [this] { if (auto* w = findParentComponentOfClass<juce::DialogWindow>()) w->exitModalState (0); };
    revealButton.onClick = [this] { if (lastOutput.exists()) lastOutput.revealToUser(); else outputFolder.revealToUser(); };

    refreshSummary();
    setSize (500, 470);
}

ExportDialog::~ExportDialog()
{
    stopTimer();
    exporter.reset();                      // stops (cancels) a running export
    setLookAndFeel (nullptr);
}

void ExportDialog::show (ReverseVerbProcessor& p, juce::Component* centreAround)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new ExportDialog (p));
    o.dialogTitle = "Export entire folder";
    o.dialogBackgroundColour = panel;
    o.componentToCentreAround = centreAround;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void ExportDialog::paint (juce::Graphics& g) { g.fillAll (panel); }

void ExportDialog::resized()
{
    auto r = getLocalBounds().reduced (18, 14);
    const int rowH = 28, gap = 8, labelW = 120;
    auto row = [&] { auto a = r.removeFromTop (rowH); r.removeFromTop (gap); return a; };

    auto a = row(); folderTitle.setBounds (a.removeFromLeft (labelW)); folderButton.setBounds (a.removeFromRight (90)); a.removeFromRight (6); folderLabel.setBounds (a);
    a = row(); patternTitle.setBounds (a.removeFromLeft (labelW)); patternEdit.setBounds (a);
    a = row(); collisionTitle.setBounds (a.removeFromLeft (labelW)); collisionBox.setBounds (a);
    a = row(); depthTitle.setBounds (a.removeFromLeft (labelW)); depthBox.setBounds (a.removeFromLeft (170)); a.removeFromLeft (14); ditherToggle.setBounds (a);
    a = row(); rateTitle.setBounds (a.removeFromLeft (labelW)); rateBox.setBounds (a);
    a = row(); normTitle.setBounds (a.removeFromLeft (labelW)); normBox.setBounds (a);
    a = row(); subfolderToggle.setBounds (a.withTrimmedLeft (labelW));
    summary.setBounds (row());
    progressBar.setBounds (row());
    status.setBounds (r.removeFromTop (rowH * 2)); r.removeFromTop (gap);
    auto btns = r.removeFromBottom (32);
    startButton.setBounds (btns.removeFromLeft (120)); btns.removeFromLeft (8);
    closeButton.setBounds (btns.removeFromLeft (100)); btns.removeFromLeft (8);
    revealButton.setBounds (btns.removeFromLeft (120));
}

void ExportDialog::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the output folder", outputFolder.getParentDirectory(), "*");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != juce::File()) { outputFolder = f; refreshSummary(); }
                          });
}

juce::StringArray ExportDialog::listFiles() const
{
    juce::StringArray out;
    const auto dir = proc.getCurrentFile().getParentDirectory();
    if (! dir.isDirectory()) return out;
    auto files = dir.findChildFiles (juce::File::findFiles, subfolderToggle.getToggleState(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    files.sort();
    for (auto& f : files)
        if (! f.isAChildOf (outputFolder)) out.add (f.getFullPathName());          // never re-export our own output
    return out;
}

void ExportDialog::refreshSummary()
{
    folderLabel.setText (" " + outputFolder.getFullPathName(), juce::dontSendNotification);
    const int n = listFiles().size();
    summary.setText (n == 0 ? "No audio files found in the sample's folder."
                            : juce::String (n) + (n == 1 ? " file" : " files") + " from \"" + proc.getCurrentFile().getParentDirectory().getFileName() + "\" will be exported with the current settings.",
                     juce::dontSendNotification);
    startButton.setEnabled (! running && n > 0);
}

BatchOptions ExportDialog::readOptions() const
{
    BatchOptions o;
    o.outputFolder = outputFolder;
    o.pattern = patternEdit.getText();
    o.collision = collisionBox.getSelectedId() == 2 ? BatchOptions::Collision::skip
                : collisionBox.getSelectedId() == 3 ? BatchOptions::Collision::overwrite : BatchOptions::Collision::autoNumber;
    o.bitDepth = depthBox.getSelectedId() == 1 ? 16 : depthBox.getSelectedId() == 3 ? 32 : 24;
    o.dither = ditherToggle.getToggleState();
    const int rate = rateBox.getSelectedId();
    o.rateMode = rate == 1 ? BatchOptions::RateMode::host : rate == 2 ? BatchOptions::RateMode::matchSource : BatchOptions::RateMode::fixed;
    o.fixedRate = rate == 3 ? 44100.0 : rate == 4 ? 48000.0 : rate == 5 ? 88200.0 : 96000.0;
    o.normalize = normBox.getSelectedId() == 2 ? BatchOptions::Normalize::peakMinus1dB
                : normBox.getSelectedId() == 3 ? BatchOptions::Normalize::peakMinus01dB : BatchOptions::Normalize::off;
    return o;
}

void ExportDialog::setRunning (bool r)
{
    running = r;
    for (auto* c : std::initializer_list<juce::Component*> { &folderButton, &patternEdit, &collisionBox, &depthBox, &rateBox, &normBox, &ditherToggle, &subfolderToggle })
        c->setEnabled (! r);
    startButton.setButtonText (r ? "CANCEL" : "EXPORT");
    startButton.setEnabled (r || ! listFiles().isEmpty());
}

void ExportDialog::start()
{
    const auto files = listFiles();
    if (files.isEmpty()) return;
    revealButton.setVisible (false);
    exporter = std::make_unique<BatchExporter> (proc.makeBatchJob (files, readOptions()));   // snapshot: later edits don't affect this run
    exporter->startThread();
    setRunning (true);
    startTimerHz (10);
}

void ExportDialog::timerCallback()
{
    if (exporter == nullptr) { stopTimer(); return; }
    const int total = exporter->numTotal(), done = exporter->numDone();
    progressValue = total > 0 ? (double) done / (double) total : 0.0;
    if (exporter->isFinished()) { finish(); return; }
    status.setText ("Rendering " + juce::String (juce::jmin (done + 1, total)) + " of " + juce::String (total) + ":  " + exporter->currentFile(), juce::dontSendNotification);
}

void ExportDialog::finish()
{
    stopTimer();
    exporter->stopThread (5000);
    const auto r = exporter->getResult();
    progressValue = 1.0;
    juce::String msg = juce::String (r.ok) + " exported";
    if (r.skipped > 0) msg << ", " << r.skipped << " skipped";
    if (r.failed > 0)  msg << ", " << r.failed << " failed";
    if (r.cancelled)   msg << "  (cancelled)";
    if (r.failures.size() > 0) msg << "\n" << r.failures.joinIntoString ("; ").substring (0, 220);
    status.setText (msg, juce::dontSendNotification);
    lastOutput = r.outputs.size() > 0 ? juce::File (r.outputs[0]).getParentDirectory() : outputFolder;
    revealButton.setVisible (true);
    exporter.reset();
    setRunning (false);
    refreshSummary();
}
