// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "UndoHistory.h"

UndoHistory::UndoHistory (juce::AudioProcessor& p, int steps) : processor (p), maxSteps (steps)
{
    history.push_back (capture());
    for (auto* prm : processor.getParameters()) prm->addListener (this);
    startTimerHz (20);
}

UndoHistory::~UndoHistory()
{
    stopTimer();
    for (auto* prm : processor.getParameters()) prm->removeListener (this);
}

UndoHistory::Snapshot UndoHistory::capture() const
{
    Snapshot s;
    for (auto* prm : processor.getParameters()) s.push_back (prm->getValue());
    return s;
}

bool UndoHistory::differs (const Snapshot& a, const Snapshot& b) const
{
    if (a.size() != b.size()) return true;
    for (size_t i = 0; i < a.size(); ++i) if (std::abs (a[i] - b[i]) > 1.0e-5f) return true;
    return false;
}

void UndoHistory::parameterValueChanged (int, float)
{
    if (restoring.load()) return;
    if (gestures.load() > 0 || groups.load() > 0)          // a user edit (host automation has neither)
    {
        pending = true;
        lastChangeMs = juce::Time::getMillisecondCounter();
    }
}

void UndoHistory::parameterGestureChanged (int, bool starting)
{
    if (restoring.load()) return;
    if (starting) ++gestures;
    else          { if (gestures.load() > 0) --gestures; lastChangeMs = juce::Time::getMillisecondCounter(); }
}

void UndoHistory::beginGroup() { ++groups; }

void UndoHistory::endGroup()
{
    if (groups.load() > 0) --groups;
    if (groups.load() == 0 && pending.load()) commitNow();
}

void UndoHistory::timerCallback()
{
    if (pending.load() && gestures.load() == 0 && groups.load() == 0
        && juce::Time::getMillisecondCounter() - lastChangeMs.load() >= 250)
        commitNow();
}

void UndoHistory::commitNow()
{
    const juce::ScopedLock sl (lock);
    if (pending.load()) commitLocked();                      // only user edits create steps; automation alone never does
}

void UndoHistory::commitLocked()
{
    pending = false;
    auto now = capture();
    if (! differs (now, history[(size_t) cursor])) return;
    history.resize ((size_t) cursor + 1);                   // a new edit discards the redo branch
    history.push_back (std::move (now));
    ++cursor;
    while ((int) history.size() > maxSteps + 1) { history.erase (history.begin()); --cursor; }
}

bool UndoHistory::canUndo() const { const juce::ScopedLock sl (lock); return cursor > 0 || pending.load(); }
bool UndoHistory::canRedo() const { const juce::ScopedLock sl (lock); return cursor + 1 < (int) history.size(); }
int UndoHistory::numUndoSteps() const { const juce::ScopedLock sl (lock); return cursor; }

void UndoHistory::apply (const Snapshot& s)
{
    restoring = true;
    int i = 0;
    for (auto* prm : processor.getParameters())
    {
        if (i < (int) s.size() && std::abs (prm->getValue() - s[(size_t) i]) > 1.0e-6f)
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (s[(size_t) i]);
            prm->endChangeGesture();
        }
        ++i;
    }
    restoring = false;
    gestures = 0;
    pending = false;
}

void UndoHistory::undo()
{
    const juce::ScopedLock sl (lock);
    if (pending.load()) commitLocked();                      // include the edit in progress as its own step
    if (cursor <= 0) return;
    --cursor;
    apply (history[(size_t) cursor]);
}

void UndoHistory::redo()
{
    const juce::ScopedLock sl (lock);
    if (cursor + 1 >= (int) history.size()) return;
    ++cursor;
    apply (history[(size_t) cursor]);
}

void UndoHistory::clear()
{
    const juce::ScopedLock sl (lock);
    history.assign (1, capture());
    cursor = 0;
    pending = false;
}
