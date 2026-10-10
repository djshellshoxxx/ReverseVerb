// ReverseVerb™ preset system tests. Copyright © 2026 Sheldon Davidson. All rights reserved.
#include "TestHelpers.h"
#include "../Source/PresetManager.h"

using namespace rvtest;

struct PresetTest : public juce::UnitTest
{
    PresetTest() : juce::UnitTest ("Presets", "ReverseVerb") {}

    static float real (ReverseVerbProcessor& p, const juce::String& id)
    {
        auto* prm = p.apvts.getParameter (id);
        return prm->convertFrom0to1 (prm->getValue());
    }

    static juce::File freshFolder (const juce::String& name)
    {
        auto d = tempDir().getChildFile (name);
        d.deleteRecursively(); d.createDirectory();
        return d;
    }

    void runTest() override
    {
        beginTest ("name sanitising");
        {
            juce::String out, err;
            expect (PresetManager::sanitizeName ("  Big Hall  ", out, err) && out == "Big Hall");
            expect (PresetManager::sanitizeName ("a/b\\c:d*e?f\"g<h>i|j", out, err) && out == "abcdefghij", "illegal characters removed");
            expect (! PresetManager::sanitizeName ("", out, err), "empty rejected");
            expect (! PresetManager::sanitizeName ("   ", out, err), "blank rejected");
            expect (! PresetManager::sanitizeName (juce::String::repeatedString ("x", 65), out, err), "too long rejected");
            expect (PresetManager::sanitizeName (juce::String::repeatedString ("x", 64), out, err), "64 ok");
            expect (! PresetManager::sanitizeName ("con", out, err) && ! PresetManager::sanitizeName ("LPT1", out, err), "reserved names rejected");
            expect (! PresetManager::sanitizeName ("..", out, err), "dot-dot rejected");
            expect (PresetManager::sanitizeName ("name. ", out, err) && out == "name", "trailing dot/space trimmed");
        }

        beginTest ("save, load round trip; trim excluded; dirty tracking");
        {
            ReverseVerbProcessor p; auto folder = freshFolder ("pm1");
            PresetManager pm (p.apvts, folder);
            setReal (p, "size", 0.33f); setReal (p, "tail", 3.5f); setReal (p, "tone", 4321.0f); setReal (p, "pitch", -0.4f);
            setReal (p, "keytrack", 1.0f); setReal (p, "rootNote", 48.0f); setReal (p, "trimStart", 0.2f);
            juce::String err;
            expect (pm.save ("Test A", "Unit", false, &err), "save: " + err);
            expect (folder.getChildFile ("Unit").getChildFile ("Test A.rvpreset").existsAsFile(), "file in category folder");
            expect (! pm.isDirty(), "clean right after save");

            setReal (p, "size", 0.9f); setReal (p, "tail", 1.0f); setReal (p, "tone", 20000.0f); setReal (p, "pitch", 0.0f);
            setReal (p, "keytrack", 0.0f); setReal (p, "rootNote", 60.0f); setReal (p, "trimStart", 0.7f);
            expect (pm.isDirty(), "dirty after a change");

            const int idx = pm.indexOf ("Test A", false);
            expect (idx >= 0, "listed");
            expect (pm.load (idx, &err), "load: " + err);
            expectWithinAbsoluteError (real (p, "size"), 0.33f, 1e-3f);
            expectWithinAbsoluteError (real (p, "tail"), 3.5f, 1e-2f);
            expectWithinAbsoluteError (real (p, "tone"), 4321.0f, 2.0f);
            expectWithinAbsoluteError (real (p, "pitch"), -0.4f, 1e-3f);
            expect (real (p, "keytrack") > 0.5f, "keytrack restored");
            expectEquals ((int) std::round (real (p, "rootNote")), 48);
            expectWithinAbsoluteError (real (p, "trimStart"), 0.7f, 1e-3f);   // trim is not part of presets
            expect (! pm.isDirty(), "clean after load");
            expectEquals (pm.currentName(), juce::String ("Test A"));
            setReal (p, "damp", 0.9f);
            expect (pm.isDirty(), "dirty again");
        }

        beginTest ("duplicate names, overwrite, delete");
        {
            ReverseVerbProcessor p; auto folder = freshFolder ("pm2");
            PresetManager pm (p.apvts, folder);
            juce::String err;
            expect (pm.save ("Dup", "", false, &err));
            expect (! pm.save ("Dup", "", false, &err) && err.contains ("already exists"), "duplicate refused");
            expect (pm.save ("Dup", "", true, &err), "overwrite allowed");
            expect (pm.save ("bad:name?", "", false, &err), "illegal characters are stripped, not refused");
            expect (folder.getChildFile ("badname.rvpreset").existsAsFile(), "saved as sanitised name");
            expect (! pm.save ("", "", false, &err), "empty name refused");
            expect (pm.save ("ok", "bad/cat", false, &err), "category is sanitised too");
            expect (folder.getChildFile ("badcat").getChildFile ("ok.rvpreset").existsAsFile(), "no path traversal via category");
            const int i = pm.indexOf ("Dup", false);
            expect (pm.remove (i, &err), "delete user preset");
            expect (pm.indexOf ("Dup", false) < 0, "gone from list");
            expect (! folder.getChildFile ("Dup.rvpreset").existsAsFile(), "file deleted");
            expect (! pm.remove (pm.indexOf ("Default", true), &err), "factory presets cannot be deleted");
        }

        beginTest ("corrupt, foreign and out-of-range files are handled safely");
        {
            ReverseVerbProcessor p; auto folder = freshFolder ("pm3");
            folder.getChildFile ("garbage.rvpreset").replaceWithText ("this is not xml <<<");
            folder.getChildFile ("foreign.rvpreset").replaceWithText ("<Other><P id=\"size\" v=\"0.1\"/></Other>");
            folder.getChildFile ("future.rvpreset").replaceWithText ("<ReverseVerbPreset version=\"99\" name=\"f\"/>");
            folder.getChildFile ("range.rvpreset").replaceWithText (
                "<ReverseVerbPreset version=\"1\" name=\"range\"><P id=\"size\" v=\"99\"/><P id=\"decay\" v=\"-5\"/>"
                "<P id=\"tail\" v=\"nan\"/><P id=\"doesNotExist\" v=\"1\"/><P id=\"trimStart\" v=\"0.9\"/></ReverseVerbPreset>");
            PresetManager pm (p.apvts, folder);
            setReal (p, "size", 0.5f); setReal (p, "trimStart", 0.25f);
            juce::String err;
            expect (! pm.load (pm.indexOf ("garbage", false), &err) && err.isNotEmpty(), "garbage refused");
            expect (! pm.load (pm.indexOf ("foreign", false), &err) && err.isNotEmpty(), "foreign refused");
            expect (! pm.load (pm.indexOf ("future", false), &err) && err.contains ("newer"), "newer version refused");
            expectWithinAbsoluteError (real (p, "size"), 0.5f, 1e-4f);               // nothing changed by failures
            expect (pm.load (pm.indexOf ("range", false), &err), "range file loads: " + err);
            expectEquals (real (p, "size"), 1.0f);                                    // clamped to range
            expectEquals (real (p, "decay"), 0.0f);
            expectWithinAbsoluteError (real (p, "tail"), 1.2f, 1e-3f);                // NaN ignored -> default
            expectWithinAbsoluteError (real (p, "trimStart"), 0.25f, 1e-4f);          // excluded
        }

        beginTest ("factory presets: all load, all render sanely");
        {
            ReverseVerbProcessor p; auto folder = freshFolder ("pm4");
            PresetManager pm (p.apvts, folder);
            int factory = 0; juce::StringArray names;
            for (auto& r : pm.list()) if (r.factory) { ++factory; expect (! names.contains (r.name), "unique name " + r.name); names.add (r.name); }
            expect (factory >= 12, "factory presets present: " + juce::String (factory));
            p.prepareToPlay (44100.0, 256);
            p.loadSampleFile (makeBurst ("pm4", 44100.0, 0.2, 2));
            for (int i = 0; i < (int) pm.list().size(); ++i)
            {
                juce::String err;
                expect (pm.load (i, &err), "load " + pm.list()[(size_t) i].name + ": " + err);
                auto r = settle (p);
                auto s = statsOf (*r);
                expect (s.finite && s.len > 0 && s.peak <= 1.01, "render ok: " + pm.list()[(size_t) i].name);
                expect (! pm.isDirty(), "clean: " + pm.list()[(size_t) i].name);
            }
        }

        beginTest ("restoreName re-attaches dirty tracking after a project load");
        {
            ReverseVerbProcessor p; auto folder = freshFolder ("pm5");
            PresetManager pm (p.apvts, folder);
            juce::String err;
            expect (pm.load (pm.indexOf ("Big Hall Rise", true), &err), err);
            PresetManager pm2 (p.apvts, folder);                    // "new session" with the same parameter values
            pm2.restoreName ("Big Hall Rise");
            expectEquals (pm2.currentName(), juce::String ("Big Hall Rise"));
            expect (! pm2.isDirty(), "clean");
            setReal (p, "size", 0.1f);
            expect (pm2.isDirty(), "dirty");
            pm2.restoreName ("No Such Preset");
            expect (pm2.currentName().isEmpty(), "unknown name clears");
        }

        beginTest ("A/B compare");
        {
            ReverseVerbProcessor p; auto folder = freshFolder ("pm6");
            PresetManager pm (p.apvts, folder);
            setReal (p, "size", 0.2f);
            pm.toggleAB();                                  // now B (copy of A)
            expectWithinAbsoluteError (real (p, "size"), 0.2f, 1e-4f);
            setReal (p, "size", 0.8f);
            pm.toggleAB();                                  // back to A
            expectWithinAbsoluteError (real (p, "size"), 0.2f, 1e-4f);
            pm.toggleAB();                                  // B again
            expectWithinAbsoluteError (real (p, "size"), 0.8f, 1e-4f);
        }

        beginTest ("listing 120 user presets is fast");
        {
            ReverseVerbProcessor p; auto folder = freshFolder ("pm7");
            PresetManager pm (p.apvts, folder);
            for (int i = 0; i < 120; ++i) { juce::String err; pm.save ("P" + juce::String (i), i % 3 == 0 ? "A" : "", false, &err); }
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            pm.refresh();
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            int user = 0; for (auto& r : pm.list()) if (! r.factory) ++user;
            expectEquals (user, 120);
            expect (ms < 500.0, "refresh took " + juce::String (ms) + " ms");
        }
    }
};
static PresetTest presetTest;
