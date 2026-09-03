#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---------------- Freeverb-style reverb with size / decay / damp / diffusion / separation / width / ER ----------------
namespace
{
    struct Comb
    {
        std::vector<float> buf; int idx = 0; float store = 0, fb = 0, d1 = 0, d2 = 1;
        void setup (int n, float feedback, float damp) { buf.assign ((size_t) juce::jmax (1, n), 0.0f); idx = 0; store = 0; fb = feedback; d1 = damp; d2 = 1.0f - damp; }
        float process (float in)
        {
            const float out = buf[(size_t) idx];
            store = out * d2 + store * d1;
            buf[(size_t) idx] = in + store * fb;
            if (++idx >= (int) buf.size()) idx = 0;
            return out;
        }
    };
    struct Allpass
    {
        std::vector<float> buf; int idx = 0; float g = 0.5f;
        void setup (int n, float gain) { buf.assign ((size_t) juce::jmax (1, n), 0.0f); idx = 0; g = gain; }
        float process (float in)
        {
            const float b = buf[(size_t) idx];
            const float out = -in + b;
            buf[(size_t) idx] = in + b * g;
            if (++idx >= (int) buf.size()) idx = 0;
            return out;
        }
    };

    struct ReverbEngine
    {
        std::array<Comb, 8> combL, combR;
        std::array<Allpass, 4> apL, apR;
        std::vector<float> erBufL, erBufR; int erIdx = 0;
        std::array<int, 8> erTaps {};
        std::array<float, 8> erGain {};
        float wet1 = 1, wet2 = 0, erLevel = 0;

        void setup (double sr, float size, float decay, float damp, float diff, float sep, float width, float er)
        {
            const int combBase[8]  = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
            const int apBase[4]    = { 556, 441, 341, 225 };
            const float scale      = (float) (sr / 44100.0) * (0.45f + 1.3f * size);
            const int spread       = (int) (sep * 60.0f * sr / 44100.0);
            const float feedback   = 0.62f + 0.36f * decay;
            const float apGain     = 0.15f + 0.6f * diff;
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

        void process (float* l, float* r, int n)
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
    };

    const int kSyncBeats[] = { 1, 2, 4, 8, 4, 8, 16 };   // 1,2,4,8 beats / 1,2,4 bars
    const float kPitchOct[] = { 1.0f, 2.0f, 4.0f };

    bool stateContainsParameter (const juce::ValueTree& state, const juce::String& parameterId)
    {
        if (state.hasProperty (parameterId)
            || state.getProperty ("id").toString() == parameterId)
            return true;

        for (int child = 0; child < state.getNumChildren(); ++child)
            if (stateContainsParameter (state.getChild (child), parameterId))
                return true;

        return false;
    }
}

// ---------------- parameters ----------------

juce::AudioProcessorValueTreeState::ParameterLayout ReverseVerbProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    using A = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&] (const juce::String& id, const juce::String& name, R range, float def, const juce::String& label = {})
    {
        p.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name, range, def, A().withLabel (label)));
    };
    add (IDs::dry,   "Hit",    R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::wet,   "Swell",  R { 0.0f, 1.0f, 0.001f }, 0.8f);
    add (IDs::size,  "Size",   R { 0.0f, 1.0f, 0.001f }, 0.7f);
    add (IDs::decay, "Decay",  R { 0.0f, 1.0f, 0.001f }, 0.8f);
    add (IDs::damp,  "Damp",   R { 0.0f, 1.0f, 0.001f }, 0.4f);
    add (IDs::diff,  "Diffusion", R { 0.0f, 1.0f, 0.001f }, 0.6f);
    add (IDs::er,    "Early Ref", R { 0.0f, 1.0f, 0.001f }, 0.3f);
    add (IDs::sep,   "Separation", R { 0.0f, 1.0f, 0.001f }, 0.4f);
    add (IDs::width, "Width",  R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::gap,   "Delay",  R { 0.0f, 500.0f, 1.0f }, 0.0f, "ms");
    add (IDs::tail,  "Length", R { 0.1f, 8.0f, 0.01f, 0.5f }, 1.2f, "s");
    add (IDs::shape, "Shape",  R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::tone,  "Color",  R { 500.0f, 20000.0f, 1.0f, 0.3f }, 20000.0f, "Hz");
    add (IDs::basscut, "Bass Cut", R { 20.0f, 2000.0f, 1.0f, 0.3f }, 20.0f, "Hz");
    add (IDs::trimStart, "Trim Start", R { 0.0f, 1.0f, 0.0001f }, 0.0f);
    add (IDs::trimEnd,   "Trim End",   R { 0.0f, 1.0f, 0.0001f }, 1.0f);
    add (IDs::pitch,        "Pitch",         R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::pitchTension, "Pitch Tension", R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::volStart,     "Vol Start",     R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::volEnd,       "Vol End",       R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::volTension,   "Vol Tension",   R { -1.0f, 1.0f, 0.001f }, 0.0f);
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::align, 1 }, "Hit on note (PDC)", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::sync, 1 }, "Sync to BPM", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::direction, 1 }, "Direction",
                     juce::StringArray { "Rise", "Fall" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::syncLen, 1 }, "Sync Length",
                     juce::StringArray { "1 beat", "2 beats", "4 beats", "8 beats", "1 bar", "2 bars", "4 bars" }, 2));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::pitchRange, 1 }, "Pitch Range",
                     juce::StringArray { "1 oct", "2 oct", "4 oct" }, 0));
    return { p.begin(), p.end() };
}

