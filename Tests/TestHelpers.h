// ReverseVerb™ test helpers. Copyright © 2026 Sheldon Davidson. All rights reserved.
#pragma once
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

namespace rvtest
{
    inline juce::File tempDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rv_tests");
        d.createDirectory();
        return d;
    }

    // Deterministic decaying noise burst with a leading click. Written as 24-bit WAV.
    inline juce::File makeBurst (const juce::String& name, double sr, double seconds, int channels, int seed = 1)
    {
        const int n = juce::jmax (4, (int) (sr * seconds));
        juce::AudioBuffer<float> b (channels, n);
        juce::Random rng (seed);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float env = std::exp (-6.0f * (float) i / (float) n);
                b.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * env * 0.8f + (i == 0 ? 0.5f : 0.0f));
            }
        auto f = tempDir().getChildFile (name + ".wav");
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) channels, 24, {}, 0));
        if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
        return f;
    }

    struct Stats { int len = 0, hit = -1; double rms = 0, peak = 0, sumAbs = 0, sumW = 0; bool finite = true; };

    inline Stats statsOf (const RenderedSample& r)
    {
        Stats s; s.len = r.audio.getNumSamples(); s.hit = r.hitIndex;
        double sq = 0; const int ch = r.audio.getNumChannels();
        for (int c = 0; c < ch; ++c)
        {
            const float* d = r.audio.getReadPointer (c);
            for (int i = 0; i < s.len; ++i)
            {
                const double x = d[i];
                if (! std::isfinite (x)) { s.finite = false; continue; }
                sq += x * x; s.peak = juce::jmax (s.peak, std::abs (x)); s.sumAbs += std::abs (x);
                if (c == 0) s.sumW += x * (double) ((i % 97) + 1);
            }
        }
        s.rms = s.len > 0 ? std::sqrt (sq / (double) (s.len * ch)) : 0.0;
        return s;
    }

    inline void setNorm (ReverseVerbProcessor& p, const juce::String& id, float v)
    {
        if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    }
    inline void setReal (ReverseVerbProcessor& p, const juce::String& id, float realValue)
    {
        if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
    }

    // Forces a synchronous render with current parameters and returns it.
    inline std::shared_ptr<const RenderedSample> settle (ReverseVerbProcessor& p)
    {
        p.exportWav (tempDir().getChildFile ("flush.wav"));   // exportWav renders if dirty
        return p.getRendered();
    }
}
