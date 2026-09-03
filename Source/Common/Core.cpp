#include "Core.h"

// ---------------- reverb ----------------

void ReverbEngine::setup (double sr, float size, float decay, float damp, float diff, float sep, float width, float er)
{
    const int combBase[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    const int apBase[4]   = { 556, 441, 341, 225 };
    const float scale     = (float) (sr / 44100.0) * (0.45f + 1.3f * size);
    const int spread      = (int) (sep * 60.0f * sr / 44100.0);
    const float feedback  = 0.62f + 0.36f * decay;
    const float apGain    = 0.15f + 0.6f * diff;
    for (int i = 0; i < 8; ++i)
    {
        combL[(size_t) i].setup ((int) (combBase[i] * scale), feedback, damp * 0.9f);
        combR[(size_t) i].setup ((int) (combBase[i] * scale) + spread, feedback, damp * 0.9f);
    }
    for (int i = 0; i < 4; ++i)
    {
        apL[(size_t) i].setup ((int) (apBase[i] * scale), apGain);
        apR[(size_t) i].setup ((int) (apBase[i] * scale) + spread / 2, apGain);
    }
    wet1 = width * 0.5f + 0.5f;
    wet2 = (1.0f - width) * 0.5f;
    erLevel = er;
    const float erMs[8] = { 7.0f, 11.0f, 17.0f, 23.0f, 29.0f, 37.0f, 43.0f, 53.0f };
    int maxTap = 1;
    for (int i = 0; i < 8; ++i)
    {
        erTaps[(size_t) i] = (int) (erMs[i] * 0.001f * sr * (0.5f + size));
        erGain[(size_t) i] = 0.8f * std::pow (0.78f, (float) i);
        maxTap = juce::jmax (maxTap, erTaps[(size_t) i]);
    }
    erBufL.assign ((size_t) maxTap + 1, 0.0f);
    erBufR.assign ((size_t) maxTap + 1, 0.0f);
    erIdx = 0;
}

void ReverbEngine::process (float* l, float* r, int n)
{
    const int erLen = (int) erBufL.size();
    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i], inR = r[i];
        const float mono = (inL + inR) * 0.015f;
        float oL = 0, oR = 0;
        for (auto& c : combL) oL += c.process (mono);
        for (auto& c : combR) oR += c.process (mono);
        for (auto& a : apL) oL = a.process (oL);
        for (auto& a : apR) oR = a.process (oR);

        erBufL[(size_t) erIdx] = inL; erBufR[(size_t) erIdx] = inR;
        float eL = 0, eR = 0;
        for (int t = 0; t < 8; ++t)
        {
            int p = erIdx - erTaps[(size_t) t]; if (p < 0) p += erLen;
            const float g = erGain[(size_t) t];
            if ((t & 1) == 0) { eL += erBufL[(size_t) p] * g; eR += erBufR[(size_t) p] * g * 0.6f; }
            else              { eR += erBufR[(size_t) p] * g; eL += erBufL[(size_t) p] * g * 0.6f; }
        }
        if (++erIdx >= erLen) erIdx = 0;

        l[i] = oL * wet1 + oR * wet2 + eL * erLevel * 0.5f;
        r[i] = oR * wet1 + oL * wet2 + eR * erLevel * 0.5f;
    }
}

// ---------------- parameters ----------------

using P = juce::AudioParameterFloat;
using R = juce::NormalisableRange<float>;
using A = juce::AudioParameterFloatAttributes;
static void addF (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p, const juce::String& id, const juce::String& name, R range, float def, const juce::String& label = {})
{
    p.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name, range, def, A().withLabel (label)));
}

void addReverbParams (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p)
{
    addF (p, IDs::size,  "Size",   R { 0.0f, 1.0f, 0.001f }, 0.7f);
    addF (p, IDs::decay, "Decay",  R { 0.0f, 1.0f, 0.001f }, 0.8f);
    addF (p, IDs::damp,  "Damp",   R { 0.0f, 1.0f, 0.001f }, 0.4f);
    addF (p, IDs::diff,  "Diffusion",  R { 0.0f, 1.0f, 0.001f }, 0.6f);
    addF (p, IDs::er,    "Early Ref",  R { 0.0f, 1.0f, 0.001f }, 0.3f);
    addF (p, IDs::sep,   "Separation", R { 0.0f, 1.0f, 0.001f }, 0.4f);
    addF (p, IDs::width, "Width",  R { 0.0f, 1.0f, 0.001f }, 1.0f);
    addF (p, IDs::gap,   "Delay",  R { 0.0f, 500.0f, 1.0f }, 0.0f, "ms");
}

