// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "RVLookAndFeel.h"

// "Export entire folder" dialog: pick options, then render every sample in the current sample's folder
// with the current settings on a background thread (progress + cancel + summary).
class ExportDialog : public juce::Component, private juce::Timer
{
public:
    explicit ExportDialog (ReverseVerbProcessor&);
    ~ExportDialog() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static void show (ReverseVerbProcessor&, juce::Component* centreAround);

private:
    void timerCallback() override;
    void chooseFolder();
    void refreshSummary();
    juce::StringArray listFiles() const;
    BatchOptions readOptions() const;
    void start();
    void finish();
    void setRunning (bool);

    ReverseVerbProcessor& proc;
    RVLookAndFeel lnf;
    juce::File outputFolder;

    juce::Label folderLabel, folderTitle, patternTitle, collisionTitle, depthTitle, rateTitle, normTitle, summary, status;
    juce::TextButton folderButton { "Choose..." }, startButton { "EXPORT" }, closeButton { "CLOSE" }, revealButton { "SHOW FILES" };
    juce::TextEditor patternEdit;
    juce::ComboBox collisionBox, depthBox, rateBox, normBox;
    juce::ToggleButton ditherToggle { "Dither (16-bit)" }, subfolderToggle { "Include subfolders" };
    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };

    std::unique_ptr<BatchExporter> exporter;
    std::unique_ptr<juce::FileChooser> chooser;
    bool running = false;
    juce::File lastOutput;
};
