#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout ReverseVerbProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { IDs::dry, 1 }, "Hit", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    addReverbParams (p);
    addSwellParams (p);
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::align, 1 }, "Hit on note (PDC)", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::mode, 1 }, "Mode", juce::StringArray { "Reverse Reverb", "Forward Reverb", "Dry (no reverb)" }, 0));
    return { p.begin(), p.end() };
}

ReverseVerbProcessor::ReverseVerbProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()), presets (apvts, "Instrument")
{
    formatManager.registerBasicFormats();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->paramID, this);
    dryParam = apvts.getRawParameterValue (IDs::dry);
    wetParam = apvts.getRawParameterValue (IDs::wet);
    rendered = std::make_shared<RenderedSample>();
    startTimer (60);
}

ReverseVerbProcessor::~ReverseVerbProcessor() { stopTimer(); }

void ReverseVerbProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

int ReverseVerbProcessor::currentBeats() const
{
    return param (IDs::sync) > 0.5f ? kSyncBeats[juce::jlimit (0, kNumSyncChoices - 1, (int) param (IDs::syncLen))] : 0;
}

void ReverseVerbProcessor::shuffleBeats (bool restore)
{
    if (restore) beatOrder.reset();
    else if (currentBeats() > 1) beatOrder.shuffle (currentBeats());
    dirty = true;
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

void ReverseVerbProcessor::startVoice (float gain, const RenderedSample* r)
{
    if (r == nullptr || r->audio.getNumSamples() == 0) return;
    Voice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr) { target = &voices[0]; for (auto& v : voices) if (v.id < target->id) target = &v; }
    target->active = true; target->pos = 0; target->gain = gain; target->id = ++voiceCounter; target->render = r;
}

void ReverseVerbProcessor::renderRange (juce::AudioBuffer<float>& out, int start, int num, float dry, float wet)
{
    if (num <= 0) return;
    const int numCh = out.getNumChannels();
    for (auto& v : voices)
    {
        if (! v.active || v.render == nullptr) continue;
        const RenderedSample& r = *v.render;
        const int total = r.audio.getNumSamples();
        const int hitAt = r.hitIndex >= 0 ? r.hitIndex : total;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* o = out.getWritePointer (ch) + start;
            const float* s = r.audio.getReadPointer (juce::jmin (ch, 1));
            int pos = v.pos;
            for (int i = 0; i < num && pos < total; ++i, ++pos)
                o[i] += s[pos] * (r.gainsBaked ? 1.0f : (pos < hitAt ? wet : dry)) * v.gain;
        }
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

    const RenderedSample* current = nullptr;
    {
        juce::SpinLock::ScopedTryLockType tl (renderLock);
        if (tl.isLocked()) current = rendered.get();
    }

    if (stopRequest.exchange (0) != 0) for (auto& v : voices) v.active = false;
    if (triggerRequest.exchange (0) != 0) startVoice (1.0f, current);

    const float dry = dryParam->load(), wet = wetParam->load();
    const int numSamples = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        renderRange (buffer, pos, at - pos, dry, wet);
        pos = at;
        if (msg.isNoteOn()) startVoice (msg.getFloatVelocity(), current);
    }
    renderRange (buffer, pos, numSamples - pos, dry, wet);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i) d[i] = softClip (d[i]);
    }

    const Voice* newest = nullptr;
    for (auto& v : voices) if (v.active && (newest == nullptr || v.id > newest->id)) newest = &v;
    playhead = newest != nullptr ? newest->pos : -1;
}

std::shared_ptr<const RenderedSample> ReverseVerbProcessor::getRendered() const
{
    juce::SpinLock::ScopedLockType l (renderLock);
    return rendered;
}

void ReverseVerbProcessor::timerCallback()
{
    if (param (IDs::sync) > 0.5f && std::abs (hostBpm.load() - lastRenderBpm) > 0.01) dirty = true;
    if (dirty.exchange (false)) render();
    if (previewAfterRender.exchange (false)) triggerPreview();
    // retire old renders nobody plays any more (memory is only ever freed here, on the message thread)
    juce::SpinLock::ScopedLockType l (renderLock);
    for (int i = (int) renderHistory.size(); --i >= 0;)
    {
        auto* ptr = renderHistory[(size_t) i].get();
        if (ptr == rendered.get()) continue;
        bool used = false;
        for (auto& v : voices) if (v.active && v.render == ptr) used = true;
        if (! used) renderHistory.erase (renderHistory.begin() + i);
    }
}