void addSwellParams (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p)
{
    addF (p, IDs::wet,   "Swell",  R { 0.0f, 1.0f, 0.001f }, 0.8f);
    addF (p, IDs::tail,  "Length", R { 0.1f, 8.0f, 0.01f, 0.5f }, 1.2f, "s");
    addF (p, IDs::shape, "Shape",  R { -1.0f, 1.0f, 0.001f }, 0.0f);
    addF (p, IDs::tone,  "Color",  R { 500.0f, 20000.0f, 1.0f, 0.3f }, 20000.0f, "Hz");
    addF (p, IDs::basscut, "Bass Cut", R { 20.0f, 2000.0f, 1.0f, 0.3f }, 20.0f, "Hz");
    addF (p, IDs::trimStart, "Trim Start", R { 0.0f, 1.0f, 0.0001f }, 0.0f);
    addF (p, IDs::trimEnd,   "Trim End",   R { 0.0f, 1.0f, 0.0001f }, 1.0f);
    addF (p, IDs::pitch,        "Pitch",         R { -1.0f, 1.0f, 0.001f }, 0.0f);
    addF (p, IDs::pitchTension, "Pitch Tension", R { -1.0f, 1.0f, 0.001f }, 0.0f);
    addF (p, IDs::volStart,     "Vol Start",     R { 0.0f, 1.0f, 0.001f }, 1.0f);
    addF (p, IDs::volEnd,       "Vol End",       R { 0.0f, 1.0f, 0.001f }, 1.0f);
    addF (p, IDs::volTension,   "Vol Tension",   R { -1.0f, 1.0f, 0.001f }, 0.0f);
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::sync, 1 }, "Sync to BPM", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::syncLen, 1 }, "Sync Length", syncChoiceNames(), 2));
    addF (p, IDs::stretch,    "Stretch",     R { 1.0f, 32.0f, 0.01f, 0.4f }, 1.0f, "x");
    addF (p, IDs::bassBoost,  "Bass Boost",  R { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");
    addF (p, IDs::trebleBoost,"Treble Boost",R { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");
    addF (p, IDs::gateDepth,  "Gate Depth",  R { 0.0f, 1.0f, 0.001f }, 1.0f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::gateShape, 1 }, "Gate Shape", juce::StringArray { "Hard", "Smooth", "LFO" }, 0));
    addF (p, IDs::panStart,   "Pan Start",   R { -1.0f, 1.0f, 0.001f }, 0.0f);
    addF (p, IDs::panEnd,     "Pan End",     R { -1.0f, 1.0f, 0.001f }, 0.0f);
    addF (p, IDs::panTension, "Pan Tension", R { -1.0f, 1.0f, 0.001f }, 0.0f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::fxType, 1 }, "FX Type",
                     juce::StringArray { "Off", "Echo", "Reverse Echo", "Chorus", "Reverse Chorus" }, 0));
    addF (p, IDs::fxTime,     "FX Time",     R { 20.0f, 1000.0f, 1.0f, 0.5f }, 250.0f, "ms");
    addF (p, IDs::fxFeedback, "FX Feedback", R { 0.0f, 0.9f, 0.001f }, 0.4f);
    addF (p, IDs::fxDepth,    "FX Depth",    R { 0.0f, 1.0f, 0.001f }, 0.5f);
    addF (p, IDs::fxMix,      "FX Mix",      R { 0.0f, 1.0f, 0.001f }, 0.5f);
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::normalize, 1 }, "Normalize", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::swapStereo, 1 }, "Swap Stereo", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::invertPhase, 1 }, "Invert Phase", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::pitchRange, 1 }, "Pitch Range",
                     juce::StringArray { "1 oct", "2 oct", "4 oct" }, 0));
}

juce::StringArray sharedParamIds()
{
    return { IDs::size, IDs::decay, IDs::damp, IDs::diff, IDs::er, IDs::sep, IDs::width, IDs::gap,
             IDs::wet, IDs::tail, IDs::shape, IDs::tone, IDs::basscut, IDs::trimStart, IDs::trimEnd,
             IDs::pitch, IDs::pitchTension, IDs::volStart, IDs::volEnd, IDs::volTension, IDs::sync, IDs::syncLen, IDs::pitchRange,
             IDs::panStart, IDs::panEnd, IDs::panTension, IDs::fxType, IDs::fxTime, IDs::fxFeedback, IDs::fxDepth, IDs::fxMix,
             IDs::normalize, IDs::swapStereo, IDs::invertPhase, IDs::stretch, IDs::bassBoost, IDs::trebleBoost, IDs::gateDepth, IDs::gateShape };
}

void RVHost::resetEdits()
{
    setParam (IDs::trimStart, 0.0f);  setParam (IDs::trimEnd, 1.0f);
    setParam (IDs::pitch, 0.0f);      setParam (IDs::pitchTension, 0.0f);
    setParam (IDs::volStart, 1.0f);   setParam (IDs::volEnd, 1.0f);  setParam (IDs::volTension, 0.0f);
    setParam (IDs::panStart, 0.0f);   setParam (IDs::panEnd, 0.0f);  setParam (IDs::panTension, 0.0f);
    setAllBeats (true);
    shuffleBeats (true);
}

void RVHost::randomizeReverb()
{
    auto& rng = juce::Random::getSystemRandom();
    setParam (IDs::size,  rng.nextFloat());
    setParam (IDs::decay, 0.4f + 0.6f * rng.nextFloat());
    setParam (IDs::damp,  rng.nextFloat());
    setParam (IDs::diff,  rng.nextFloat());
    setParam (IDs::er,    rng.nextFloat() * 0.7f);
    setParam (IDs::sep,   rng.nextFloat());
    setParam (IDs::shape, rng.nextFloat() * 1.4f - 0.7f);
    setParam (IDs::tone,  2000.0f + std::pow (rng.nextFloat(), 0.5f) * 18000.0f);
    setParam (IDs::basscut, 20.0f + std::pow (rng.nextFloat(), 2.0f) * 800.0f);
}


// ---------------- DSP helpers ----------------

void applyEcho (juce::AudioBuffer<float>& b, double sr, float timeMs, float feedback, float mix)
{
    const int n = b.getNumSamples();
    const int d = juce::jmax (1, (int) (timeMs * 0.001f * sr));
    if (n <= 0 || mix <= 0.001f) return;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* x = b.getWritePointer (ch);
        std::vector<float> wet ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const float delayed = i >= d ? wet[(size_t) (i - d)] * feedback + x[i - d] : 0.0f;
            wet[(size_t) i] = delayed;
        }
        for (int i = 0; i < n; ++i) x[i] = x[i] * (1.0f - mix * 0.5f) + wet[(size_t) i] * mix;
    }
}

