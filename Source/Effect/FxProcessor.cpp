#include "FxProcessor.h"
#include "FxEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout FxProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    using A = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&] (const juce::String& id, const juce::String& name, R range, float def, const juce::String& label = {})
    { p.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name, range, def, A().withLabel (label))); };

    add (IDs::dry, "Dry", R { 0.0f, 1.0f, 0.001f }, 1.0f);
    addReverbParams (p);
    addSwellParams (p);
    add (IDs::threshold, "Threshold", R { -60.0f, 0.0f, 0.1f }, -30.0f, "dB");
    add (IDs::sens, "Sensitivity", R { 1.0f, 12.0f, 0.1f }, 3.0f, "x");
    add (IDs::hold, "Hold", R { 10.0f, 1000.0f, 1.0f, 0.5f }, 80.0f, "ms");
    add (IDs::hitLen, "Hit Length", R { 30.0f, 1000.0f, 1.0f, 0.5f }, 250.0f, "ms");
    add (IDs::maxVoices, "Max Swells", R { 1.0f, 8.0f, 1.0f }, 4.0f);
    add (IDs::swellGain, "Boost", R { -12.0f, 24.0f, 0.1f }, 0.0f, "dB");
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::gateHit, 1 }, "Gate Hit", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::followLevel, 1 }, "Follow Level", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::freeze, 1 }, "Freeze", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::listen, 1 }, "Listen (swell only)", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::trigMode, 1 }, "Trigger", juce::StringArray { "Audio", "MIDI", "Both" }, 0));
    return { p.begin(), p.end() };
}

FxProcessor::FxProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("ReverseVerbFX render"),
      apvts (*this, nullptr, "PARAMS", createLayout()), presets (apvts, "FX")
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID != IDs::dry && rp->paramID != IDs::wet && rp->paramID != IDs::listen)
                apvts.addParameterListener (rp->paramID, this);
    lastRendered = std::make_shared<RenderedSample>();
    for (auto& h : histDb) h.store (-100.0f);
    for (auto& t : histTrig) t.store (0);
    startThread (juce::Thread::Priority::normal);
    startTimer (60);
}

FxProcessor::~FxProcessor()
{
    stopTimer();
    stopThread (3000);
}

void FxProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

bool FxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void FxProcessor::prepareToPlay (double sampleRate, int)
{
    stopThread (3000);
    hostSampleRate = sampleRate;
    int len = 1;
    while (len < (int) (sampleRate * 24.0)) len <<= 1;
    ring.setSize (2, len);
    ring.clear();
    ringMask = len - 1;
    clock = 0;
    prerollSamples = (int) (sampleRate * 0.006);
    for (auto& j : jobs)
    {
        j.hit.setSize (2, (int) (sampleRate * 1.05) + prerollSamples + 64);
        j.hit.clear();
        j.state = Free;
        j.hitLen = 0;
    }
    for (auto& j : jobs) { j.play = nullptr; j.swell.reset(); j.fadeRemaining = -1; }
    capturingJob = -1;
    fastEnv = slowEnv = 0.0f;
    armed = true; lastTrigPeak = 0.0f;
    fastCoef = (float) std::exp (-1.0 / (0.008 * sampleRate));
    slowCoef = (float) (1.0 - std::exp (-1.0 / (0.2 * sampleRate)));
    lastTrig = -1000000;
    for (auto& v : voices) v.active = false;
    {
        // anything rendered at the old sample rate is invalid now
        juce::SpinLock::ScopedLockType l (renderLock);
        frozenSwell = nullptr; frozenHistory.clear(); frozenLen = 0; frozenGain = 1.0f;
        lastRendered = std::make_shared<RenderedSample>();
        renderHistory.clear();
    }
    lastFreeze = false;
    wantFreezeSnapshot = param (IDs::freeze) > 0.5f;
    updateLatency();
    startThread (juce::Thread::Priority::normal);
}

void FxProcessor::shuffleBeats (bool restore)
{
    if (restore) beatOrder.reset();
    else if (param (IDs::sync) > 0.5f) beatOrder.shuffle (kSyncBeats[juce::jlimit (0, kNumSyncChoices - 1, (int) param (IDs::syncLen))]);
}

