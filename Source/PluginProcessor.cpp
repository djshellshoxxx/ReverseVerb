// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "PluginProcessor.h"
#include "PluginEditor.h"

ReverseVerbProcessor::ReverseVerbProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    for (auto* id : { &IDs::size, &IDs::decay, &IDs::damp, &IDs::diff, &IDs::er, &IDs::sep, &IDs::width, &IDs::gap,
                      &IDs::tail, &IDs::shape, &IDs::tone, &IDs::basscut, &IDs::align, &IDs::trimStart, &IDs::trimEnd,
                      &IDs::sync, &IDs::syncLen, &IDs::pitch, &IDs::pitchRange, &IDs::pitchTension,
                      &IDs::volStart, &IDs::volEnd, &IDs::volTension })
        apvts.addParameterListener (*id, this);
    dryParam = apvts.getRawParameterValue (IDs::dry);
    wetParam = apvts.getRawParameterValue (IDs::wet);
    rendered = std::make_shared<RenderedSample>();
    renderThread = std::make_unique<RenderThread> (*this);
    renderThread->startThread (juce::Thread::Priority::background);
    startTimer (40);
}

ReverseVerbProcessor::~ReverseVerbProcessor()
{
    stopTimer();
    renderThread->signalThreadShouldExit();
    renderThread->notify();
    renderThread->stopThread (5000);
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
    if (std::abs (sampleRate - hostSampleRate.load()) > 0.5) dirty = true;
    hostSampleRate = sampleRate;
    voices.stopAll();
    playhead = -1;
}

bool ReverseVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

// ---------------- playback ----------------

void ReverseVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0) hostBpm = *bpm;

    std::shared_ptr<const RenderedSample> r;
    {
        juce::SpinLock::ScopedTryLockType tl (renderLock);
        if (! tl.isLocked()) return;
        r = rendered;
    }
    if (r == nullptr || r->audio.getNumSamples() == 0) { playhead = -1; return; }

    if (stopRequest.exchange (0) != 0) voices.stopAll();
    if (triggerRequest.exchange (0) != 0) voices.start (1.0f);

    const float dry = dryParam->load(), wet = wetParam->load();
    const int numSamples = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        voices.render (buffer, *r, pos, at - pos, dry, wet);
        pos = at;
        if (msg.isNoteOn()) voices.start (msg.getFloatVelocity());
    }
    voices.render (buffer, *r, pos, numSamples - pos, dry, wet);

    playhead = voices.newestPosition();
}

std::shared_ptr<const RenderedSample> ReverseVerbProcessor::getRendered() const
{
    juce::SpinLock::ScopedLockType l (renderLock);
    return rendered;
}

void ReverseVerbProcessor::timerCallback()
{
    if (param (IDs::sync) > 0.5f && std::abs (hostBpm.load() - lastRenderBpm.load()) > 0.01) dirty = true;
    if (dirty.exchange (false)) renderThread->notify();
    const int lat = pendingLatency.exchange (-1);
    if (lat >= 0 && lat != getLatencySamples()) setLatencySamples (lat);
}

// ---------------- render ----------------

RenderSettings ReverseVerbProcessor::currentSettings() const
{
    RenderSettings s;
    s.sampleRate = hostSampleRate.load();
    s.bpm = hostBpm.load();
    s.size = param (IDs::size);   s.decay = param (IDs::decay);   s.damp = param (IDs::damp);   s.diff = param (IDs::diff);
    s.er = param (IDs::er);       s.sep = param (IDs::sep);       s.width = param (IDs::width); s.gap = param (IDs::gap);
    s.tail = param (IDs::tail);   s.shape = param (IDs::shape);   s.tone = param (IDs::tone);   s.basscut = param (IDs::basscut);
    s.trimStart = param (IDs::trimStart); s.trimEnd = param (IDs::trimEnd);
    s.pitch = param (IDs::pitch); s.pitchTension = param (IDs::pitchTension);
    s.volStart = param (IDs::volStart); s.volEnd = param (IDs::volEnd); s.volTension = param (IDs::volTension);
    s.sync = param (IDs::sync) > 0.5f;
    s.syncLen = (int) param (IDs::syncLen);
    s.pitchRange = (int) param (IDs::pitchRange);
    return s;
}

void ReverseVerbProcessor::render()
{
    const juce::ScopedLock rl (renderMutex);
    const bool wantPreview = previewAfterRender.exchange (false);   // taken before the source is read, so the source is already new

    const auto settings = currentSettings();
    lastRenderBpm = settings.bpm;

    SourceAudio source;
    { const juce::ScopedLock sl (sourceLock); source.buffer = sourceBuffer; source.sampleRate = sourceSR; source.version = sourceVersion; }

    auto out = renderSample (settings, source, &cache);

    const int latency = out->hitIndex > 0 ? out->hitIndex : 0;
    std::shared_ptr<RenderedSample> old;
    { juce::SpinLock::ScopedLockType l (renderLock); old = std::move (rendered); rendered = out; }
    retired[retireIdx++ & 1u] = std::move (old);      // frees the render from two swaps ago, here, not on the audio thread
    pendingLatency = param (IDs::align) > 0.5f ? latency : 0;
    if (wantPreview) triggerPreview();
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
    { const juce::ScopedLock sl (sourceLock); sourceBuffer = std::make_shared<const juce::AudioBuffer<float>> (std::move (buf)); sourceSR = reader->sampleRate; ++sourceVersion; }
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
    { const juce::ScopedLock rl (renderMutex); if (dirty.exchange (false)) render(); }
    auto r = getRendered();
    if (r == nullptr || r->audio.getNumSamples() == 0) return false;
    const int n = r->audio.getNumSamples();
    const int hitAt = r->hitIndex >= 0 ? r->hitIndex : n;
    juce::AudioBuffer<float> mix;
    mix.makeCopyOf (r->audio);
    for (int ch = 0; ch < 2; ++ch)
    {
        mix.applyGain (ch, 0, hitAt, wetParam->load());
        mix.applyGain (ch, hitAt, n - hitAt, dryParam->load());
    }
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
    state.setProperty ("file", currentFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ReverseVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        apvts.replaceState (state);
        juce::File f (state.getProperty ("file", "").toString());
        if (f.existsAsFile()) loadSampleFile (f);
        dirty = true;
    }
}

juce::AudioProcessorEditor* ReverseVerbProcessor::createEditor() { return new ReverseVerbEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverseVerbProcessor(); }