void applyChorus (juce::AudioBuffer<float>& b, double sr, float rateHz, float depth, float mix)
{
    const int n = b.getNumSamples();
    if (n <= 0 || mix <= 0.001f) return;
    const float base = (float) (sr * 0.012), dep = (float) (sr * 0.008) * depth;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* x = b.getWritePointer (ch);
        std::vector<float> src (x, x + n);
        const float phase0 = ch == 0 ? 0.0f : juce::MathConstants<float>::pi * 0.5f;
        for (int i = 0; i < n; ++i)
        {
            float wet = 0.0f;
            for (int v = 0; v < 2; ++v)
            {
                const float ph = phase0 + (float) v * 2.1f + juce::MathConstants<float>::twoPi * rateHz * (1.0f + 0.37f * (float) v) * (float) i / (float) sr;
                const float dl = base + dep * (0.5f + 0.5f * std::sin (ph));
                const float rp = (float) i - dl;
                if (rp >= 1.0f) { const int i0 = (int) rp; const float f = rp - (float) i0; wet += src[(size_t) i0] + (src[(size_t) juce::jmin (n - 1, i0 + 1)] - src[(size_t) i0]) * f; }
            }
            x[i] = src[(size_t) i] * (1.0f - mix * 0.5f) + wet * 0.5f * mix;
        }
    }
}

void applySwellFx (juce::AudioBuffer<float>& b, double sr, const ParamSource& P, bool beforeReverse)
{
    const int type = (int) P.param (IDs::fxType);
    if (type == 0) return;
    const bool reverseVariant = type == 2 || type == 4;
    if (reverseVariant != beforeReverse) return;
    const float mix = P.param (IDs::fxMix);
    if (type == 1 || type == 2) applyEcho (b, sr, P.param (IDs::fxTime), P.param (IDs::fxFeedback), mix);
    else applyChorus (b, sr, 0.1f + 4.9f * (P.param (IDs::fxTime) - 20.0f) / 980.0f, P.param (IDs::fxDepth), mix);
}

void applyBeatOrder (juce::AudioBuffer<float>& b, int gridLen, int beats, const BeatOrder& order, double sr)
{
    if (beats <= 1 || gridLen <= 0 || order.isIdentity()) return;
    const int n = b.getNumSamples();
    juce::AudioBuffer<float> src; src.makeCopyOf (b);
    const int fade = juce::jmax (1, (int) (sr * 0.004));
    for (int slot = 0; slot < beats; ++slot)
    {
        const int srcBeat = juce::jlimit (0, beats - 1, (int) order.order[(size_t) slot].load());
        const int dStart = (int) ((juce::int64) gridLen * slot / beats), dEnd = (int) ((juce::int64) gridLen * (slot + 1) / beats);
        const int sStart = (int) ((juce::int64) gridLen * srcBeat / beats);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* d = b.getWritePointer (ch); const float* s = src.getReadPointer (ch);
            for (int i = dStart; i < dEnd && i < n; ++i)
            {
                const int si = sStart + (i - dStart);
                float g = 1.0f;
                if (i - dStart < fade) g = (float) (i - dStart) / (float) fade;
                if (dEnd - i < fade) g = juce::jmin (g, (float) (dEnd - i) / (float) fade);
                d[i] = si < n ? s[si] * g : 0.0f;
            }
        }
    }
}

void applyBeatGate (juce::AudioBuffer<float>& b, int gridLen, int beats, const GateMask& mask, int shape, float depth, double sr)
{
    if (beats <= 0 || gridLen <= 0) return;
    bool any = false;
    for (int i = 0; i < beats; ++i) if (! mask.get (i)) any = true;
    if (! any) return;
    const int n = b.getNumSamples();
    const float floorG = 1.0f - depth;
    const float beatLen = (float) gridLen / (float) beats;
    const int fade = juce::jmax (1, (int) (sr * 0.005));
    for (int i = 0; i < n; ++i)
    {
        const float pos = (float) i / beatLen;
        const int beat = juce::jmin (beats - 1, (int) pos);
        const float frac = pos - (float) beat;
        const bool on = mask.get (beat);
        float g;
        if (shape == 0)
        {
            g = on ? 1.0f : floorG;
            const bool prevOn = beat > 0 ? mask.get (beat - 1) : on;
            const bool nextOn = beat < beats - 1 ? mask.get (beat + 1) : on;
            const int inBeat = (int) (frac * beatLen), toEnd = (int) ((1.0f - frac) * beatLen);
            if (prevOn != on && inBeat < fade) g = juce::jmap ((float) inBeat / (float) fade, prevOn ? 1.0f : floorG, g);
            if (nextOn != on && toEnd < fade)  g = juce::jmap ((float) toEnd / (float) fade, nextOn ? 1.0f : floorG, g);
        }
        else if (shape == 1)
        {
            const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * frac);
            g = on ? floorG + (1.0f - floorG) * win : floorG;
        }
        else
        {
            // LFO: smooth cosine transitions across beat boundaries, half a beat wide
            auto level = [&] (int bt) { return bt < 0 || bt >= beats || mask.get (bt) ? 1.0f : floorG; };
            const float cur = level (beat);
            if (frac < 0.25f) { const float t = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::pi * (frac / 0.25f - 0.5f)); g = juce::jmap (t, 0.5f * (level (beat - 1) + cur), cur); }
            else if (frac > 0.75f) { const float t = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::pi * ((frac - 0.75f) / 0.25f - 0.5f)); g = juce::jmap (t, cur, 0.5f * (cur + level (beat + 1))); }
            else g = cur;
        }
        for (int ch = 0; ch < b.getNumChannels(); ++ch) b.getWritePointer (ch)[i] *= g;
    }
}