void FxProcessor::updateLatency()
{
    const double sr = hostSampleRate;
    const int hitMax = (int) (param (IDs::hitLen) * 0.001 * sr) + prerollSamples;
    const int gapLen = (int) (param (IDs::gap) * 0.001 * sr);
    double tailSec = param (IDs::tail);
    if (param (IDs::sync) > 0.5f)
        tailSec = kSyncBeats[juce::jlimit (0, kNumSyncChoices - 1, (int) param (IDs::syncLen))] * 60.0 / juce::jmax (20.0, hostBpm.load());
    const float pitchAmt = param (IDs::pitch);
    const float oct = kPitchOct[juce::jlimit (0, 2, (int) param (IDs::pitchRange))];
    const double stretch = pitchAmt < 0.0f ? std::pow (2.0, oct * -pitchAmt) : 1.0;
    const double tstretch = juce::jmax (1.0, (double) param (IDs::stretch));
    double swellMax = (tailSec * sr + hitMax * tstretch + gapLen) * stretch;
    swellMax = juce::jmin (swellMax, 20.0 * sr);
    {
        juce::SpinLock::ScopedLockType l (renderLock);
        if (param (IDs::freeze) > 0.5f && frozenSwell != nullptr) swellMax = frozenLen;   // frozen: budget follows the frozen buffer, not the knobs
    }
    const int margin = (int) (0.12 * sr);
    int L = (int) swellMax + hitMax + margin;
    const int q = (int) (0.1 * sr);
    L = ((L + q - 1) / q) * q;
    L = juce::jmin (L, ringMask + 1 - 16384);
    maxSwellSamples = juce::jmax (1024, L - hitMax - margin);
    if (L != latency.load())
    {
        latency = L;
        setLatencySamples (L);
    }
}

// ---------------- render thread ----------------

void FxProcessor::run()
{
    while (! threadShouldExit())
    {
        bool did = false;
        for (auto& j : jobs)
            if (j.state.load() == Captured)
            {
                j.state = Rendering;
                renderJob (j);
                j.state = Ready;
                did = true;
            }
        if (wantFreezeSnapshot.exchange (false))
        {
            std::shared_ptr<const RenderedSample> r;
            { juce::SpinLock::ScopedLockType l (renderLock); r = lastRendered; }
            if (r != nullptr && r->hitIndex > 0)
            {
                auto buf = std::make_shared<juce::AudioBuffer<float>> (2, r->hitIndex);
                for (int ch = 0; ch < 2; ++ch) buf->copyFrom (ch, 0, r->audio, ch, 0, r->hitIndex);
                juce::SpinLock::ScopedLockType l (renderLock);
                frozenSwell = buf; frozenLen = r->hitIndex; frozenGain = 1.0f;
                frozenHistory.push_back (buf);
            }
            did = true;
        }
        if (! did) wait (10);
    }
}

