// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "BatchExporter.h"
#include "ExportMix.h"

juce::String BatchExporter::sanitiseFileName (const juce::String& in)
{
    juce::String s;
    for (auto c : in) if (c >= 32 && juce::String ("\\/:*?\"<>|").indexOfChar (c) < 0) s += c;
    s = s.trim();
    while (s.endsWithChar ('.') || s.endsWithChar (' ')) s = s.dropLastCharacters (1);
    return s.isEmpty() ? juce::String ("export") : s.substring (0, 120);
}

BatchResult BatchExporter::process()
{
    BatchResult res;
    const auto& opt = job.options;
    done = 0;

    if (! opt.outputFolder.createDirectory().wasOk())
    {
        res.failed = job.files.size();
        res.failures.add ("Could not create the output folder: " + opt.outputFolder.getFullPathName());
        return res;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::WavAudioFormat wav;
    int index = 0;

    for (auto& path : job.files)
    {
        if (threadShouldExit()) { res.cancelled = true; break; }
        ++index;
        const juce::File src (path);
        { const juce::ScopedLock sl (lock); current = src.getFileName(); }

        auto fail = [&] (const juce::String& why) { ++res.failed; res.failures.add (src.getFileName() + ": " + why); ++done; };

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (src));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0) { fail ("could not read audio"); continue; }

        const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 10.0));
        auto buf = std::make_shared<juce::AudioBuffer<float>> ((int) reader->numChannels, len);
        reader->read (buf.get(), 0, len, 0, true, true);

        RenderSettings st = job.settings;
        if (opt.rateMode == BatchOptions::RateMode::matchSource) st.sampleRate = reader->sampleRate;
        else if (opt.rateMode == BatchOptions::RateMode::fixed) st.sampleRate = opt.fixedRate;

        SourceAudio source; source.buffer = buf; source.sampleRate = reader->sampleRate; source.version = index;
        auto rendered = renderSample (st, source, nullptr);
        if (rendered == nullptr || rendered->audio.getNumSamples() == 0) { fail ("nothing to render"); continue; }

        auto mix = mixForExport (*rendered, job.dry, job.wet, job.outGainDb, job.limiter);
        const int n = mix.getNumSamples();

        if (opt.normalize != BatchOptions::Normalize::off)
        {
            const float peak = juce::jmax (mix.getMagnitude (0, 0, n), mix.getMagnitude (1, 0, n));
            const float target = juce::Decibels::decibelsToGain (opt.normalize == BatchOptions::Normalize::peakMinus1dB ? -1.0f : -0.1f);
            if (peak > 1.0e-6f) mix.applyGain (target / peak);
        }
        if (opt.bitDepth == 16 && opt.dither)
        {
            juce::Random rng (index * 7919);
            const float lsb = 1.0f / 32768.0f;
            for (int ch = 0; ch < 2; ++ch)
            {
                float* d = mix.getWritePointer (ch);
                for (int i = 0; i < n; ++i) d[i] += (rng.nextFloat() - rng.nextFloat()) * lsb;
            }
        }

        // file name
        auto base = opt.pattern.replace ("{name}", src.getFileNameWithoutExtension()).replace ("{index}", juce::String (index));
        base = sanitiseFileName (base);
        auto target = opt.outputFolder.getChildFile (base + ".wav");
        if (target.existsAsFile())
        {
            if (opt.collision == BatchOptions::Collision::skip) { ++res.skipped; ++done; continue; }
            if (opt.collision == BatchOptions::Collision::autoNumber)
                for (int k = 2; target.existsAsFile() && k < 100000; ++k) target = opt.outputFolder.getChildFile (base + "_" + juce::String (k) + ".wav");
        }

        {
            juce::TemporaryFile tmp (target);
            bool written = false;
            {
                std::unique_ptr<juce::FileOutputStream> os (tmp.getFile().createOutputStream());
                if (os != nullptr && os->openedOk())
                {
                    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), rendered->sampleRate, 2, opt.bitDepth, {}, 0));
                    if (w != nullptr)
                    {
                        os.release();                                       // the writer owns the stream
                        written = w->writeFromAudioSampleBuffer (mix, 0, n);
                    }
                }
            }                                                               // writer destroyed here: file flushed and closed
            if (! written || ! tmp.overwriteTargetFileWithTemporary()) { fail ("could not write " + target.getFileName()); continue; }
        }
        ++res.ok; res.outputs.add (target.getFullPathName()); ++done;
    }
    return res;
}