ReverseVerbProcessor::ReverseVerbProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();
    for (auto* id : { &IDs::size, &IDs::decay, &IDs::damp, &IDs::diff, &IDs::er, &IDs::sep, &IDs::width, &IDs::gap,
                      &IDs::tail, &IDs::shape, &IDs::tone, &IDs::basscut, &IDs::align, &IDs::trimStart, &IDs::trimEnd,
                      &IDs::sync, &IDs::syncLen, &IDs::pitch, &IDs::pitchRange, &IDs::pitchTension,
                      &IDs::volStart, &IDs::volEnd, &IDs::volTension, &IDs::direction })
        apvts.addParameterListener (*id, this);
    dryParam = apvts.getRawParameterValue (IDs::dry);
    wetParam = apvts.getRawParameterValue (IDs::wet);
    rendered = std::make_shared<RenderedSample>();
    startTimer (60);
}

ReverseVerbProcessor::~ReverseVerbProcessor() { stopTimer(); }

RenderDirection ReverseVerbProcessor::getDirection() const noexcept
{
    return param (IDs::direction) >= 0.5f ? RenderDirection::fall : RenderDirection::rise;
}

void ReverseVerbProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void ReverseVerbProcessor::resetEdits()
{
    setParam (IDs::trimStart, 0.0f);  setParam (IDs::trimEnd, 1.0f);
    setParam (IDs::pitch, 0.0f);      setParam (IDs::pitchTension, 0.0f);
    setParam (IDs::volStart, 1.0f);   setParam (IDs::volEnd, 1.0f);  setParam (IDs::volTension, 0.0f);
}

void ReverseVerbProcessor::randomizeReverb()
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

void ReverseVerbProcessor::prepareToPlay (double sampleRate, int)
{
    if (std::abs (sampleRate - hostSampleRate) > 0.5) dirty = true;
    hostSampleRate = sampleRate;
    for (auto& v : voices) v.active = false;
    playhead = -1;
}

bool ReverseVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

// ---------------- playback ----------------

void ReverseVerbProcessor::startVoice (float gain)
{
    Voice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr) { target = &voices[0]; for (auto& v : voices) if (v.id < target->id) target = &v; }
    target->active = true; target->pos = 0; target->gain = gain; target->id = ++voiceCounter;
}

void ReverseVerbProcessor::renderRange (juce::AudioBuffer<float>& out, const RenderedSample& r, int start, int num, float dry, float wet)
{
    if (num <= 0) return;
    const int total = r.getNumSamples();
    for (auto& v : voices)
    {
        if (! v.active) continue;
        mixRenderedRange (out, r, v.pos, start, num, wet * v.gain, dry * v.gain);
        v.pos += num;
        if (v.pos >= total) v.active = false;
    }
}

void ReverseVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0) hostBpm = *bpm;

    const auto r = std::atomic_load_explicit (&rendered, std::memory_order_acquire);
    if (r == nullptr || r->getNumSamples() == 0) { playhead = -1; return; }

    if (stopRequest.exchange (0) != 0) for (auto& v : voices) v.active = false;
    if (triggerRequest.exchange (0) != 0) startVoice (1.0f);

    const float dry = dryParam->load(), wet = wetParam->load();
    const int numSamples = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        renderRange (buffer, *r, pos, at - pos, dry, wet);
        pos = at;
        if (msg.isNoteOn()) startVoice (msg.getFloatVelocity());
    }
    renderRange (buffer, *r, pos, numSamples - pos, dry, wet);

    const Voice* newest = nullptr;
    for (auto& v : voices) if (v.active && (newest == nullptr || v.id > newest->id)) newest = &v;
    playhead = newest != nullptr ? newest->pos : -1;
}

std::shared_ptr<const RenderedSample> ReverseVerbProcessor::getRendered() const
{
    return std::atomic_load_explicit (&rendered, std::memory_order_acquire);
}

void ReverseVerbProcessor::timerCallback()
{
    if (param (IDs::sync) > 0.5f && std::abs (hostBpm.load() - lastRenderBpm) > 0.01) dirty = true;
    if (dirty.exchange (false)) render();
    if (previewAfterRender.exchange (false)) triggerPreview();
}

// ---------------- render ----------------