void applyPanEnvelope (juce::AudioBuffer<float>& b, const ParamSource& P, std::vector<float>* panOut)
{
    const int n = b.getNumSamples();
    const float p0 = P.param (IDs::panStart), p1 = P.param (IDs::panEnd), pt = P.param (IDs::panTension);
    const bool flat = std::abs (p0) < 0.001f && std::abs (p1) < 0.001f;
    if (panOut) panOut->clear();
    if (b.getNumChannels() < 2) return;
    for (int i = 0; i < n; ++i)
    {
        float p = 0.0f;
        if (! flat)
        {
            p = p0 + (p1 - p0) * tensionCurve ((float) i / (float) juce::jmax (1, n - 1), pt);
            const float gl = p > 0.0f ? 1.0f - p : 1.0f, gr = p < 0.0f ? 1.0f + p : 1.0f;
            b.getWritePointer (0)[i] *= gl;
            b.getWritePointer (1)[i] *= gr;
        }
        if (panOut && i % RenderedSample::envStep == 0) panOut->push_back (p);
    }
}

void finalizeOutput (juce::AudioBuffer<float>& b, const ParamSource& P)
{
    const int n = b.getNumSamples();
    if (n <= 0) return;
    if (P.param (IDs::swapStereo) > 0.5f && b.getNumChannels() >= 2)
        for (int i = 0; i < n; ++i) std::swap (b.getWritePointer (0)[i], b.getWritePointer (1)[i]);
    if (P.param (IDs::invertPhase) > 0.5f) b.applyGain (-1.0f);
    if (P.param (IDs::normalize) > 0.5f)
    {
        const float m = b.getMagnitude (0, n);
        if (m > 0.0f) b.applyGain (0.989f / m);
    }
}

void applyShelves (juce::AudioBuffer<float>& b, double sr, const ParamSource& P)
{
    const float bass = P.param (IDs::bassBoost), treble = P.param (IDs::trebleBoost);
    const int n = b.getNumSamples();
    if (n <= 0) return;
    if (std::abs (bass) > 0.05f)
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        { juce::IIRFilter f; f.setCoefficients (juce::IIRCoefficients::makeLowShelf (sr, 160.0, 0.7, juce::Decibels::decibelsToGain (bass))); f.processSamples (b.getWritePointer (ch), n); }
    if (std::abs (treble) > 0.05f)
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        { juce::IIRFilter f; f.setCoefficients (juce::IIRCoefficients::makeHighShelf (sr, 4000.0, 0.7, juce::Decibels::decibelsToGain (treble))); f.processSamples (b.getWritePointer (ch), n); }
}

void makeGhost (const juce::AudioBuffer<float>& b, RenderedSample& meta)
{
    const int n = b.getNumSamples();
    meta.ghostMin.assign ((size_t) RenderedSample::ghostRes, 0.0f);
    meta.ghostMax.assign ((size_t) RenderedSample::ghostRes, 0.0f);
    if (n <= 0) return;
    const float* d = b.getReadPointer (0);
    for (int c = 0; c < RenderedSample::ghostRes; ++c)
    {
        const int a = (int) ((juce::int64) n * c / RenderedSample::ghostRes), e = juce::jmax (a + 1, (int) ((juce::int64) n * (c + 1) / RenderedSample::ghostRes));
        float mn = 0, mx = 0;
        for (int i = a; i < e && i < n; ++i) { mn = juce::jmin (mn, d[i]); mx = juce::jmax (mx, d[i]); }
        meta.ghostMin[(size_t) c] = mn; meta.ghostMax[(size_t) c] = mx;
    }
}

juce::AudioBuffer<float> stretchHit (const juce::AudioBuffer<float>& hit, double sr, float factor)
{
    const int n = hit.getNumSamples();
    if (n <= 0 || factor <= 1.01f) { juce::AudioBuffer<float> c; c.makeCopyOf (hit); return c; }
    const int outLen = (int) (n * factor);
    const int grain = juce::jmax (64, (int) (sr * 0.045));
    const int hop = grain / 4;
    juce::AudioBuffer<float> out (2, outLen + grain);
    out.clear();
    juce::Random rng (12345);
    std::vector<float> win ((size_t) grain);
    for (int i = 0; i < grain; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) grain);
    for (int o = 0; o < outLen; o += hop)
    {
        const float srcPos = (float) o / factor;
        const int jitter = rng.nextInt (juce::jmax (1, grain / 3)) - grain / 6;
        int s0 = juce::jlimit (0, juce::jmax (0, n - 1), (int) srcPos + jitter);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* src = hit.getReadPointer (juce::jmin (ch, hit.getNumChannels() - 1));
            auto* d = out.getWritePointer (ch);
            for (int i = 0; i < grain; ++i)
            {
                const int si = s0 + i;
                if (si >= n) break;
                d[o + i] += src[si] * win[(size_t) i];
            }
        }
    }
    out.setSize (2, outLen, true);
    // even out the envelope so the stretch becomes a smooth, steady body (then the SHAPE knob draws the curve)
    const int w = juce::jmax (1, (int) (sr * 0.03));
    std::vector<float> env ((size_t) outLen, 0.0f);
    double acc = 0.0; int cnt = 0;
    for (int i = 0; i < outLen; ++i)
    {
        const float v = out.getSample (0, i); acc += v * v; ++cnt;
        if (i >= w) { const float o = out.getSample (0, i - w); acc -= o * o; --cnt; }
        env[(size_t) i] = std::sqrt ((float) (acc / juce::jmax (1, cnt)));
    }
    float target = 0.0f; for (auto e : env) target = juce::jmax (target, e);
    target *= 0.5f;
    for (int i = 0; i < outLen; ++i)
    {
        const float g = juce::jmin (16.0f, target / (env[(size_t) i] + 1.0e-4f));
        for (int ch = 0; ch < 2; ++ch) out.getWritePointer (ch)[i] *= g;
    }
    out.applyGainRamp (0, juce::jmin (outLen, (int) (sr * 0.01)), 0.0f, 1.0f);
    out.applyGainRamp (juce::jmax (0, outLen - (int) (sr * 0.05)), juce::jmin (outLen, (int) (sr * 0.05)), 1.0f, 0.0f);
    const float m = out.getMagnitude (0, outLen);
    if (m > 0.0f) out.applyGain (0.9f / m);
    return out;
}

