// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "dsp/RenderEngine.h"

struct BatchOptions
{
    enum class Collision { skip, overwrite, autoNumber };
    enum class RateMode  { host, matchSource, fixed };
    enum class Normalize { off, peakMinus1dB, peakMinus01dB };

    juce::File outputFolder;
    juce::String pattern = "{name}_reverse";          // tokens: {name}, {index}
    Collision collision = Collision::autoNumber;
    int bitDepth = 24;                                // 16, 24 or 32 (float)
    bool dither = true;                               // TPDF, 16-bit only
    RateMode rateMode = RateMode::host;
    double fixedRate = 48000.0;
    Normalize normalize = Normalize::off;
};

// Immutable snapshot of everything needed to render: taken on the message thread, then used freely on any thread.
struct BatchJob
{
    RenderSettings settings;
    float dry = 1.0f, wet = 0.8f, outGainDb = 0.0f;
    bool limiter = true;
    juce::StringArray files;
    BatchOptions options;
};

struct BatchResult
{
    int ok = 0, skipped = 0, failed = 0;
    bool cancelled = false;
    juce::StringArray failures, outputs;
};

// Renders every file with the job's settings and writes WAVs. Touches no live plugin state.
// Files are written via a temporary file and renamed, so a cancel never leaves a partial file.
class BatchExporter : public juce::Thread
{
public:
    explicit BatchExporter (BatchJob jobToRun) : juce::Thread ("ReverseVerb batch export"), job (std::move (jobToRun)) {}
    ~BatchExporter() override { stopThread (10000); }

    BatchResult process();                            // runs on the calling thread; honours threadShouldExit()
    void run() override { auto r = process(); { const juce::ScopedLock sl (lock); result = std::move (r); } finished = true; }

    int numDone() const { return done.load(); }
    int numTotal() const { return job.files.size(); }
    bool isFinished() const { return finished.load(); }
    juce::String currentFile() const { const juce::ScopedLock sl (lock); return current; }
    BatchResult getResult() const { const juce::ScopedLock sl (lock); return result; }

    static juce::String sanitiseFileName (const juce::String&);

private:
    BatchJob job;
    mutable juce::CriticalSection lock;
    juce::String current;
    BatchResult result;
    std::atomic<int> done { 0 };
    std::atomic<bool> finished { false };
};
