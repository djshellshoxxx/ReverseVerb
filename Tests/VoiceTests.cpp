// ReverseVerb™ voice engine / output stage / playback-feature tests.
// Copyright © 2026 Sheldon Davidson. All rights reserved.
#include "TestHelpers.h"
#include "../Source/Voices.h"
#include "../Source/OutputStage.h"

using namespace rvtest;

namespace
{
    std::shared_ptr<RenderedSample> makeSample (int n, int hit, std::function<float (int)> f, double sr = 48000.0)
    {
        auto r = std::make_shared<RenderedSample>();
        r->audio.setSize (2, n);
        for (int i = 0; i < n; ++i) { r->audio.setSample (0, i, f (i)); r->audio.setSample (1, i, f (i)); }
        r->hitIndex = hit; r->sampleRate = sr;
        return r;
    }

    // renders until the bank reports no active voice (or maxSamples); returns channel 0
    std::vector<float> renderAll (VoiceBank& bank, const RenderedSample& r, int maxSamples, int block)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, block);
        while ((int) out.size() < maxSamples)
        {
            buf.clear();
            bank.render (buf, r, 0, block);
            for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
            if (bank.newestPosition() < 0) break;
        }
        return out;
    }
}

struct VoiceBankTest : public juce::UnitTest
{
    VoiceBankTest() : juce::UnitTest ("Voice bank", "ReverseVerb") {}

    void runTest() override
    {
        beginTest ("rate 1 is bit-identical to the plain reference loop (odd block sizes)");
        {
            juce::Random rng (7);
            auto s = makeSample (5000, 3000, [&] (int) { return rng.nextFloat() * 2.0f - 1.0f; });
            VoiceBank bank; bank.prepare (48000.0); bank.setGains (0.8f, 0.5f); bank.start (0.7f);
            std::vector<float> got;
            const int blocks[] = { 100, 333, 1, 777, 256, 257, 3500 };
            juce::AudioBuffer<float> buf (2, 4000);
            for (int b : blocks)
            {
                buf.clear(); bank.render (buf, *s, 0, b);
                for (int i = 0; i < b; ++i) got.push_back (buf.getSample (0, i));
            }
            bool same = true;
            for (int i = 0; i < (int) got.size() && i < 5000; ++i)
            {
                const float ref = s->audio.getSample (0, i) * (i < 3000 ? 0.5f : 0.8f) * 0.7f;
                if (got[(size_t) i] != ref) { same = false; break; }
            }
            expect (same, "output equals reference");
            expect (bank.newestPosition() < 0, "voice ended");
        }

        beginTest ("buffer swap crossfades without a jump");
        {
            auto a = makeSample (20000, -1, [] (int) { return 0.2f; });
            auto b = makeSample (20000, -1, [] (int) { return 0.8f; });
            VoiceBank bank; bank.prepare (48000.0); bank.setGains (1.0f, 1.0f); bank.start (1.0f);
            juce::AudioBuffer<float> buf (2, 1000);
            buf.clear(); bank.render (buf, *a, 0, 1000);
            expectWithinAbsoluteError (buf.getSample (0, 999), 0.2f, 1e-6f);
            bank.onBufferSwap (a, *b);
            std::vector<float> y;
            for (int k = 0; k < 4; ++k) { buf.clear(); bank.render (buf, *b, 0, 250); for (int i = 0; i < 250; ++i) y.push_back (buf.getSample (0, i)); }
            const int fadeLen = juce::jmax (32, (int) (0.01 * 48000.0));
            float maxStep = 0, prev = 0.2f; bool monotonic = true, inRange = true;
            for (float v : y) { maxStep = juce::jmax (maxStep, std::abs (v - prev)); if (v < prev - 1e-6f) monotonic = false; if (v < 0.2f - 1e-5f || v > 0.8f + 1e-5f) inRange = false; prev = v; }
            expect (monotonic, "monotonic ramp");
            expect (inRange, "no overshoot");
            expect (maxStep <= 0.6f / (float) fadeLen * 1.6f, "max step " + juce::String (maxStep));
            expectWithinAbsoluteError (y.back(), 0.8f, 1e-5f);     // 1000 samples > 480 fade length
        }

        beginTest ("keytrack: octave up plays twice as fast and ends in half the time");
        {
            const double f = 441.0, sr = 44100.0;
            auto s = makeSample (44100, -1, [&] (int i) { return std::sin (juce::MathConstants<float>::twoPi * (float) (f * i / sr)); }, sr);
            VoiceBank b1; b1.prepare (sr); b1.setGains (1, 1); b1.start (1.0f, 1.0);
            VoiceBank b2; b2.prepare (sr); b2.setGains (1, 1); b2.start (1.0f, VoiceBank::rateForNote (72, 60));
            auto y1 = renderAll (b1, *s, 60000, 256), y2 = renderAll (b2, *s, 60000, 256);
            expectWithinAbsoluteError ((double) y2.size() / (double) y1.size(), 0.5, 0.02);
            auto crossings = [] (const std::vector<float>& y, int n) { int c = 0; for (int i = 1; i < n; ++i) if ((y[(size_t) i - 1] < 0) != (y[(size_t) i] < 0)) ++c; return c; };
            const int c1 = crossings (y1, 2000), c2 = crossings (y2, 2000);
            expect (std::abs (c2 - 2 * c1) <= 3, "zero crossings " + juce::String (c1) + " vs " + juce::String (c2));
            expectEquals (VoiceBank::rateForNote (60, 60), 1.0);
            expectEquals (VoiceBank::rateForNote (127, 0), 4.0);     // clamped
            expectEquals (VoiceBank::rateForNote (0, 127), 0.25);
        }

        beginTest ("PDC alignment keeps the dry hit on the note");
        {
            auto vs = VoiceBank::alignStart (1000, 1.0);
            expectEquals (vs.delay, 0); expectEquals (vs.startPos, 0.0);
            vs = VoiceBank::alignStart (1000, 2.0);                  // hit arrives at 500, must land at 1000 -> wait 500
            expectEquals (vs.delay, 500); expectEquals (vs.startPos, 0.0);
            vs = VoiceBank::alignStart (1000, 0.5);                  // hit would arrive at 2000 -> skip ahead
            expectEquals (vs.delay, 0); expectWithinAbsoluteError (vs.startPos, 500.0, 1e-9);
            // end-to-end: impulse at hitIndex must appear at output sample == latency (1000)
            for (double rate : { 2.0, 0.5, 1.5, 0.75 })
            {
                auto s = makeSample (4000, 1000, [] (int i) { return i == 1000 ? 1.0f : 0.0f; });
                VoiceBank bank; bank.prepare (48000.0); bank.setGains (1, 1);
                bank.start (1.0f, rate, VoiceBank::alignStart (1000, rate));
                auto y = renderAll (bank, *s, 6000, 100);
                int peakAt = (int) (std::max_element (y.begin(), y.end(), [] (float p, float q) { return std::abs (p) < std::abs (q); }) - y.begin());
                expect (std::abs (peakAt - 1000) <= 2, "rate " + juce::String (rate) + " hit at " + juce::String (peakAt));
            }
        }
    }
};
static VoiceBankTest voiceBankTest;