juce::AudioBuffer<float> generateHit (int type, double sr)
{
    juce::Random rng (777);
    auto noise = [&] { return rng.nextFloat() * 2.0f - 1.0f; };
    int n = (int) (sr * (type == 0 ? 0.18 : type == 1 ? 0.28 : type == 2 ? 0.30 : type == 3 ? 0.45 : 0.12));
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    auto* l = b.getWritePointer (0);
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        float v = 0.0f;
        if (type == 0)       v = noise() * std::exp (-t * 38.0f);                                                          // closed hat
        else if (type == 1)  v = 0.8f * std::sin (juce::MathConstants<float>::twoPi * (185.0f + 90.0f * std::exp (-t * 40.0f)) * t) * std::exp (-t * 22.0f)
                                 + 0.7f * noise() * std::exp (-t * 14.0f);                                                   // snare
        else if (type == 2)  { float g = 0.0f; for (int k = 0; k < 4; ++k) { const float tk = t - 0.011f * (float) k; if (tk >= 0.0f) g += std::exp (-tk * 140.0f); }
                               v = noise() * (g * 0.6f + std::exp (-t * 12.0f) * (t > 0.03f ? 0.8f : 0.0f)); }               // clap
        else if (type == 3)  v = std::sin (juce::MathConstants<float>::twoPi * (48.0f + 110.0f * std::exp (-t * 28.0f)) * t) * std::exp (-t * 7.0f)
                                 + 0.3f * noise() * std::exp (-t * 120.0f);                                                  // kick
        else                 v = 0.7f * std::sin (juce::MathConstants<float>::twoPi * 1700.0f * t) * std::exp (-t * 70.0f) + 0.5f * noise() * std::exp (-t * 90.0f);   // rim
        l[i] = v;
    }
    if (type == 0) { juce::IIRFilter f; f.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 6500.0)); f.processSamples (l, n); f.reset(); f.processSamples (l, n); }
    if (type == 1) { juce::IIRFilter f; f.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 140.0)); f.processSamples (l, n); }
    if (type == 2) { juce::IIRFilter f; f.setCoefficients (juce::IIRCoefficients::makeBandPass (sr, 1600.0, 0.8)); f.processSamples (l, n); }
    b.copyFrom (1, 0, b, 0, 0, n);
    const float m = b.getMagnitude (0, n);
    if (m > 0.0f) b.applyGain (0.9f / m);
    return b;
}

// ---------------- swell renderer ----------------