// ---------------- render ----------------

void ReverseVerbProcessor::render()
{
    juce::AudioBuffer<float> src; double srcSR;
    { const juce::ScopedLock sl (sourceLock); src.makeCopyOf (sourceBuffer); srcSR = sourceSR; }

    auto out = std::make_shared<RenderedSample>();
    const double sr = hostSampleRate;
    const double bpm = hostBpm.load();
    lastRenderBpm = bpm;
    out->sampleRate = sr;

    if (src.getNumSamples() > 0 && srcSR > 0)
    {
        // 1. resample hit to host rate, stereo, normalise
        const int srcLen = src.getNumSamples();
        juce::AudioBuffer<float> padded (src.getNumChannels(), srcLen + 16);
        padded.clear();
        for (int ch = 0; ch < src.getNumChannels(); ++ch) padded.copyFrom (ch, 0, src, ch, 0, srcLen);
        const double ratio = srcSR / sr;
        const int hitLen0 = juce::jmax (1, (int) std::floor (srcLen / ratio));
        juce::AudioBuffer<float> hit (2, hitLen0);
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, padded.getReadPointer (juce::jmin (ch, padded.getNumChannels() - 1)), hit.getWritePointer (ch), hitLen0);
        }
        { const float m = hit.getMagnitude (0, hitLen0); if (m > 0.0f) hit.applyGain (0.9f / m); }
        juce::AudioBuffer<float> origHit; origHit.makeCopyOf (hit);

        // 2. stretch (turns the hit into a long smooth body)
        hit = stretchHit (hit, sr, param (IDs::stretch));
        const int hitLen = hit.getNumSamples();

        const int mode = juce::jlimit (0, 2, (int) param (IDs::mode));
        const int gapLen = (int) (param (IDs::gap) * 0.001f * sr);
        const bool sync = param (IDs::sync) > 0.5f;
        const int beats = currentBeats();
        double tailSec = param (IDs::tail);
        if (sync) tailSec = juce::jlimit (0.05, 60.0, beats * 60.0 / bpm - hitLen / sr - gapLen / sr - (mode == 0 ? origHit.getNumSamples() / sr : 0.0));

        juce::AudioBuffer<float> full;
        int hitIndex = -1;

        if (mode == 2)
        {
            full.makeCopyOf (hit);
            hitIndex = 0;
            if (sync) { const int want = (int) (beats * 60.0 / bpm * sr); if (want > full.getNumSamples()) full.setSize (2, want, true, true); }
        }
        else
        {
            const int tailLen = (int) (tailSec * sr);
            const int revLen = hitLen + tailLen;
            juce::AudioBuffer<float> rev (2, revLen);
            rev.clear();
            for (int ch = 0; ch < 2; ++ch) rev.copyFrom (ch, 0, hit, ch, 0, hitLen);
            ReverbEngine engine;
            engine.setup (sr, param (IDs::size), param (IDs::decay), param (IDs::damp), param (IDs::diff), param (IDs::sep), param (IDs::width), param (IDs::er));
            engine.process (rev.getWritePointer (0), rev.getWritePointer (1), revLen);
            if (mode == 0)
            {
                applySwellFx (rev, sr, *this, true);
                for (int ch = 0; ch < 2; ++ch) rev.reverse (ch, 0, revLen);
                applySwellFx (rev, sr, *this, false);
            }
            else
            {
                applySwellFx (rev, sr, *this, false);
                applySwellFx (rev, sr, *this, true);
            }

            auto applyIIR = [&] (const juce::IIRCoefficients& c, int passes)
            {
                for (int pass = 0; pass < passes; ++pass)
                    for (int ch = 0; ch < 2; ++ch) { juce::IIRFilter f; f.setCoefficients (c); f.reset(); f.processSamples (rev.getWritePointer (ch), revLen); }
            };
            const float hp = param (IDs::basscut);
            if (hp > 21.0f) applyIIR (juce::IIRCoefficients::makeHighPass (sr, hp), 2);
            const float lp = param (IDs::tone);
            if (lp < 19900.0f) applyIIR (juce::IIRCoefficients::makeLowPass (sr, lp), 1);

            const float s = param (IDs::shape);
            if (std::abs (s) > 0.001f && revLen > 1)
                for (int i = 0; i < revLen; ++i)
                {
                    float x = (float) i / (float) (revLen - 1);
                    if (mode == 1) x = 1.0f - x;
                    const float g = s > 0.0f ? std::pow (x, 4.0f * s) : 1.0f + (-s) * 3.0f * (1.0f - x);
                    for (int ch = 0; ch < 2; ++ch) rev.getWritePointer (ch)[i] *= g;
                }
            const int fade = juce::jmin (revLen, (int) (sr * 0.01));
            if (mode == 0) rev.applyGainRamp (0, fade, 0.0f, 1.0f);
            else rev.applyGainRamp (revLen - fade, fade, 1.0f, 0.0f);
            { const float m = rev.getMagnitude (0, revLen); if (m > 0.0f) rev.applyGain (0.9f / m); }

            if (mode == 0)
            {
                const int oh = origHit.getNumSamples();
                const int swellLen = revLen + gapLen;
                full.setSize (2, swellLen + oh);
                full.clear();
                for (int ch = 0; ch < 2; ++ch) { full.copyFrom (ch, 0, rev, ch, 0, revLen); full.copyFrom (ch, swellLen, origHit, ch, 0, oh); }
                hitIndex = swellLen;
            }
            else
            {
                full.setSize (2, revLen);
                full.clear();
                for (int ch = 0; ch < 2; ++ch)
                {
                    full.addFrom (ch, 0, rev, ch, 0, revLen, wetParam->load());
                    full.addFrom (ch, 0, hit, ch, 0, hitLen, dryParam->load());
                }
                hitIndex = -1;
                out->gainsBaked = true;
            }
        }

        const int fullLen = full.getNumSamples();
        out->fullLengthSec = fullLen / sr;
        out->beats = beats;
        if (beats > 0)
        {
            applyBeatOrder (full, fullLen, beats, beatOrder, sr);
            applyBeatGate (full, fullLen, beats, gateMask, (int) param (IDs::gateShape), param (IDs::gateDepth), sr);
            out->gateMask.resize ((size_t) beats);
            for (int i = 0; i < beats; ++i) out->gateMask[(size_t) i] = gateMask.get (i) ? 1 : 0;
        }
        makeGhost (full, *out);

        // trim
        int tStart = juce::jlimit (0, fullLen - 1, (int) (param (IDs::trimStart) * fullLen));
        int tEnd   = juce::jlimit (tStart + (int) (sr * 0.02), fullLen, (int) (param (IDs::trimEnd) * fullLen));
        if (tEnd <= tStart) { tStart = 0; tEnd = fullLen; }
        const int trimLen = tEnd - tStart;
        out->trimStartSec = tStart / sr;
        out->trimEndSec   = tEnd / sr;
        int hitIdx = -1;
        if (mode == 2) hitIdx = 0;
        else if (hitIndex >= 0)
        {
            hitIdx = hitIndex - tStart;
            if (hitIdx >= trimLen) hitIdx = -1;
            else if (hitIdx < 0) hitIdx = (tStart < hitIndex + origHit.getNumSamples()) ? 0 : -1;   // trim starts inside the hit: rest is still hit
        }

        // pitch sweep
        const float pitchAmt = param (IDs::pitch);
        const float octaves = kPitchOct[juce::jlimit (0, 2, (int) param (IDs::pitchRange))];
        const float pitchT = param (IDs::pitchTension);
        juce::AudioBuffer<float> outBuf;
        std::vector<float> semi;
        int hitOut = -1;
        if (std::abs (pitchAmt) > 0.001f && trimLen > 2)
        {
            std::vector<float> l, r;
            l.reserve ((size_t) trimLen * 2); r.reserve ((size_t) trimLen * 2);
            const float* fl = full.getReadPointer (0) + tStart;
            const float* fr = full.getReadPointer (1) + tStart;
            double p = 0.0;
            while (p < trimLen - 1 && l.size() < (size_t) (sr * 120.0))
            {
                const int i0 = (int) p; const float frac = (float) (p - i0);
                l.push_back (fl[i0] + (fl[i0 + 1] - fl[i0]) * frac);
                r.push_back (fr[i0] + (fr[i0 + 1] - fr[i0]) * frac);
                const float st = pitchAmt * octaves * 12.0f * tensionCurve ((float) p / (float) trimLen, pitchT);
                semi.push_back (st);
                if (hitIdx >= 0 && hitOut < 0 && p >= hitIdx) hitOut = (int) l.size() - 1;
                p += std::pow (2.0, st / 12.0);
            }
            outBuf.setSize (2, (int) l.size());
            for (size_t i = 0; i < l.size(); ++i) { outBuf.setSample (0, (int) i, l[i]); outBuf.setSample (1, (int) i, r[i]); }
        }
        else
        {
            outBuf.setSize (2, trimLen);
            for (int ch = 0; ch < 2; ++ch) outBuf.copyFrom (ch, 0, full, ch, tStart, trimLen);
            semi.assign ((size_t) trimLen, 0.0f);
            hitOut = hitIdx;
        }

        // volume envelope
        const int n = outBuf.getNumSamples();
        const float v0 = param (IDs::volStart), v1 = param (IDs::volEnd), vt = param (IDs::volTension);
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
            if (i % RenderedSample::envStep == 0) { out->gainLin.push_back (g); out->pitchSemi.push_back (semi[(size_t) i]); }
        }
        applyPanEnvelope (outBuf, *this, &out->panPos);
        applyShelves (outBuf, sr, *this);
        finalizeOutput (outBuf, *this);

        out->audio = std::move (outBuf);
        out->hitIndex = hitOut;
    }

    const int latency = out->hitIndex > 0 ? out->hitIndex : 0;
    tailSeconds = out->audio.getNumSamples() / sr;
    { juce::SpinLock::ScopedLockType l (renderLock); renderHistory.push_back (out); rendered = out; }
    setLatencySamples (param (IDs::align) > 0.5f ? latency : 0);
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
    const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 10.0));
    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);
    { const juce::ScopedLock sl (sourceLock); sourceBuffer = std::move (buf); sourceSR = reader->sampleRate; }
    currentFile = f;
    generatedName = {}; generatedType = -1;
    refreshFolderList (f);
    if (previewAfter) previewAfterRender = true;
    dirty = true;
    return true;
}