struct OutputStageTest : public juce::UnitTest
{
    OutputStageTest() : juce::UnitTest ("Output stage", "ReverseVerb") {}

    void runTest() override
    {
        beginTest ("soft clip: transparent, bounded, continuous, monotonic");
        float prev = -2.0f; bool mono = true, bounded = true;
        for (float x = -4.0f; x <= 4.0f; x += 0.001f)
        {
            const float y = softClip (x);
            if (y < prev - 1e-6f) mono = false;
            if (std::abs (y) > 0.99f) bounded = false;
            if (std::abs (x) <= 0.89f && y != x) expect (false, "not transparent at " + juce::String (x));
            prev = y;
        }
        expect (mono, "monotonic"); expect (bounded, "bounded to 0.99");
        expectWithinAbsoluteError (softClip (0.89f + 1e-4f), 0.89f + 1e-4f, 1e-6f);
        const float slope = (softClip (0.891f) - softClip (0.890f)) / 0.001f;
        expectWithinAbsoluteError (slope, 1.0f, 0.02f);

        beginTest ("gain and meter");
        OutputStage st; st.prepare (48000.0);
        juce::AudioBuffer<float> buf (2, 512);
        for (int i = 0; i < 512; ++i) { buf.setSample (0, i, 0.25f); buf.setSample (1, i, 0.25f); }
        st.process (buf, 0.0f, false);                                // unity: untouched
        expectEquals (buf.getSample (0, 100), 0.25f);
        expectWithinAbsoluteError (st.takePeak (0), 0.25f, 1e-6f);
        expectEquals (st.takePeak (0), 0.0f);                         // reading resets
        for (int k = 0; k < 20; ++k) { for (int i = 0; i < 512; ++i) buf.setSample (0, i, 0.25f); st.process (buf, 6.0206f, false); }
        expectWithinAbsoluteError (buf.getSample (0, 511), 0.5f, 1e-3f);   // +6 dB after smoothing settles
        for (int i = 0; i < 512; ++i) buf.setSample (0, i, 3.0f);
        st.process (buf, 0.0f, true);
        expect (buf.getMagnitude (0, 0, 512) <= 0.99f, "limited");
        expect (st.clipLatched(), "clip latched"); st.clearClip(); expect (! st.clipLatched(), "clip cleared");
    }
};
static OutputStageTest outputStageTest;

