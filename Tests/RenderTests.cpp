// ReverseVerb™ render tests: golden regression + parameter fuzz + audio-thread smoke.
// Copyright © 2026 Sheldon Davidson. All rights reserved.
#include "TestHelpers.h"

using namespace rvtest;

namespace
{
    struct Scenario { const char* name; double hostSR, srcSR, srcSeconds; int srcCh; std::vector<std::pair<const char*, float>> params; };

    std::vector<Scenario> scenarios()
    {
        return {
            { "default_44k",    44100, 44100, 0.20, 2, {} },
            { "resample_48k",   48000, 44100, 0.15, 1, { { "size", 0.4f }, { "decay", 0.9f } } },
            { "resample_96k",   96000, 48000, 0.10, 2, { { "tail", 2.0f }, { "gap", 120.0f } } },
            { "sync_4bars",     44100, 44100, 0.20, 2, { { "sync", 1.0f }, { "syncLen", 6.0f } } },
            { "pitch_vol_trim", 44100, 44100, 0.20, 2, { { "pitch", 0.6f }, { "pitchRange", 2.0f }, { "pitchTension", 0.5f },
                                                          { "volStart", 0.2f }, { "volEnd", 0.9f }, { "volTension", 0.4f },
                                                          { "trimStart", 0.1f }, { "trimEnd", 0.8f } } },
            { "trim_start_one", 44100, 44100, 0.20, 2, { { "trimStart", 1.0f } } },
            { "filters_shape",  44100, 44100, 0.20, 2, { { "tone", 3000.0f }, { "basscut", 400.0f }, { "shape", 0.5f } } },
        };
    }

    std::map<juce::String, Stats> runScenarios (juce::UnitTest& ut)
    {
        std::map<juce::String, Stats> out;
        for (auto& sc : scenarios())
        {
            ReverseVerbProcessor p;
            p.prepareToPlay (sc.hostSR, 512);
            auto f = makeBurst (sc.name, sc.srcSR, sc.srcSeconds, sc.srcCh);
            ut.expect (p.loadSampleFile (f), juce::String ("load ") + sc.name);
            for (auto& kv : sc.params) setReal (p, kv.first, kv.second);
            auto r = settle (p);
            ut.expect (r != nullptr, juce::String ("rendered ") + sc.name);
            if (r != nullptr) out[sc.name] = statsOf (*r);
        }
        return out;
    }
}

struct GoldenTest : public juce::UnitTest
{
    GoldenTest() : juce::UnitTest ("Golden render", "ReverseVerb") {}

    void runTest() override
    {
        const juce::File golden (juce::String (RV_TEST_DATA_DIR) + "/golden.txt");
        beginTest ("scenarios match golden");
        auto got = runScenarios (*this);

        if (juce::SystemStats::getEnvironmentVariable ("RV_WRITE_GOLDEN", "").isNotEmpty())
        {
            juce::String txt;
            for (auto& kv : got)
                txt << kv.first << " " << kv.second.len << " " << kv.second.hit << " "
                    << juce::String (kv.second.rms, 9) << " " << juce::String (kv.second.peak, 9) << " "
                    << juce::String (kv.second.sumAbs, 9) << " " << juce::String (kv.second.sumW, 9) << "\n";
            golden.replaceWithText (txt);
            logMessage ("golden written: " + golden.getFullPathName());
            return;
        }

        const double tol = juce::SystemStats::getEnvironmentVariable ("RV_GOLDEN_TOL", "1e-3").getDoubleValue();
        juce::StringArray lines; golden.readLines (lines);
        expect (lines.size() > 0, "golden.txt present");
        for (auto& line : lines)
        {
            auto t = juce::StringArray::fromTokens (line, " ", "");
            if (t.size() < 7) continue;
            auto it = got.find (t[0]);
            expect (it != got.end(), "scenario exists: " + t[0]);
            if (it == got.end()) continue;
            auto& s = it->second;
            auto close = [tol] (double a, double b) { return std::abs (a - b) <= tol * juce::jmax (1.0, std::abs (b)); };
            expect (s.finite, t[0] + " finite");
            expectEquals (s.len, t[1].getIntValue(), t[0] + " length");
            expectEquals (s.hit, t[2].getIntValue(), t[0] + " hitIndex");
            expect (close (s.rms, t[3].getDoubleValue()), t[0] + " rms");
            expect (close (s.peak, t[4].getDoubleValue()), t[0] + " peak");
            expect (close (s.sumAbs, t[5].getDoubleValue()), t[0] + " sumAbs");
            expect (close (s.sumW, t[6].getDoubleValue()), t[0] + " sumW");
        }
    }
};
static GoldenTest goldenTest;