void ReverseVerbProcessor::render()
{
    juce::AudioBuffer<float> src; double srcSR;
    { const juce::ScopedLock sl (sourceLock); src.makeCopyOf (sourceBuffer); srcSR = sourceSR; }

    auto out = std::make_shared<RenderedSample>();
    const double sr = hostSampleRate;
    const double bpm = hostBpm.load();
    const auto direction = getDirection();
    lastRenderBpm = bpm;
    out->sampleRate = sr;
    out->direction = direction;

    if (src.getNumSamples() > 0 && srcSR > 0)
    {
        // 1. resample hit to host rate, stereo
        const int srcLen = src.getNumSamples();
        juce::AudioBuffer<float> padded (src.getNumChannels(), srcLen + 16);
        padded.clear();
        for (int ch = 0; ch < src.getNumChannels(); ++ch) padded.copyFrom (ch, 0, src, ch, 0, srcLen);
        const double ratio = srcSR / sr;
        const int hitLen = juce::jmax (1, (int) std::floor (srcLen / ratio));
        juce::AudioBuffer<float> hit (2, hitLen);
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, padded.getReadPointer (juce::jmin (ch, padded.getNumChannels() - 1)), hit.getWritePointer (ch), hitLen);
        }
        const float hitMag = hit.getMagnitude (0, hitLen);
        if (hitMag > 0.0f) hit.applyGain (0.9f / hitMag);

        // 2. tail length (free or synced to BPM)
        const int gapLen = (int) (param (IDs::gap) * 0.001f * sr);
        double tailSec = param (IDs::tail);
        const bool sync = param (IDs::sync) > 0.5f;
        int beats = 0;
        if (sync)
        {
            const int choice = juce::jlimit (0, 6, (int) param (IDs::syncLen));
            beats = kSyncBeats[choice];
            tailSec = juce::jmax (0.05, beats * 60.0 / bpm - hitLen / sr - gapLen / sr);
        }
        const int tailLen = (int) (tailSec * sr);
        const int revLen  = hitLen + tailLen;

        // 3. reverb
        juce::AudioBuffer<float> rev (2, revLen);
        rev.clear();
        for (int ch = 0; ch < 2; ++ch) rev.copyFrom (ch, 0, hit, ch, 0, hitLen);
        ReverbEngine engine;
        engine.setup (sr, param (IDs::size), param (IDs::decay), param (IDs::damp), param (IDs::diff), param (IDs::sep), param (IDs::width), param (IDs::er));
        engine.process (rev.getWritePointer (0), rev.getWritePointer (1), revLen);

        // 4. Orient the wet response. The reverb engine is wet-only, so the
        // dry transient is mixed exclusively from the separate dry layer.
        if (direction == RenderDirection::rise)
            for (int ch = 0; ch < 2; ++ch) rev.reverse (ch, 0, revLen);

        // 5. filters
        auto applyIIR = [&] (const juce::IIRCoefficients& c, int passes)
        {
            for (int pass = 0; pass < passes; ++pass)
                for (int ch = 0; ch < 2; ++ch) { juce::IIRFilter f; f.setCoefficients (c); f.reset(); f.processSamples (rev.getWritePointer (ch), revLen); }
        };
        const float hp = param (IDs::basscut);
        if (hp > 21.0f) applyIIR (juce::IIRCoefficients::makeHighPass (sr, hp), 2);
        const float lp = param (IDs::tone);
        if (lp < 19900.0f) applyIIR (juce::IIRCoefficients::makeLowPass (sr, lp), 1);

        // 6. shape, fade, normalize
        const float s = param (IDs::shape);
        if (std::abs (s) > 0.001f && revLen > 1)
            for (int i = 0; i < revLen; ++i)
            {
                const float timelineProgress = (float) i / (float) (revLen - 1);
                const float towardHit = direction == RenderDirection::rise
                                          ? timelineProgress
                                          : 1.0f - timelineProgress;
                const float g = s > 0.0f ? std::pow (towardHit, 4.0f * s)
                                         : 1.0f + (-s) * 3.0f * (1.0f - towardHit);
                for (int ch = 0; ch < 2; ++ch) rev.getWritePointer (ch)[i] *= g;
            }
        const int boundaryFade = juce::jmin (revLen, (int) (sr * 0.01));
        if (direction == RenderDirection::rise)
            rev.applyGainRamp (0, boundaryFade, 0.0f, 1.0f);
        else
            rev.applyGainRamp (revLen - boundaryFade, boundaryFade, 1.0f, 0.0f);
        const float revMag = rev.getMagnitude (0, revLen);
        if (revMag > 0.0f) rev.applyGain (0.9f / revMag);

        // 7. Place wet and dry on one aligned timeline. Keeping these layers
        // separate prevents the mix controls from relying on timeline guesses.
        const auto timeline = calculateLayerTimeline (direction, revLen, hitLen, gapLen);
        const int fullLen = timeline.totalLength;
        juce::AudioBuffer<float> fullWet (2, fullLen);
        juce::AudioBuffer<float> fullDry (2, fullLen);
        fullWet.clear();
        fullDry.clear();
        for (int ch = 0; ch < 2; ++ch)
        {
            fullWet.copyFrom (ch, timeline.wetStart, rev, ch, 0, revLen);
            fullDry.copyFrom (ch, timeline.dryStart, hit, ch, 0, hitLen);
        }
        out->fullLengthSec = fullLen / sr;
        out->beats = beats;

        // 8. trim
        int tStart = (int) (param (IDs::trimStart) * fullLen);
        int tEnd   = (int) (param (IDs::trimEnd) * fullLen);
        tStart = juce::jlimit (0, fullLen - 1, tStart);
        tEnd   = juce::jlimit (tStart + (int) (sr * 0.02), fullLen, tEnd);
        const int trimLen = tEnd - tStart;
        out->trimStartSec = tStart / sr;
        out->trimEndSec   = tEnd / sr;
        int hitIdx = timeline.dryStart - tStart;
        if (hitIdx < 0 || hitIdx >= trimLen) hitIdx = -1;
        int dryInputStart = juce::jmax (0, timeline.dryStart - tStart);
        int dryInputEnd = juce::jmin (trimLen, timeline.dryStart + hitLen - tStart);
        if (dryInputEnd <= dryInputStart) dryInputStart = dryInputEnd = -1;
        int wetInputStart = juce::jmax (0, timeline.wetStart - tStart);
        int wetInputEnd = juce::jmin (trimLen, timeline.wetStart + revLen - tStart);
        if (wetInputEnd <= wetInputStart) wetInputStart = wetInputEnd = -1;

        // 9. pitch sweep (varispeed)
        const float pitchAmt = param (IDs::pitch);
        const float octaves = kPitchOct[juce::jlimit (0, 2, (int) param (IDs::pitchRange))];
        const float pitchT = param (IDs::pitchTension);
        juce::AudioBuffer<float> wetBuffer;
        juce::AudioBuffer<float> dryBuffer;
        std::vector<float> semiPerSample;
        int hitOut = -1;
        int dryOutStart = -1, dryOutEnd = -1;
        int wetOutStart = -1, wetOutEnd = -1;
        if (std::abs (pitchAmt) > 0.001f)
        {
            std::array<std::vector<float>, 2> wetSamples, drySamples;
            for (auto& samples : wetSamples) samples.reserve ((size_t) trimLen * 2);
            for (auto& samples : drySamples) samples.reserve ((size_t) trimLen * 2);
            double p = 0.0;
            while (p < trimLen - 1 && semiPerSample.size() < (size_t) (sr * 60.0))
            {
                const int i0 = (int) p; const float frac = (float) (p - i0);
                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto* wet = fullWet.getReadPointer (ch) + tStart;
                    const auto* dry = fullDry.getReadPointer (ch) + tStart;
                    wetSamples[(size_t) ch].push_back (wet[i0] + (wet[i0 + 1] - wet[i0]) * frac);
                    drySamples[(size_t) ch].push_back (dry[i0] + (dry[i0 + 1] - dry[i0]) * frac);
                }
                const float semis = pitchAmt * octaves * 12.0f * tensionCurve ((float) p / (float) trimLen, pitchT);
                semiPerSample.push_back (semis);
                const int outputIndex = (int) semiPerSample.size() - 1;
                if (hitIdx >= 0 && hitOut < 0 && p >= hitIdx) hitOut = outputIndex;
                if (dryInputStart >= 0 && p >= dryInputStart && p < dryInputEnd)
                {
                    if (dryOutStart < 0) dryOutStart = outputIndex;
                    dryOutEnd = outputIndex + 1;
                }
                if (wetInputStart >= 0 && p >= wetInputStart && p < wetInputEnd)
                {
                    if (wetOutStart < 0) wetOutStart = outputIndex;
                    wetOutEnd = outputIndex + 1;
                }
                p += std::pow (2.0, semis / 12.0);
            }
            const int outputLength = (int) semiPerSample.size();
            wetBuffer.setSize (2, outputLength);
            dryBuffer.setSize (2, outputLength);
            for (int ch = 0; ch < 2; ++ch)
            {
                wetBuffer.copyFrom (ch, 0, wetSamples[(size_t) ch].data(), outputLength);
                dryBuffer.copyFrom (ch, 0, drySamples[(size_t) ch].data(), outputLength);
            }
        }
        else
        {
            wetBuffer.setSize (2, trimLen);
            dryBuffer.setSize (2, trimLen);
            for (int ch = 0; ch < 2; ++ch)
            {
                wetBuffer.copyFrom (ch, 0, fullWet, ch, tStart, trimLen);
                dryBuffer.copyFrom (ch, 0, fullDry, ch, tStart, trimLen);
            }
            semiPerSample.assign ((size_t) trimLen, 0.0f);
            hitOut = hitIdx;
            dryOutStart = dryInputStart;
            dryOutEnd = dryInputEnd;
            wetOutStart = wetInputStart;
            wetOutEnd = wetInputEnd;
        }

        // 10. volume envelope
        const int n = wetBuffer.getNumSamples();
        const float v0 = param (IDs::volStart), v1 = param (IDs::volEnd), vt = param (IDs::volTension);
        const bool flatVol = std::abs (v0 - 1.0f) < 0.001f && std::abs (v1 - 1.0f) < 0.001f;
        for (int i = 0; i < n; ++i)
        {
            float g = 1.0f;
            if (! flatVol)
            {
                const float lvl = v0 + (v1 - v0) * tensionCurve ((float) i / (float) juce::jmax (1, n - 1), vt);
                g = lvl * lvl;
                for (int ch = 0; ch < 2; ++ch)
                {
                    wetBuffer.getWritePointer (ch)[i] *= g;
                    dryBuffer.getWritePointer (ch)[i] *= g;
                }
            }
            if (i % RenderedSample::envStep == 0) { out->gainLin.push_back (g); out->pitchSemi.push_back (semiPerSample[(size_t) i]); }
        }

        out->wetAudio = std::move (wetBuffer);
        out->dryAudio = std::move (dryBuffer);
        out->displayAudio.setSize (2, n);
        out->displayAudio.clear();
        mixRenderedRange (out->displayAudio, *out, 0, 0, n, 1.0f, 1.0f);
        out->dryHitIndex = hitOut;
        out->dryStart = dryOutStart;
        out->dryEnd = dryOutEnd;
        out->wetStart = wetOutStart;
        out->wetEnd = wetOutEnd;
    }

    const int latency = latencySamplesFor (*out, param (IDs::align) > 0.5f);
    std::shared_ptr<const RenderedSample> immutableOut = std::move (out);
    std::atomic_store_explicit (&rendered, std::move (immutableOut), std::memory_order_release);
    setLatencySamples (latency);
}