struct ProcessorFeatureTest : public juce::UnitTest
{
    ProcessorFeatureTest() : juce::UnitTest ("Processor features", "ReverseVerb") {}

    static int activeSamples (ReverseVerbProcessor& p, int note, int maxBlocks)
    {
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi; midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
        int active = 0;   // fixed window: the swell ends in a pre-delay gap that can span a whole block
        for (int b = 0; b < maxBlocks; ++b)
        {
            p.processBlock (buf, midi); midi.clear();
            for (int i = 0; i < 256; ++i) if (std::abs (buf.getSample (0, i)) > 1e-7f) ++active;
        }
        return active;
    }

    void runTest() override
    {
        beginTest ("new parameters default to the old behaviour");
        {
            ReverseVerbProcessor p;
            expect (p.param (IDs::keytrack) < 0.5f, "keytrack off");
            expectEquals ((int) p.param (IDs::rootNote), 60);
            expectEquals (p.param (IDs::outGain), 0.0f);
            expect (p.param (IDs::limiter) > 0.5f, "limiter on");
        }

        beginTest ("project load renders immediately (no message loop)");
        {
            ReverseVerbProcessor a;
            a.prepareToPlay (44100.0, 256);
            a.loadSampleFile (makeBurst ("state", 44100.0, 0.2, 2));
            setReal (a, "size", 0.33f); setReal (a, "tail", 1.5f);
            auto ra = settle (a);
            juce::MemoryBlock state; a.getStateInformation (state);

            ReverseVerbProcessor b;
            b.prepareToPlay (44100.0, 256);
            b.setStateInformation (state.getData(), (int) state.getSize());
            auto rb = b.getRendered();
            expect (rb != nullptr && rb->audio.getNumSamples() > 0, "rendered on load");
            if (rb != nullptr) expectEquals (rb->audio.getNumSamples(), ra->audio.getNumSamples());
        }

        beginTest ("editor size is saved and restored with the project");
        {
            ReverseVerbProcessor a; a.setUiSize (1325, 900);
            juce::MemoryBlock state; a.getStateInformation (state);
            ReverseVerbProcessor b;
            expectEquals (b.getUiWidth(), 1060);
            b.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (b.getUiWidth(), 1325); expectEquals (b.getUiHeight(), 900);
        }

        beginTest ("keytrack: octave up shortens the sound by half; off is unchanged");
        {
            ReverseVerbProcessor p;
            p.prepareToPlay (44100.0, 256);
            p.loadSampleFile (makeBurst ("kt", 44100.0, 0.2, 2));
            p.renderBlocking();
            const int base = activeSamples (p, 60, 400);
            setNorm (p, IDs::keytrack, 1.0f);
            const int same = activeSamples (p, 60, 400);
            const int up = activeSamples (p, 72, 400);
            expect (std::abs (same - base) <= 2, "root note unchanged");
            expectWithinAbsoluteError ((double) up / (double) base, 0.5, 0.06);
        }

        beginTest ("offline rendering is deterministic");
        {
            auto run = [&]
            {
                ReverseVerbProcessor p;
                p.setNonRealtime (true);
                p.prepareToPlay (44100.0, 256);
                p.loadSampleFile (makeBurst ("off", 44100.0, 0.2, 2));
                p.renderBlocking();
                std::vector<float> out;
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi; midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                for (int b = 0; b < 120; ++b)
                {
                    if (b == 20) setReal (p, "size", 0.2f);          // parameter change mid-note
                    p.processBlock (buf, midi); midi.clear();
                    for (int i = 0; i < 256; ++i) out.push_back (buf.getSample (0, i));
                }
                return out;
            };
            auto x = run(), y = run();
            expect (x == y, "two offline runs are bit-identical");
            double e = 0; for (float v : x) e += std::abs (v);
            expect (e > 1.0, "non-silent");
        }
    }
};
static ProcessorFeatureTest processorFeatureTest;