juce::AudioBuffer<float> renderSwell (const juce::AudioBuffer<float>& hitIn, double sr, const ParamSource& P, double bpm,
                                      int maxOutSamples, RenderedSample& meta, const GateMask* gate, const BeatOrder* order)
{
    juce::AudioBuffer<float> hit = stretchHit (hitIn, sr, P.param (IDs::stretch));
    const int hitLen = hit.getNumSamples();
    const int gapLen = (int) (P.param (IDs::gap) * 0.001f * sr);
    double tailSec = P.param (IDs::tail);
    int beats = 0;
    if (P.param (IDs::sync) > 0.5f)
    {
        beats = kSyncBeats[juce::jlimit (0, kNumSyncChoices - 1, (int) P.param (IDs::syncLen))];
        tailSec = juce::jlimit (0.05, 60.0, beats * 60.0 / juce::jmax (20.0, bpm) - hitLen / sr - gapLen / sr);
    }
    const int tailLen = (int) (tailSec * sr);
    const int revLen  = hitLen + tailLen;

    juce::AudioBuffer<float> rev (2, revLen);
    rev.clear();
    for (int ch = 0; ch < 2; ++ch) rev.copyFrom (ch, 0, hit, juce::jmin (ch, hit.getNumChannels() - 1), 0, hitLen);
    ReverbEngine engine;
    engine.setup (sr, P.param (IDs::size), P.param (IDs::decay), P.param (IDs::damp), P.param (IDs::diff), P.param (IDs::sep), P.param (IDs::width), P.param (IDs::er));
    engine.process (rev.getWritePointer (0), rev.getWritePointer (1), revLen);
    applySwellFx (rev, sr, P, true);
    for (int ch = 0; ch < 2; ++ch) rev.reverse (ch, 0, revLen);
    applySwellFx (rev, sr, P, false);

    auto applyIIR = [&] (const juce::IIRCoefficients& c, int passes)
    {
        for (int pass = 0; pass < passes; ++pass)
            for (int ch = 0; ch < 2; ++ch) { juce::IIRFilter f; f.setCoefficients (c); f.reset(); f.processSamples (rev.getWritePointer (ch), revLen); }
    };
    const float hp = P.param (IDs::basscut);
    if (hp > 21.0f) applyIIR (juce::IIRCoefficients::makeHighPass (sr, hp), 2);
    const float lp = P.param (IDs::tone);
    if (lp < 19900.0f) applyIIR (juce::IIRCoefficients::makeLowPass (sr, lp), 1);

    const float s = P.param (IDs::shape);
    if (std::abs (s) > 0.001f && revLen > 1)
        for (int i = 0; i < revLen; ++i)
        {
            const float x = (float) i / (float) (revLen - 1);
            const float g = s > 0.0f ? std::pow (x, 4.0f * s) : 1.0f + (-s) * 3.0f * (1.0f - x);
            for (int ch = 0; ch < 2; ++ch) rev.getWritePointer (ch)[i] *= g;
        }
    rev.applyGainRamp (0, juce::jmin (revLen, (int) (sr * 0.01)), 0.0f, 1.0f);
    const float revMag = rev.getMagnitude (0, revLen);
    if (revMag > 0.0f) rev.applyGain (0.9f / revMag);

    const int fullLen = revLen + gapLen;
    juce::AudioBuffer<float> full (2, fullLen);
    full.clear();
    for (int ch = 0; ch < 2; ++ch) full.copyFrom (ch, 0, rev, ch, 0, revLen);
    meta.fullLengthSec = fullLen / sr;
    meta.beats = beats;
    meta.sampleRate = sr;
    if (beats > 0)
    {
        if (order) applyBeatOrder (full, fullLen, beats, *order, sr);
        if (gate)
        {
            applyBeatGate (full, fullLen, beats, *gate, (int) P.param (IDs::gateShape), P.param (IDs::gateDepth), sr);
            meta.gateMask.resize ((size_t) beats);
            for (int i = 0; i < beats; ++i) meta.gateMask[(size_t) i] = gate->get (i) ? 1 : 0;
        }
    }
    makeGhost (full, meta);

    // trim
    int tStart = juce::jlimit (0, fullLen - 1, (int) (P.param (IDs::trimStart) * fullLen));
    int tEnd   = juce::jlimit (tStart + (int) (sr * 0.02), fullLen, (int) (P.param (IDs::trimEnd) * fullLen));
    if (tEnd <= tStart) { tStart = 0; tEnd = fullLen; }
    const int trimLen = tEnd - tStart;
    meta.trimStartSec = tStart / sr;
    meta.trimEndSec   = tEnd / sr;

    // pitch sweep
    const float pitchAmt = P.param (IDs::pitch);
    const float octaves = kPitchOct[juce::jlimit (0, 2, (int) P.param (IDs::pitchRange))];
    const float pitchT = P.param (IDs::pitchTension);
    juce::AudioBuffer<float> out;
    std::vector<float> semi;
    if (std::abs (pitchAmt) > 0.001f && trimLen > 2)
    {
        // pass 1: count output samples (no storage)
        auto semisAt = [&] (double p) { return pitchAmt * octaves * 12.0f * tensionCurve ((float) p / (float) trimLen, pitchT); };
        juce::int64 count = 0;
        {
            double p = 0.0;
            const juce::int64 hardCap = (juce::int64) (sr * 120.0);
            while (p < trimLen - 1 && count < hardCap) { p += std::pow (2.0, semisAt (p) / 12.0); ++count; }
        }
        const int keep = maxOutSamples > 0 ? (int) juce::jmin<juce::int64> (count, maxOutSamples) : (int) juce::jmin<juce::int64> (count, (juce::int64) (sr * 60.0));
        const juce::int64 skip = count - keep;
        out.setSize (2, juce::jmax (1, keep));
        out.clear();
        semi.assign ((size_t) juce::jmax (1, keep), 0.0f);
        const float* fl = full.getReadPointer (0) + tStart;
        const float* fr = full.getReadPointer (1) + tStart;
        auto* ol = out.getWritePointer (0); auto* orr = out.getWritePointer (1);
        double p = 0.0; juce::int64 idx = 0;
        while (p < trimLen - 1 && idx < count)
        {
            const float st = semisAt (p);
            if (idx >= skip)
            {
                const int i0 = (int) p; const float frac = (float) (p - i0);
                const int o = (int) (idx - skip);
                ol[o] = fl[i0] + (fl[i0 + 1] - fl[i0]) * frac;
                orr[o] = fr[i0] + (fr[i0 + 1] - fr[i0]) * frac;
                semi[(size_t) o] = st;
            }
            p += std::pow (2.0, st / 12.0);
            ++idx;
        }
        if (skip > 0) out.applyGainRamp (0, juce::jmin (keep, (int) (sr * 0.02)), 0.0f, 1.0f);
    }
    else
    {
        out.setSize (2, trimLen);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, 0, full, ch, tStart, trimLen);
        semi.assign ((size_t) trimLen, 0.0f);
    }

    // cap (no-pitch path): keep the END of the swell
    if (maxOutSamples > 0 && out.getNumSamples() > maxOutSamples)
    {
        const int drop = out.getNumSamples() - maxOutSamples;
        juce::AudioBuffer<float> capped (2, maxOutSamples);
        for (int ch = 0; ch < 2; ++ch) capped.copyFrom (ch, 0, out, ch, drop, maxOutSamples);
        capped.applyGainRamp (0, juce::jmin (maxOutSamples, (int) (sr * 0.02)), 0.0f, 1.0f);
        out = std::move (capped);
        semi.erase (semi.begin(), semi.begin() + drop);
    }

    // volume envelope + env readouts
    const int n = out.getNumSamples();
    const float v0 = P.param (IDs::volStart), v1 = P.param (IDs::volEnd), vt = P.param (IDs::volTension);
    const bool flat = std::abs (v0 - 1.0f) < 0.001f && std::abs (v1 - 1.0f) < 0.001f;
    meta.gainLin.clear(); meta.pitchSemi.clear();
    for (int i = 0; i < n; ++i)
    {
        float g = 1.0f;
        if (! flat)
        {
            const float lvl = v0 + (v1 - v0) * tensionCurve ((float) i / (float) juce::jmax (1, n - 1), vt);
            g = lvl * lvl;
            for (int ch = 0; ch < 2; ++ch) out.getWritePointer (ch)[i] *= g;
        }
        if (i % RenderedSample::envStep == 0) { meta.gainLin.push_back (g); meta.pitchSemi.push_back (semi[(size_t) i]); }
    }
    applyPanEnvelope (out, P, &meta.panPos);
    applyShelves (out, sr, P);
    finalizeOutput (out, P);
    return out;
}