// ---------------- samples ----------------

void ReverseVerbProcessor::refreshFolderList (const juce::File& f)
{
    auto dir = f.getParentDirectory();
    if (folderFiles.isEmpty() || folderFiles[0].getParentDirectory() != dir)
    {
        folderFiles = dir.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        folderFiles.sort();
    }
    currentIndex = folderFiles.indexOf (f);
}

bool ReverseVerbProcessor::loadSampleFile (const juce::File& f, bool previewAfter)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples,
                                                 (juce::int64) (reader->sampleRate * 10.0));
    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);
    { const juce::ScopedLock sl (sourceLock); sourceBuffer = std::move (buf); sourceSR = reader->sampleRate; }
    currentFile = f;
    refreshFolderList (f);
    if (previewAfter) previewAfterRender = true;
    dirty = true;
    return true;
}

void ReverseVerbProcessor::nextSample()
{
    if (folderFiles.isEmpty()) return;
    for (int tries = 0; tries < folderFiles.size(); ++tries)
    {
        currentIndex = (currentIndex + 1) % folderFiles.size();
        if (loadSampleFile (folderFiles[currentIndex], true)) return;
    }
}

void ReverseVerbProcessor::prevSample()
{
    if (folderFiles.isEmpty()) return;
    for (int tries = 0; tries < folderFiles.size(); ++tries)
    {
        currentIndex = (currentIndex - 1 + folderFiles.size()) % folderFiles.size();
        if (loadSampleFile (folderFiles[currentIndex], true)) return;
    }
}

