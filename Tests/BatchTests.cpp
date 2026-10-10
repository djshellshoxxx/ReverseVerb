// ReverseVerb™ batch export tests. Copyright © 2026 Sheldon Davidson. All rights reserved.
#include "TestHelpers.h"
#include "../Source/BatchExporter.h"

using namespace rvtest;

struct BatchTest : public juce::UnitTest
{
    BatchTest() : juce::UnitTest ("Batch export", "ReverseVerb") {}

    static juce::File freshDir (const juce::String& n)
    {
        auto d = tempDir().getChildFile (n);
        d.deleteRecursively(); d.createDirectory();
        return d;
    }

    struct Info { double sr = 0; int bits = 0; bool isFloat = false; int ch = 0; juce::int64 len = 0; };
    static Info infoOf (const juce::File& f)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        Info i;
        if (r != nullptr) { i.sr = r->sampleRate; i.bits = (int) r->bitsPerSample; i.isFloat = r->usesFloatingPointData; i.ch = (int) r->numChannels; i.len = r->lengthInSamples; }
        return i;
    }
    static float peakOf (const juce::File& f)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) return -1.0f;
        juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
        float p = 0; for (int c = 0; c < b.getNumChannels(); ++c) p = juce::jmax (p, b.getMagnitude (c, 0, b.getNumSamples()));
        return p;
    }

    void runTest() override
    {
        // a processor holding the "current" settings the batch will snapshot
        ReverseVerbProcessor p;
        p.prepareToPlay (44100.0, 256);
        setReal (p, "size", 0.4f); setReal (p, "tail", 1.0f); setReal (p, "tone", 6000.0f); setReal (p, "wet", 0.6f);
        setReal (p, "pitch", 0.3f); setReal (p, "outGain", -3.0f);

        auto srcDir = freshDir ("batch_src");
        auto makeSrc = [&] (const juce::String& n, double sr, int ch, int seed)
        {
            auto f = makeBurst (n, sr, 0.15, ch, seed);
            auto dest = srcDir.getChildFile (n + ".wav"); f.copyFileTo (dest); return dest;
        };
        juce::Array<juce::File> srcs { makeSrc ("kick", 44100, 1, 1), makeSrc ("snare", 48000, 2, 2), makeSrc ("clap", 96000, 2, 3) };
        juce::StringArray paths; for (auto& f : srcs) paths.add (f.getFullPathName());

        beginTest ("every batch file is identical to a single export of the same sample");
        {
            auto out = freshDir ("batch_out1");
            BatchOptions o; o.outputFolder = out;
            BatchExporter ex (p.makeBatchJob (paths, o));
            auto res = ex.process();
            expectEquals (res.ok, 3); expectEquals (res.failed, 0);
            for (auto& s : srcs)
            {
                ReverseVerbProcessor single;
                single.prepareToPlay (44100.0, 256);
                for (auto* prm : p.getParameters()) single.apvts.getParameter (static_cast<juce::AudioProcessorParameterWithID*> (prm)->paramID)->setValueNotifyingHost (prm->getValue());
                expect (single.loadSampleFile (s), "load " + s.getFileName());
                auto ref = tempDir().getChildFile ("single_ref.wav");
                expect (single.exportWav (ref), "single export");
                auto batch = out.getChildFile (s.getFileNameWithoutExtension() + "_reverse.wav");
                expect (batch.existsAsFile(), "batch file exists: " + batch.getFileName());
                const bool same = batch.hasIdenticalContentTo (ref);
                expect (same, "identical to single export: " + s.getFileName());
                if (! same)
                {
                    juce::AudioFormatManager fm; fm.registerBasicFormats();
                    std::unique_ptr<juce::AudioFormatReader> a (fm.createReaderFor (batch)), b (fm.createReaderFor (ref));
                    if (a != nullptr && b != nullptr)
                    {
                        const int n = (int) juce::jmin (a->lengthInSamples, b->lengthInSamples);
                        juce::AudioBuffer<float> ba (2, n), bb (2, n);
                        a->read (&ba, 0, n, 0, true, true); b->read (&bb, 0, n, 0, true, true);
                        float maxDiff = 0; int firstDiff = -1;
                        for (int i = 0; i < n; ++i) { const float d = std::abs (ba.getSample (0, i) - bb.getSample (0, i)); if (d > maxDiff) maxDiff = d; if (d > 1e-6f && firstDiff < 0) firstDiff = i; }
                        logMessage (s.getFileName() + ": lengths " + juce::String (a->lengthInSamples) + " vs " + juce::String (b->lengthInSamples)
                                    + ", sr " + juce::String (a->sampleRate) + " vs " + juce::String (b->sampleRate)
                                    + ", maxDiff " + juce::String (maxDiff) + ", first diff @" + juce::String (firstDiff)
                                    + ", bytes " + juce::String (batch.getSize()) + " vs " + juce::String (ref.getSize()));
                    }
                }
            }
        }

        beginTest ("a corrupt file fails alone; the rest still export");
        {
            auto out = freshDir ("batch_out2");
            auto bad = srcDir.getChildFile ("broken.wav"); bad.replaceWithText ("not audio");
            auto withBad = paths; withBad.insert (1, bad.getFullPathName());
            BatchOptions o; o.outputFolder = out;
            auto res = BatchExporter (p.makeBatchJob (withBad, o)).process();
            expectEquals (res.ok, 3); expectEquals (res.failed, 1);
            expect (res.failures[0].contains ("broken.wav"), "failure names the file");
        }

        beginTest ("collision policies");
        {
            auto out = freshDir ("batch_out3");
            BatchOptions o; o.outputFolder = out;
            auto j = p.makeBatchJob (paths, o);
            expectEquals (BatchExporter (j).process().ok, 3);
            j.options.collision = BatchOptions::Collision::skip;
            auto r2 = BatchExporter (j).process();
            expectEquals (r2.skipped, 3); expectEquals (r2.ok, 0);
            j.options.collision = BatchOptions::Collision::autoNumber;
            expectEquals (BatchExporter (j).process().ok, 3);
            expect (out.getChildFile ("kick_reverse_2.wav").existsAsFile(), "auto-numbered");
            j.options.collision = BatchOptions::Collision::overwrite;
            expectEquals (BatchExporter (j).process().ok, 3);
            expectEquals (out.getNumberOfChildFiles (juce::File::findFiles, "*.wav"), 6);   // overwrite added nothing
        }

        beginTest ("bit depth, sample rate and normalisation options");
        {
            auto out = freshDir ("batch_out4");
            BatchOptions o; o.outputFolder = out; o.bitDepth = 16; o.rateMode = BatchOptions::RateMode::fixed; o.fixedRate = 48000.0;
            o.normalize = BatchOptions::Normalize::peakMinus1dB;
            BatchExporter (p.makeBatchJob (paths, o)).process();
            auto f = out.getChildFile ("kick_reverse.wav");
            auto i = infoOf (f);
            expectEquals (i.bits, 16); expectEquals (i.sr, 48000.0); expectEquals (i.ch, 2);
            expectWithinAbsoluteError (peakOf (f), juce::Decibels::decibelsToGain (-1.0f), 0.002f);

            auto out2 = freshDir ("batch_out5");
            o.outputFolder = out2; o.bitDepth = 32; o.rateMode = BatchOptions::RateMode::matchSource; o.normalize = BatchOptions::Normalize::off;
            BatchExporter (p.makeBatchJob (paths, o)).process();
            auto i32 = infoOf (out2.getChildFile ("clap_reverse.wav"));
            expectEquals (i32.bits, 32); expect (i32.isFloat, "32-bit is float"); expectEquals (i32.sr, 96000.0);
            expectEquals (infoOf (out2.getChildFile ("snare_reverse.wav")).sr, 48000.0);
        }

        beginTest ("file names cannot escape the output folder");
        {
            auto out = freshDir ("batch_out6");
            BatchOptions o; o.outputFolder = out; o.pattern = "../../evil/{name}:{index}?";
            auto res = BatchExporter (p.makeBatchJob (paths, o)).process();
            expectEquals (res.ok, 3);
            for (auto& op : res.outputs) expect (juce::File (op).getParentDirectory() == out, "inside output folder: " + op);
            expectEquals (BatchExporter::sanitiseFileName ("a/b\\c"), juce::String ("abc"));
            expectEquals (BatchExporter::sanitiseFileName ("..."), juce::String ("export"));
        }

        beginTest ("cancel leaves only complete files");
        {
            auto out = freshDir ("batch_out7");
            juce::StringArray many; for (int i = 0; i < 12; ++i) many.add (paths[i % 3]);
            BatchOptions o; o.outputFolder = out;
            BatchExporter ex (p.makeBatchJob (many, o));
            ex.startThread();
            juce::Thread::sleep (30);
            ex.signalThreadShouldExit();
            ex.stopThread (20000);
            expect (ex.isFinished(), "finished");
            auto res = ex.getResult();
            expect (res.cancelled || res.ok == 12, "cancelled (or very fast)");
            for (auto& f : out.findChildFiles (juce::File::findFiles, false))
            {
                expect (f.hasFileExtension ("wav") && f.getFileName().indexOf ("_temp") < 0, "no temp leftovers: " + f.getFileName());
                expect (infoOf (f).len > 0, "complete file: " + f.getFileName());
            }
            expectEquals (out.getNumberOfChildFiles (juce::File::findFiles), res.ok);
        }
    }
};
static BatchTest batchTest;