// ---------------- safe wav write ----------------

bool writeWavSafely (const juce::File& dest, const juce::AudioBuffer<float>& audio, double sr)
{
    if (audio.getNumSamples() <= 0) return false;
    dest.getParentDirectory().createDirectory();
    auto tmp = dest.getSiblingFile (".rv_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()) + ".tmp");
    {
        std::unique_ptr<juce::FileOutputStream> os (tmp.createOutputStream());
        if (os == nullptr || ! os->openedOk()) return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned int) audio.getNumChannels(), 24, {}, 0));
        if (w == nullptr) { tmp.deleteFile(); return false; }
        os.release();
        if (! w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples())) { w.reset(); tmp.deleteFile(); return false; }
        if (! w->flush()) { w.reset(); tmp.deleteFile(); return false; }
    }
    if (dest.existsAsFile() && ! dest.deleteFile()) { tmp.deleteFile(); return false; }
    if (! tmp.moveFileTo (dest)) { tmp.deleteFile(); return false; }
    return true;
}

// ---------------- presets ----------------

std::vector<FactoryPreset> factoryPresets()
{
    using M = std::map<juce::String, float>;
    auto base = [] (float size, float decay, float damp, float diff, float er, float sep, float tail, float shape, float tone, float bass)
    {
        return M { { IDs::size, size }, { IDs::decay, decay }, { IDs::damp, damp }, { IDs::diff, diff }, { IDs::er, er }, { IDs::sep, sep },
                   { IDs::width, 1.0f }, { IDs::gap, 0.0f }, { IDs::tail, tail }, { IDs::shape, shape }, { IDs::tone, tone }, { IDs::basscut, bass },
                   { IDs::wet, 0.8f }, { IDs::trimStart, 0.0f }, { IDs::trimEnd, 1.0f }, { IDs::pitch, 0.0f }, { IDs::pitchTension, 0.0f },
                   { IDs::volStart, 1.0f }, { IDs::volEnd, 1.0f }, { IDs::volTension, 0.0f }, { IDs::sync, 0.0f },
                   { IDs::panStart, 0.0f }, { IDs::panEnd, 0.0f }, { IDs::panTension, 0.0f }, { IDs::fxType, 0.0f }, { IDs::normalize, 0.0f }, { IDs::swapStereo, 0.0f }, { IDs::invertPhase, 0.0f } };
    };
    std::vector<FactoryPreset> v;
    v.push_back ({ "Init",              base (0.7f, 0.8f, 0.4f, 0.6f, 0.3f, 0.4f, 1.2f, 0.0f, 20000.0f, 20.0f) });
    v.push_back ({ "DnB Snare Swell",   base (0.75f, 0.85f, 0.5f, 0.7f, 0.2f, 0.5f, 0.7f, 0.3f, 12000.0f, 180.0f) });
    v.push_back ({ "Tight Hat Rush",    base (0.35f, 0.6f, 0.7f, 0.9f, 0.5f, 0.6f, 0.3f, 0.5f, 16000.0f, 400.0f) });
    v.push_back ({ "Dubstep Clap Rise", base (0.95f, 0.95f, 0.3f, 0.5f, 0.3f, 0.7f, 2.0f, 0.4f, 9000.0f, 120.0f) });
    v.push_back ({ "Dark Cavern",       base (1.0f, 1.0f, 0.85f, 0.4f, 0.1f, 0.3f, 3.0f, -0.3f, 2500.0f, 60.0f) });
    v.push_back ({ "Bright Shimmer",    base (0.6f, 0.9f, 0.1f, 0.8f, 0.6f, 0.8f, 1.5f, 0.2f, 20000.0f, 600.0f) });
    { auto m = base (0.8f, 0.9f, 0.4f, 0.7f, 0.3f, 0.5f, 1.5f, 0.3f, 14000.0f, 150.0f); m[IDs::pitch] = 0.6f; m[IDs::pitchRange] = 1.0f; m[IDs::pitchTension] = 0.4f; v.push_back ({ "Pitch Riser", m }); }
    { auto m = base (0.8f, 0.9f, 0.5f, 0.6f, 0.2f, 0.5f, 1.0f, 0.0f, 12000.0f, 100.0f); m[IDs::volStart] = 0.0f; m[IDs::volTension] = 0.5f; v.push_back ({ "Fade From Silence", m }); }
    { auto m = base (0.7f, 0.85f, 0.4f, 0.7f, 0.3f, 0.4f, 1.0f, 0.2f, 14000.0f, 150.0f); m[IDs::sync] = 1.0f; m[IDs::syncLen] = 0.0f; v.push_back ({ "Synced 1 Beat", m }); }
    { auto m = base (0.9f, 0.9f, 0.4f, 0.7f, 0.3f, 0.5f, 2.0f, 0.3f, 14000.0f, 150.0f); m[IDs::sync] = 1.0f; m[IDs::syncLen] = 4.0f; v.push_back ({ "Synced 1 Bar", m }); }
    { auto m = base (0.9f, 0.95f, 0.5f, 0.6f, 0.2f, 0.5f, 2.5f, 0.5f, 10000.0f, 200.0f); m[IDs::gap] = 60.0f; v.push_back ({ "Gap Before Hit", m }); }
    { auto m = base (0.6f, 0.8f, 0.5f, 0.7f, 0.3f, 0.5f, 1.0f, 0.4f, 14000.0f, 150.0f); m[IDs::stretch] = 8.0f; v.push_back ({ "Stretched Rise", m }); }
    { auto m = base (0.8f, 0.9f, 0.4f, 0.7f, 0.3f, 0.5f, 1.5f, 0.2f, 12000.0f, 120.0f); m[IDs::fxType] = 2.0f; m[IDs::fxTime] = 180.0f; m[IDs::fxFeedback] = 0.5f; m[IDs::fxMix] = 0.5f; v.push_back ({ "Reverse Echo Swell", m }); }
    { auto m = base (0.7f, 0.85f, 0.3f, 0.8f, 0.4f, 0.7f, 1.2f, 0.2f, 16000.0f, 200.0f); m[IDs::fxType] = 3.0f; m[IDs::fxTime] = 200.0f; m[IDs::fxDepth] = 0.7f; m[IDs::fxMix] = 0.6f; m[IDs::panStart] = -0.8f; m[IDs::panEnd] = 0.8f; v.push_back ({ "Chorus Pan Sweep", m }); }
    { auto m = base (0.7f, 0.85f, 0.4f, 0.7f, 0.3f, 0.5f, 1.2f, 0.2f, 14000.0f, 150.0f); m[IDs::fxType] = 2.0f; m[IDs::fxTime] = 180.0f; m[IDs::fxFeedback] = 0.5f; m[IDs::fxMix] = 0.6f; v.push_back ({ "Reverse Echo Swell", m }); }
    { auto m = base (0.6f, 0.8f, 0.3f, 0.8f, 0.4f, 0.7f, 1.5f, 0.0f, 16000.0f, 200.0f); m[IDs::fxType] = 3.0f; m[IDs::fxTime] = 600.0f; m[IDs::fxDepth] = 0.7f; m[IDs::fxMix] = 0.6f; v.push_back ({ "Chorus Wash", m }); }
    { auto m = base (0.8f, 0.9f, 0.4f, 0.7f, 0.3f, 0.5f, 1.0f, 0.3f, 14000.0f, 150.0f); m[IDs::panStart] = -1.0f; m[IDs::panEnd] = 1.0f; v.push_back ({ "Pan Sweep L to R", m }); }
    return v;
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, const juce::String& subfolder)
    : apvts (s), factory (factoryPresets())
{
    folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ReverseVerb").getChildFile (subfolder);
    folder.createDirectory();
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->paramID, this);
}