void FxProcessor::renderJob (Job& j)
{
    const double sr = hostSampleRate;
    const int n = j.hitLen;
    juce::AudioBuffer<float> hit (2, juce::jmax (1, n));
    hit.clear();
    for (int ch = 0; ch < 2; ++ch) hit.copyFrom (ch, 0, j.hit, ch, 0, n);
    hit.applyGainRamp (0, juce::jmin (n, (int) (sr * 0.002)), 0.0f, 1.0f);
    const int fo = juce::jmin (n, (int) (sr * 0.01));
    hit.applyGainRamp (n - fo, fo, 1.0f, 0.0f);
    const float hitPeak = hit.getMagnitude (0, n);

    auto meta = std::make_shared<RenderedSample>();
    juce::AudioBuffer<float> swell = renderSwell (hit, sr, *this, hostBpm.load(), maxSwellSamples.load(), *meta, &gateMask, &beatOrder);
    const int sl = swell.getNumSamples();

    float gain = juce::Decibels::decibelsToGain (param (IDs::swellGain));
    if (param (IDs::followLevel) > 0.5f) gain *= juce::jlimit (0.0f, 1.2f, hitPeak / 0.9f);

    auto buf = std::make_shared<juce::AudioBuffer<float>> (2, juce::jmax (1, sl));
    buf->clear();
    for (int ch = 0; ch < 2; ++ch) buf->copyFrom (ch, 0, swell, ch, 0, sl);

    // display copy: swell + hit
    meta->audio.setSize (2, sl + n);
    meta->audio.clear();
    for (int ch = 0; ch < 2; ++ch)
    {
        meta->audio.copyFrom (ch, 0, swell, ch, 0, sl);
        meta->audio.applyGain (ch, 0, sl, gain);
        meta->audio.copyFrom (ch, sl, hit, ch, 0, n);
    }
    meta->hitIndex = sl;

    j.swellLen = sl;
    j.gain = gain;
    j.swell = buf;          // old buffer (if any) freed here, on the render thread
    j.play = buf.get();
    {
        juce::SpinLock::ScopedLockType l (renderLock);
        renderHistory.push_back (meta);
        lastRendered = meta;
        if (param (IDs::freeze) > 0.5f && frozenSwell == nullptr) { frozenSwell = buf; frozenLen = sl; frozenGain = gain; frozenHistory.push_back (buf); }
    }
    ++captured;
}

// ---------------- audio thread ----------------

void FxProcessor::startCapture (juce::int64 c, int prerollAvail)
{
    int slot = -1;
    for (int i = 0; i < numJobs; ++i) if (jobs[(size_t) i].state.load() == Free) { slot = i; break; }
    if (slot < 0) { ++dropped; return; }
    auto& j = jobs[(size_t) slot];
    j.captureStart = c - prerollAvail;
    j.hitLen = 0;
    j.peakEnv = 0.0f;
    j.frozen = false;
    for (int k = 0; k < prerollAvail; ++k)
    {
        const int idx = (int) ((c - prerollAvail + k) & ringMask);
        j.hit.setSample (0, j.hitLen, ring.getSample (0, idx));
        j.hit.setSample (1, j.hitLen, ring.getSample (1, idx));
        ++j.hitLen;
    }
    j.state = Capturing;
    capturingJob = slot;
}

void FxProcessor::finishCapture()
{
    if (capturingJob < 0) return;
    auto& j = jobs[(size_t) capturingJob];
    const int minLen = (int) (hostSampleRate * 0.01);
    if (j.hitLen >= minLen) { j.state = Captured; notify(); }
    else j.state = Free;
    capturingJob = -1;
}

void FxProcessor::startAudition (float gain, const RenderedSample* r)
{
    if (r == nullptr || r->audio.getNumSamples() == 0) return;
    Voice* t = nullptr;
    for (auto& v : voices) if (! v.active) { t = &v; break; }
    if (t == nullptr) { t = &voices[0]; for (auto& v : voices) if (v.id < t->id) t = &v; }
    t->active = true; t->pos = 0; t->gain = gain; t->id = ++voiceCounter; t->render = r;
}

void FxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0 || ringMask == 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0) hostBpm = *bpm;

    const int L = latency.load();
    const float dry = param (IDs::listen) > 0.5f ? 0.0f : param (IDs::dry);
    const float wet = param (IDs::wet);
    const int mode = (int) param (IDs::trigMode);
    const bool audioTrig = mode != 1, midiTrig = mode != 0;
    const float thr = juce::Decibels::decibelsToGain (param (IDs::threshold));
    const float sens = param (IDs::sens);
    const juce::int64 holdSamples = (juce::int64) (param (IDs::hold) * 0.001 * hostSampleRate);
    const int hitMax = (int) (param (IDs::hitLen) * 0.001 * hostSampleRate) + prerollSamples;
    const bool gate = param (IDs::gateHit) > 0.5f;
    const bool freeze = param (IDs::freeze) > 0.5f;
    const int maxV = juce::jlimit (1, numJobs, (int) param (IDs::maxVoices));

    // MIDI / manual triggers
    int trigPos[64]; int numTrig = 0;
    if (manualRequest.exchange (0) != 0) trigPos[numTrig++] = 0;
    if (midiTrig)
        for (const auto meta : midi)
            if (meta.getMessage().isNoteOn() && numTrig < 64) trigPos[numTrig++] = juce::jlimit (0, numSamples - 1, meta.samplePosition);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numCh > 1 ? 1 : 0);
    auto* rL = ring.getWritePointer (0);
    auto* rR = ring.getWritePointer (1);


    // ---- input pass: ring write, detection, capture ----
    for (int n = 0; n < numSamples; ++n)
    {
        const juce::int64 c = clock + n;
        const float l = inL[n], r = inR[n];
        const int wi = (int) (c & ringMask);
        rL[wi] = l; rR[wi] = r;

        const float mono = juce::jmax (std::abs (l), std::abs (r));
        fastEnv = juce::jmax (mono, fastEnv * fastCoef);
        const float slowPrev = slowEnv;
        slowEnv += (fastEnv - slowEnv) * slowCoef;

        bool trig = false;
        for (int t = 0; t < numTrig; ++t) if (trigPos[t] == n) trig = true;
        if (! armed && fastEnv < juce::jmax (thr * 0.7f, lastTrigPeak * 0.35f)) armed = true;   // hysteresis: level must drop before re-arming
        if (audioTrig && armed && fastEnv > thr && fastEnv > slowPrev * sens) trig = true;
        if (trig && c - lastTrig < holdSamples) trig = false;

        if (trig)
        {
            lastTrig = c;
            armed = false; lastTrigPeak = fastEnv;
            lastTriggerClock = c;
            histTrigFlag = true;
            finishCapture();
            if (freeze)
            {
                bool usedFrozen = false;
                {
                    juce::SpinLock::ScopedLockType lk (renderLock);   // held while we point a job at the frozen buffer
                    if (frozenSwell != nullptr)
                    {
                        usedFrozen = true;
                        int slot = -1;
                        for (int i = 0; i < numJobs; ++i) if (jobs[(size_t) i].state.load() == Free) { slot = i; break; }
                        if (slot >= 0)
                        {
                            auto& j = jobs[(size_t) slot];
                            j.captureStart = c - juce::jmin<juce::int64> (prerollSamples, c);
                            j.play = frozenSwell.get(); j.swellLen = frozenLen; j.gain = frozenGain; j.frozen = true;
                            j.state = Ready;
                        }
                        else ++dropped;
                    }
                }
                if (! usedFrozen) startCapture (c, (int) juce::jmin<juce::int64> (prerollSamples, c));
            }
            else startCapture (c, (int) juce::jmin<juce::int64> (prerollSamples, c));
        }

        if (capturingJob >= 0)
        {
            auto& j = jobs[(size_t) capturingJob];
            j.hit.setSample (0, j.hitLen, l);
            j.hit.setSample (1, j.hitLen, r);
            ++j.hitLen;
            j.peakEnv = juce::jmax (j.peakEnv, fastEnv);
            lastTrigPeak = juce::jmax (lastTrigPeak, fastEnv);
            const bool gated = gate && j.hitLen > (int) (hostSampleRate * 0.03) && fastEnv < j.peakEnv * 0.02f;
            if (j.hitLen >= hitMax || gated || j.hitLen >= j.hit.getNumSamples() - 1) finishCapture();
        }

        histMax = juce::jmax (histMax, fastEnv);
        if (++histAccum >= 256)
        {
            const int w = histWrite.load();
            histDb[(size_t) w].store (juce::Decibels::gainToDecibels (histMax, -100.0f));
            histTrig[(size_t) w].store (histTrigFlag ? 1 : 0);
            histWrite = (w + 1) % historyLen;
            histAccum = 0; histMax = 0.0f; histTrigFlag = false;
        }
    }
    inputDb = juce::Decibels::gainToDecibels (fastEnv, -100.0f);

    // ---- output: delayed dry ----
    auto* oL = buffer.getWritePointer (0);
    auto* oR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int n = 0; n < numSamples; ++n)
    {
        const int ri = (int) ((clock + n - L) & ringMask);
        const float dl = (clock + n - L) >= 0 ? rL[ri] * dry : 0.0f;
        const float dr = (clock + n - L) >= 0 ? rR[ri] * dry : 0.0f;
        oL[n] = oR != nullptr ? dl : (dl + dr) * 0.5f;
        if (oR != nullptr) oR[n] = dr;
    }

    // ---- schedule / play swells ----
    int active = 0;
    for (auto& j : jobs) if (j.state.load() == Playing) ++active;
    for (auto& j : jobs)
    {
        if (j.state.load() != Ready) continue;
        j.playStart = j.captureStart + L - j.swellLen;
        if (clock >= j.playStart + j.swellLen) { j.state = Free; ++missed; continue; }
        if (j.playStart < clock + numSamples)
        {
            if (active >= maxV)
            {
                Job* oldest = nullptr;
                for (auto& o : jobs) if (o.state.load() == Playing && o.fadeRemaining < 0 && (oldest == nullptr || o.captureStart < oldest->captureStart)) oldest = &o;
                if (oldest != nullptr) { oldest->fadeRemaining = (int) (hostSampleRate * 0.008); --active; }
            }
            j.playPos = (int) juce::jmax<juce::int64> (0, clock - j.playStart);
            j.fadeRemaining = -1;
            j.state = Playing;
            ++active;
        }
    }
    for (auto& j : jobs)
    {
        if (j.state.load() != Playing || j.play == nullptr) continue;
        const int offset = (int) juce::jmax<juce::int64> (0, j.playStart - clock);
        const float g = j.gain * wet;
        const float* sL = j.play->getReadPointer (0);
        const float* sR = j.play->getReadPointer (1);
        int pos = j.playPos;
        const int fadeLen = (int) (hostSampleRate * 0.008);
        for (int n = offset; n < numSamples && pos < j.swellLen; ++n, ++pos)
        {
            float f = g;
            if (j.fadeRemaining >= 0) { f *= (float) j.fadeRemaining / (float) juce::jmax (1, fadeLen); if (j.fadeRemaining > 0) --j.fadeRemaining; }
            if (oR != nullptr) { oL[n] += sL[pos] * f; oR[n] += sR[pos] * f; }
            else oL[n] += (sL[pos] + sR[pos]) * 0.5f * f;
            if (j.fadeRemaining == 0) { pos = j.swellLen; break; }
        }
        j.playPos = pos;
        if (pos >= j.swellLen) { j.state = Free; j.fadeRemaining = -1; }
    }
    active = 0;
    for (auto& j : jobs) if (j.state.load() == Playing) ++active;
    activeCount = active;

    // ---- audition (click on waveform) ----
    const RenderedSample* current = nullptr;
    {
        juce::SpinLock::ScopedTryLockType tl (renderLock);
        if (tl.isLocked()) current = lastRendered.get();
    }
    if (auditionRequest.exchange (0) != 0) startAudition (1.0f, current);
    {
        const float hitGain = param (IDs::listen) > 0.5f ? 0.0f : 1.0f;
        for (auto& v : voices)
        {
            if (! v.active || v.render == nullptr) continue;
            const auto& rs = *v.render;
            const int total = rs.audio.getNumSamples();
            const int hitAt = rs.hitIndex >= 0 ? rs.hitIndex : total;
            const float* aL = rs.audio.getReadPointer (0);
            const float* aR = rs.audio.getReadPointer (1);
            int pos = v.pos;
            for (int n = 0; n < numSamples && pos < total; ++n, ++pos)
            {
                const float g = (pos < hitAt ? wet : hitGain) * v.gain;
                if (oR != nullptr) { oL[n] += aL[pos] * g; oR[n] += aR[pos] * g; }
                else oL[n] += (aL[pos] + aR[pos]) * 0.5f * g;
            }
            v.pos = pos;
            if (pos >= total) v.active = false;
        }
        const Voice* newest = nullptr;
        for (auto& v : voices) if (v.active && (newest == nullptr || v.id > newest->id)) newest = &v;
        playhead = newest != nullptr ? newest->pos : -1;
    }
    for (int n = 0; n < numSamples; ++n) { oL[n] = softClip (oL[n]); if (oR != nullptr) oR[n] = softClip (oR[n]); }

    clock += numSamples;
    clockShared = clock;
}