bool ReverseVerbProcessor::exportWav (const juce::File& dest)
{
    if (dirty.exchange (false)) render();
    auto r = getRendered();
    if (r == nullptr || r->getNumSamples() == 0) return false;
    const int n = r->getNumSamples();
    juce::AudioBuffer<float> mix (2, n);
    mix.clear();
    mixRenderedRange (mix, *r, 0, 0, n, wetParam->load(), dryParam->load());
    dest.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (dest.createOutputStream());
    if (os == nullptr || ! os->openedOk()) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), r->sampleRate, 2, 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    w->writeFromAudioSampleBuffer (mix, 0, n);
    return true;
}

// ---------------- state ----------------

void ReverseVerbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("schemaVersion", 2, nullptr);
    state.setProperty ("file", currentFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ReverseVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        const bool hasDirection = stateContainsParameter (state, IDs::direction);
        apvts.replaceState (state);
        if (! hasDirection)
            if (auto* direction = apvts.getParameter (IDs::direction))
                direction->setValueNotifyingHost (direction->convertTo0to1 (0.0f));
        juce::File f (state.getProperty ("file", "").toString());
        if (f.existsAsFile()) loadSampleFile (f);
        dirty = true;
    }
}

juce::AudioProcessorEditor* ReverseVerbProcessor::createEditor() { return new ReverseVerbEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverseVerbProcessor(); }