struct FuzzTest : public juce::UnitTest
{
    FuzzTest() : juce::UnitTest ("Parameter fuzz", "ReverseVerb") {}

    void runTest() override
    {
        const int iterations = juce::SystemStats::getEnvironmentVariable ("RV_FUZZ_N", "120").getIntValue();
        const double srs[] = { 22050, 44100, 48000, 88200, 96000 };
        juce::Random rng (12345);
        beginTest ("random parameters keep invariants");

        for (int it = 0; it < iterations; ++it)
        {
            const double hostSR = srs[rng.nextInt (5)], srcSR = srs[rng.nextInt (5)];
            ReverseVerbProcessor p;
            p.prepareToPlay (hostSR, 64 << rng.nextInt (4));
            auto f = makeBurst ("fuzz", srcSR, 0.005 + rng.nextDouble() * 1.2, 1 + rng.nextInt (2), it);
            if (! p.loadSampleFile (f)) { expect (false, "fuzz load"); continue; }

            for (auto* prm : p.getParameters())
            {
                switch (rng.nextInt (4))
                {
                    case 0: prm->setValueNotifyingHost (0.0f); break;
                    case 1: prm->setValueNotifyingHost (1.0f); break;
                    case 2: break;                                   // default
                    default: prm->setValueNotifyingHost (rng.nextFloat()); break;
                }
            }
            auto r = settle (p);
            const juce::String tag = "iter " + juce::String (it);
            expect (r != nullptr, tag + " rendered");
            if (r == nullptr) continue;
            auto s = statsOf (*r);
            expect (s.finite, tag + " finite");
            expect (s.len >= 1, tag + " non-empty");
            expect (s.peak <= 1.01, tag + " peak " + juce::String (s.peak));
            expect (s.hit == -1 || (s.hit >= 0 && s.hit < s.len), tag + " hit index range");
            expect (r->trimEndSec > r->trimStartSec, tag + " trim order");
            expect (r->gainLin.size() == r->pitchSemi.size(), tag + " env sizes");

            // audio thread smoke: notes through processBlock must stay finite
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60 + rng.nextInt (24), (juce::uint8) (1 + rng.nextInt (127))), rng.nextInt (256));
            bool ok = true;
            for (int b = 0; b < 40; ++b)
            {
                p.processBlock (buf, midi); midi.clear();
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 256; ++i)
                        if (! std::isfinite (buf.getSample (c, i)) || std::abs (buf.getSample (c, i)) > 8.0f) ok = false;
            }
            expect (ok, tag + " processBlock output sane");
        }
    }
};
static FuzzTest fuzzTest;

struct PlaybackTest : public juce::UnitTest
{
    PlaybackTest() : juce::UnitTest ("Playback", "ReverseVerb") {}

    void runTest() override
    {
        beginTest ("note produces sound and ends");
        ReverseVerbProcessor p;
        p.prepareToPlay (44100.0, 256);
        p.loadSampleFile (makeBurst ("play", 44100.0, 0.2, 2));
        auto r = settle (p);
        expect (r != nullptr && r->audio.getNumSamples() > 0, "rendered");
        const int total = r->audio.getNumSamples();

        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi; midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
        double energy = 0; int blocks = total / 256 + 8;
        for (int b = 0; b < blocks; ++b) { p.processBlock (buf, midi); midi.clear(); energy += buf.getMagnitude (0, 256); }
        expect (energy > 0.1, "non-silent output");
        p.processBlock (buf, midi);
        expectEquals ((double) buf.getMagnitude (0, 256), 0.0, "silent after sample ends");
    }
};
static PlaybackTest playbackTest;
