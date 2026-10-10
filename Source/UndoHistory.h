// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>

// Undo / redo for parameter edits: a history of full parameter snapshots (normalised values).
//  - Only USER edits are recorded: changes made inside a parameter gesture (knob drag, waveform handle,
//    setParam) or inside a group (randomize, reset, preset load). Host automation playback is ignored.
//  - A drag is one step: the change is committed once the gesture has ended and the value has been idle.
//  - Listener callbacks may arrive on any thread and only touch atomics; everything else is locked.
class UndoHistory : private juce::AudioProcessorParameter::Listener, private juce::Timer
{
public:
    explicit UndoHistory (juce::AudioProcessor&, int maxSteps = 100);
    ~UndoHistory() override;

    void beginGroup();                  // nest-safe: everything changed until the matching endGroup is ONE step
    void endGroup();

    struct Group
    {
        explicit Group (UndoHistory& h) : history (h) { history.beginGroup(); }
        ~Group() { history.endGroup(); }
        UndoHistory& history;
    };

    bool canUndo() const;
    bool canRedo() const;
    void undo();                        // message thread
    void redo();
    void clear();                       // new baseline (e.g. after a project load)
    void commitNow();                   // commit any pending edit immediately (also used by tests)
    int numUndoSteps() const;

private:
    using Snapshot = std::vector<float>;
    Snapshot capture() const;
    void apply (const Snapshot&);
    bool differs (const Snapshot&, const Snapshot&) const;
    void commitLocked();

    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool starting) override;
    void timerCallback() override;

    juce::AudioProcessor& processor;
    const int maxSteps;
    mutable juce::CriticalSection lock;
    std::vector<Snapshot> history;      // history[cursor] is the current committed state
    int cursor = 0;

    std::atomic<int> gestures { 0 }, groups { 0 };
    std::atomic<bool> pending { false }, restoring { false };
    std::atomic<juce::uint32> lastChangeMs { 0 };
};