PresetManager::~PresetManager()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->paramID, this);
}

juce::StringArray PresetManager::getNames()
{
    juce::StringArray names;
    for (auto& f : factory) names.add (f.name);
    userFiles = folder.findChildFiles (juce::File::findFiles, false, "*.rvpreset");
    userFiles.sort();
    for (auto& f : userFiles) names.add (f.getFileNameWithoutExtension());
    return names;
}

void PresetManager::applyValues (const std::map<juce::String, float>& v)
{
    applying = true;
    for (auto& kv : v)
        if (auto* p = apvts.getParameter (kv.first))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
            p->endChangeGesture();
        }
    applying = false;
}

bool PresetManager::load (int index)
{
    auto names = getNames();
    if (index < 0 || index >= names.size()) return false;
    if (index < (int) factory.size())
    {
        applyValues (factory[(size_t) index].values);
    }
    else
    {
        auto xml = juce::XmlDocument::parse (userFiles[index - (int) factory.size()]);
        if (xml == nullptr) return false;
        std::map<juce::String, float> v;
        for (auto* e : xml->getChildIterator())
            if (e->hasAttribute ("id")) v[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");
        applyValues (v);
    }
    current = index;
    currentName = names[index];
    return true;
}

bool PresetManager::save (const juce::String& nameIn)
{
    auto name = juce::File::createLegalFileName (nameIn.trim());
    if (name.isEmpty()) return false;
    juce::XmlElement root ("ReverseVerbPreset");
    for (auto& id : sharedParamIds())
        if (auto* p = apvts.getRawParameterValue (id))
        {
            auto* e = root.createNewChildElement ("param");
            e->setAttribute ("id", id);
            e->setAttribute ("value", (double) p->load());
        }
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! sharedParamIds().contains (rp->paramID) && rp->paramID != IDs::freeze && rp->paramID != IDs::listen)
            {
                auto* e = root.createNewChildElement ("param");
                e->setAttribute ("id", rp->paramID);
                e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
            }
    auto file = folder.getChildFile (name + ".rvpreset");
    if (! root.writeTo (file)) return false;
    auto names = getNames();
    int idx = -1;
    for (int i = (int) factory.size(); i < names.size(); ++i) if (names[i] == name) { idx = i; break; }
    current = idx;
    currentName = name;
    return true;
}

bool PresetManager::remove (int index)
{
    getNames();
    const int u = index - (int) factory.size();
    if (u < 0 || u >= userFiles.size()) return false;
    userFiles[u].deleteFile();
    current = -1; currentName = {};
    return true;
}