// ---------------- misc ----------------

std::shared_ptr<const RenderedSample> FxProcessor::getRendered() const
{
    juce::SpinLock::ScopedLockType l (renderLock);
    return lastRendered;
}

FxProcessor::Stats FxProcessor::getStats() const
{
    Stats s;
    s.latency = latency.load(); s.captured = captured.load(); s.missed = missed.load(); s.dropped = dropped.load();
    s.active = activeCount.load(); s.lastTrigger = lastTriggerClock.load(); s.clock = clockShared.load(); s.inputDb = inputDb.load();
    return s;
}

void FxProcessor::copyHistory (std::vector<float>& db, std::vector<unsigned char>& trig) const
{
    db.resize (historyLen); trig.resize (historyLen);
    const int w = histWrite.load();
    for (int i = 0; i < historyLen; ++i)
    {
        const int idx = (w + i) % historyLen;
        db[(size_t) i] = histDb[(size_t) idx].load();
        trig[(size_t) i] = histTrig[(size_t) idx].load();
    }
}

void FxProcessor::timerCallback()
{
    const bool fz = param (IDs::freeze) > 0.5f;
    if (fz && ! lastFreeze) { { juce::SpinLock::ScopedLockType l (renderLock); frozenSwell = nullptr; } wantFreezeSnapshot = true; notify(); }
    if (! fz && lastFreeze) { juce::SpinLock::ScopedLockType l (renderLock); frozenSwell = nullptr; }
    lastFreeze = fz;
    {
        // prune frozen buffers no job is using any more (only ever freed here, on the message thread)
        juce::SpinLock::ScopedLockType l (renderLock);
        for (int i = (int) frozenHistory.size(); --i >= 0;)
        {
            auto* ptr = frozenHistory[(size_t) i].get();
            if (ptr == frozenSwell.get()) continue;
            bool used = false;
            for (auto& j : jobs) { const int st = j.state.load(); if ((st == Ready || st == Playing) && j.play == ptr) used = true; }
            if (! used) frozenHistory.erase (frozenHistory.begin() + i);
        }
        for (int i = (int) renderHistory.size(); --i >= 0;)
        {
            auto* ptr = renderHistory[(size_t) i].get();
            if (ptr == lastRendered.get()) continue;
            bool used = false;
            for (auto& v : voices) if (v.active && v.render == ptr) used = true;
            if (! used) renderHistory.erase (renderHistory.begin() + i);
        }
    }
    const double bpm = hostBpm.load();
    if (paramsDirty.exchange (false) || (param (IDs::sync) > 0.5f && std::abs (bpm - lastBpm) > 0.01))
    {
        lastBpm = bpm;
        updateLatency();
    }
}

bool FxProcessor::exportWav (const juce::File& dest)
{
    auto r = getRendered();
    if (r == nullptr || r->audio.getNumSamples() == 0) return false;
    const int n = r->audio.getNumSamples();
    const int hitAt = r->hitIndex >= 0 ? r->hitIndex : n;
    juce::AudioBuffer<float> mix;
    mix.makeCopyOf (r->audio);
    for (int ch = 0; ch < 2; ++ch) mix.applyGain (ch, 0, hitAt, param (IDs::wet));
    return writeWavSafely (dest, mix, r->sampleRate);
}

void FxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("gateMask", gateMask.toString(), nullptr);
    state.setProperty ("beatOrder", beatOrder.toString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void FxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            apvts.replaceState (state);
            gateMask.fromString (state.getProperty ("gateMask", "").toString());
            beatOrder.fromString (state.getProperty ("beatOrder", "").toString());
            paramsDirty = true;
        }
    }
}

juce::AudioProcessorEditor* FxProcessor::createEditor() { return new FxEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FxProcessor(); }
