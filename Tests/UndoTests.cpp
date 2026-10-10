// ReverseVerb™ undo/redo tests. Copyright © 2026 Sheldon Davidson. All rights reserved.
#include "TestHelpers.h"

using namespace rvtest;

struct UndoTest : public juce::UnitTest
{
    UndoTest() : juce::UnitTest ("Undo", "ReverseVerb") {}

    static float norm (ReverseVerbProcessor& p, const juce::String& id) { return p.apvts.getParameter (id)->getValue(); }

    // simulates a knob drag: gesture + many value changes
    static void drag (ReverseVerbProcessor& p, const juce::String& id, float from, float to, int moves = 100)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->beginChangeGesture();
        for (int i = 0; i <= moves; ++i) prm->setValueNotifyingHost (from + (to - from) * (float) i / (float) moves);
        prm->endChangeGesture();
    }

    void runTest() override
    {
        beginTest ("a drag is one undo step; undo/redo restore exact values");
        {
            ReverseVerbProcessor p; auto& u = p.getUndo();
            const float start = norm (p, "size");
            expect (! u.canUndo() && ! u.canRedo(), "empty history");
            drag (p, "size", start, 0.2f);
            u.commitNow();
            expectEquals (u.numUndoSteps(), 1);
            expect (u.canUndo() && ! u.canRedo());
            drag (p, "size", 0.2f, 0.9f);
            u.commitNow();
            expectEquals (u.numUndoSteps(), 2);

            u.undo();
            expectWithinAbsoluteError (norm (p, "size"), 0.2f, 1e-6f);
            expect (u.canRedo(), "can redo");
            expectEquals (u.numUndoSteps(), 1);                       // undo does not add history
            u.undo();
            expectWithinAbsoluteError (norm (p, "size"), start, 1e-6f);
            expect (! u.canUndo(), "at the beginning");
            u.redo(); u.redo();
            expectWithinAbsoluteError (norm (p, "size"), 0.9f, 1e-6f);
            expect (! u.canRedo(), "at the end");
        }

        beginTest ("a group is one step");
        {
            ReverseVerbProcessor p; auto& u = p.getUndo();
            const float a = norm (p, "size"), b = norm (p, "decay"), c = norm (p, "damp");
            p.randomizeReverb();                                      // many parameters, one group
            expectEquals (u.numUndoSteps(), 1);
            u.undo();
            expectWithinAbsoluteError (norm (p, "size"), a, 1e-6f);
            expectWithinAbsoluteError (norm (p, "decay"), b, 1e-6f);
            expectWithinAbsoluteError (norm (p, "damp"), c, 1e-6f);
            u.redo();
            expectEquals (u.numUndoSteps(), 1);
        }

        beginTest ("host automation (no gesture) is not recorded");
        {
            ReverseVerbProcessor p; auto& u = p.getUndo();
            p.apvts.getParameter ("size")->setValue (0.123f);         // what a host does during playback
            p.apvts.getParameter ("damp")->setValue (0.456f);
            u.commitNow();
            expectEquals (u.numUndoSteps(), 0);
        }

        beginTest ("a new edit after undo discards the redo branch");
        {
            ReverseVerbProcessor p; auto& u = p.getUndo();
            drag (p, "size", 0.5f, 0.1f); u.commitNow();
            drag (p, "size", 0.1f, 0.8f); u.commitNow();
            u.undo();
            expect (u.canRedo());
            drag (p, "decay", norm (p, "decay"), 0.3f); u.commitNow();
            expect (! u.canRedo(), "redo branch gone");
            expectEquals (u.numUndoSteps(), 2);
        }

        beginTest ("history is capped at 100 steps");
        {
            ReverseVerbProcessor p; auto& u = p.getUndo();
            for (int i = 0; i < 130; ++i) { drag (p, "size", norm (p, "size"), (i % 2 == 0) ? 0.1f + 0.003f * (float) i : 0.9f - 0.003f * (float) i, 2); u.commitNow(); }
            expectEquals (u.numUndoSteps(), 100);
            int undone = 0; while (u.canUndo() && undone < 500) { u.undo(); ++undone; }
            expectEquals (undone, 100);
        }

        beginTest ("clear starts a new baseline; presets load as one step");
        {
            ReverseVerbProcessor p; auto& u = p.getUndo();
            drag (p, "size", 0.5f, 0.2f); u.commitNow();
            u.clear();
            expect (! u.canUndo() && ! u.canRedo(), "cleared");
            const float before = norm (p, "tail");
            juce::String err;
            auto& pm = p.getPresets();
            expect (pm.load (pm.indexOf ("Infinite Wash", true), &err), err);
            expectEquals (u.numUndoSteps(), 1);                       // whole preset = one step
            u.undo();
            expectWithinAbsoluteError (norm (p, "tail"), before, 1e-6f);
        }

        beginTest ("undo with an edit still pending commits it first");
        {
            ReverseVerbProcessor p; auto& u = p.getUndo();
            const float start = norm (p, "size");
            drag (p, "size", start, 0.77f);                           // not committed yet (debounce)
            expect (u.canUndo(), "pending edit is undoable");
            u.undo();
            expectWithinAbsoluteError (norm (p, "size"), start, 1e-6f);
        }
    }
};
static UndoTest undoTest;