void ReverseVerbProcessor::generate (int type)
{
    static const char* names[] = { "generated hat", "generated snare", "generated clap", "generated kick", "generated rim" };
    type = juce::jlimit (0, 4, type);
    auto buf = generateHit (type, 44100.0);
    { const juce::ScopedLock sl (sourceLock); sourceBuffer = std::move (buf); sourceSR = 44100.0; }
    currentFile = juce::File();
    generatedName = names[type];
    generatedType = type;
    previewAfterRender = true;
    dirty = true;
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
    if (r == nullptr || r->audio.getNumSamples() == 0) return false;
    const int n = r->audio.getNumSamples();
    const int hitAt = r->hitIndex >= 0 ? r->hitIndex : n;
    juce::AudioBuffer<float> mix;
    mix.makeCopyOf (r->audio);
    if (! r->gainsBaked)
        for (int ch = 0; ch < 2; ++ch)
        {
            mix.applyGain (ch, 0, hitAt, wetParam->load());
            mix.applyGain (ch, hitAt, n - hitAt, dryParam->load());
        }
    return writeWavSafely (dest, mix, r->sampleRate);
}

// ---------------- state ----------------

void ReverseVerbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("file", currentFile.getFullPathName(), nullptr);
    state.setProperty ("generated", generatedType, nullptr);
    state.setProperty ("gateMask", gateMask.toString(), nullptr);
    state.setProperty ("beatOrder", beatOrder.toString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ReverseVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        apvts.replaceState (state);
        gateMask.fromString (state.getProperty ("gateMask", "").toString());
        beatOrder.fromString (state.getProperty ("beatOrder", "").toString());
        juce::File f (state.getProperty ("file", "").toString());
        const int gen = (int) state.getProperty ("generated", -1);
        if (f.existsAsFile()) loadSampleFile (f);
        else if (gen >= 0) { generate (gen); previewAfterRender = false; }
        else
        {
            const juce::ScopedLock sl (sourceLock);
            sourceBuffer.setSize (0, 0);
            currentFile = juce::File(); generatedName = {}; generatedType = -1;
            missingFile = f;
        }
        dirty = true;
    }
}

juce::AudioProcessorEditor* ReverseVerbProcessor::createEditor() { return new ReverseVerbEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverseVerbProcessor(); }
