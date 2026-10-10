// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "RenderEngine.h"
#include "ReverbEngine.h"

namespace
{
    const int kSyncBeats[] = { 1, 2, 4, 8, 4, 8, 16 };   // 1,2,4,8 beats / 1,2,4 bars
    const float kPitchOct[] = { 1.0f, 2.0f, 4.0f };
}

std::shared_ptr<RenderedSample> renderSample (const RenderSettings& st, const SourceAudio& source, RenderCache* externalCache)
{
    RenderCache localCache;
    RenderCache& cache = externalCache != nullptr ? *externalCache : localCache;

    auto out = std::make_shared<RenderedSample>();
    const double sr = st.sampleRate;
    const double bpm = st.bpm;
    out->sampleRate = sr;

    const double srcSR = source.sampleRate;
    const int version = source.version;
    const bool haveSrc = source.buffer != nullptr && source.buffer->getNumSamples() > 0 && srcSR > 0;

    if (haveSrc)
    {
        const bool sync = st.sync;
        const std::array<double, 17> key {{ (double) version, sr, sync ? bpm : 0.0, sync ? 1.0 : 0.0, sync ? (double) st.syncLen : 0.0,
                                            st.size, st.decay, st.damp, st.diff, st.sep,
                                            st.width, st.er, st.gap, sync ? 0.0 : (double) st.tail,
                                            st.tone, st.basscut, st.shape }};

        if (! cache.valid || cache.key != key)
        {
            const juce::AudioBuffer<float>& src = *source.buffer;

            // 1. resample hit to host rate, stereo
            const int srcLen = src.getNumSamples();
            const double ratio = srcSR / sr;
            const int hitLen = juce::jmax (1, (int) std::floor (srcLen / ratio));
            juce::AudioBuffer<float> hit (2, hitLen);
            if (std::abs (ratio - 1.0) < 1.0e-9)
            {
                for (int ch = 0; ch < 2; ++ch) hit.copyFrom (ch, 0, src, juce::jmin (ch, src.getNumChannels() - 1), 0, hitLen);
            }
            else
            {
                juce::AudioBuffer<float> padded (src.getNumChannels(), srcLen + 16);
                padded.clear();
                for (int ch = 0; ch < src.getNumChannels(); ++ch) padded.copyFrom (ch, 0, src, ch, 0, srcLen);
                for (int ch = 0; ch < 2; ++ch)
                {
                    juce::LagrangeInterpolator interp;
                    interp.process (ratio, padded.getReadPointer (juce::jmin (ch, padded.getNumChannels() - 1)), hit.getWritePointer (ch), hitLen);
                }
            }
            const float hitMag = hit.getMagnitude (0, hitLen);
            if (hitMag > 0.0f) hit.applyGain (0.9f / hitMag);

            // 2. tail length (free or synced to BPM)
            const int gapLen = (int) (st.gap * 0.001f * sr);
            double tailSec = st.tail;
            int beats = 0;
            if (sync)
            {
                const int choice = juce::jlimit (0, 6, st.syncLen);
                beats = kSyncBeats[choice];
                tailSec = juce::jmax (0.05, beats * 60.0 / bpm - hitLen / sr - gapLen / sr);
            }
            const int tailLen = (int) (tailSec * sr);
            const int revLen  = hitLen + tailLen;

            // 3. reverb
            juce::AudioBuffer<float> rev (2, revLen);
            rev.clear();
            for (int ch = 0; ch < 2; ++ch) rev.copyFrom (ch, 0, hit, ch, 0, hitLen);
            rvdsp::ReverbEngine engine;
            engine.setup (sr, st.size, st.decay, st.damp, st.diff, st.sep, st.width, st.er);
            engine.process (rev.getWritePointer (0), rev.getWritePointer (1), revLen);

            // 4. reverse
            for (int ch = 0; ch < 2; ++ch) rev.reverse (ch, 0, revLen);

            // 5. filters
            auto applyIIR = [&] (const juce::IIRCoefficients& c, int passes)
            {
                for (int pass = 0; pass < passes; ++pass)
                    for (int ch = 0; ch < 2; ++ch) { juce::IIRFilter f; f.setCoefficients (c); f.reset(); f.processSamples (rev.getWritePointer (ch), revLen); }
            };
            const float hp = st.basscut;
            if (hp > 21.0f) applyIIR (juce::IIRCoefficients::makeHighPass (sr, hp), 2);
            const float lp = st.tone;
            if (lp < 19900.0f) applyIIR (juce::IIRCoefficients::makeLowPass (sr, lp), 1);

            // 6. shape, fade, normalize
            const float sh = st.shape;
            if (std::abs (sh) > 0.001f && revLen > 1)
                for (int i = 0; i < revLen; ++i)
                {
                    const float x = (float) i / (float) (revLen - 1);
                    const float g = sh > 0.0f ? std::pow (x, 4.0f * sh) : 1.0f + (-sh) * 3.0f * (1.0f - x);
                    for (int ch = 0; ch < 2; ++ch) rev.getWritePointer (ch)[i] *= g;
                }
            rev.applyGainRamp (0, juce::jmin (revLen, (int) (sr * 0.01)), 0.0f, 1.0f);
            const float revMag = rev.getMagnitude (0, revLen);
            if (revMag > 0.0f) rev.applyGain (0.9f / revMag);

            // 7. combine: swell + gap + hit
            const int swellLen = revLen + gapLen;
            const int fullLen = swellLen + hitLen;
            cache.full.setSize (2, fullLen);
            cache.full.clear();
            for (int ch = 0; ch < 2; ++ch) { cache.full.copyFrom (ch, 0, rev, ch, 0, revLen); cache.full.copyFrom (ch, swellLen, hit, ch, 0, hitLen); }
            cache.hitLen = hitLen; cache.swellLen = swellLen; cache.beats = beats;
            cache.key = key; cache.valid = true;
        }

        const auto& full = cache.full;
        const int hitLen = cache.hitLen, swellLen = cache.swellLen;
        const int fullLen = full.getNumSamples();
        out->fullLengthSec = fullLen / sr;
        out->beats = cache.beats;

        // 8. trim
        const int minTrim = juce::jlimit (1, fullLen, (int) (sr * 0.02));
        int tStart = (int) (st.trimStart * fullLen);
        int tEnd   = (int) (st.trimEnd * fullLen);
        tStart = juce::jlimit (0, fullLen - minTrim, tStart);
        tEnd   = juce::jlimit (tStart + minTrim, fullLen, tEnd);
        const int trimLen = tEnd - tStart;
        out->trimStartSec = tStart / sr;
        out->trimEndSec   = tEnd / sr;
        int hitIdx = swellLen - tStart;
        if (hitIdx < 0 || hitIdx >= trimLen) hitIdx = -1;

        // 9. pitch sweep (varispeed)
        const float pitchAmt = st.pitch;
        const float octaves = kPitchOct[juce::jlimit (0, 2, st.pitchRange)];
        const float pitchT = st.pitchTension;
        juce::AudioBuffer<float> outBuf;
        std::vector<float> semiPerSample;
        int hitOut = -1;
        if (std::abs (pitchAmt) > 0.001f)
        {
            std::vector<float> l, r;
            l.reserve ((size_t) trimLen * 2); r.reserve ((size_t) trimLen * 2);
            const float* fl = full.getReadPointer (0) + tStart;
            const float* fr = full.getReadPointer (1) + tStart;
            double p = 0.0;
            const size_t maxOut = (size_t) (sr * 60.0);
            while (p < trimLen - 1 && l.size() < maxOut)
            {
                const int i0 = (int) p; const float frac = (float) (p - i0);
                l.push_back (fl[i0] + (fl[i0 + 1] - fl[i0]) * frac);
                r.push_back (fr[i0] + (fr[i0 + 1] - fr[i0]) * frac);
                const float semis = pitchAmt * octaves * 12.0f * tensionCurve ((float) p / (float) trimLen, pitchT);
                semiPerSample.push_back (semis);
                if (hitIdx >= 0 && hitOut < 0 && p >= hitIdx) hitOut = (int) l.size() - 1;
                p += std::exp2 ((double) semis * (1.0 / 12.0));
            }
            outBuf.setSize (2, (int) l.size());
            std::copy (l.begin(), l.end(), outBuf.getWritePointer (0));
            std::copy (r.begin(), r.end(), outBuf.getWritePointer (1));
        }
        else
        {
            outBuf.setSize (2, trimLen);
            for (int ch = 0; ch < 2; ++ch) outBuf.copyFrom (ch, 0, full, ch, tStart, trimLen);
            semiPerSample.assign ((size_t) trimLen, 0.0f);
            hitOut = hitIdx;
        }

        // 10. volume envelope
        const int n = outBuf.getNumSamples();
        const float v0 = st.volStart, v1 = st.volEnd, vt = st.volTension;
        const bool flatVol = std::abs (v0 - 1.0f) < 0.001f && std::abs (v1 - 1.0f) < 0.001f;
        for (int i = 0; i < n; ++i)
        {
            float g = 1.0f;
            if (! flatVol)
            {
                const float lvl = v0 + (v1 - v0) * tensionCurve ((float) i / (float) juce::jmax (1, n - 1), vt);
                g = lvl * lvl;
                for (int ch = 0; ch < 2; ++ch) outBuf.getWritePointer (ch)[i] *= g;
            }
            if (i % RenderedSample::envStep == 0) { out->gainLin.push_back (g); out->pitchSemi.push_back (semiPerSample[(size_t) i]); }
        }

        out->audio = std::move (outBuf);
        out->hitIndex = hitOut;
    }
    else
        cache.valid = false;

    return out;
}
