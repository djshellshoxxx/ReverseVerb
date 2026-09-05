#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TimeStretch.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
    add (IDs::tail,  "Length", R { 0.1f, 180.0f, 0.01f, 0.3f }, 1.2f, "s");
    add (IDs::shape, "Shape",  R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::tone,  "Color",  R { 500.0f, 20000.0f, 1.0f, 0.3f }, 20000.0f, "Hz");
    add (IDs::basscut, "Bass Cut", R { 20.0f, 2000.0f, 1.0f, 0.3f }, 20.0f, "Hz");
    add (IDs::trimStart, "Trim Start", R { 0.0f, 1.0f, 0.0001f }, 0.0f);
    add (IDs::trimEnd,   "Trim End",   R { 0.0f, 1.0f, 0.0001f }, 1.0f);
    add (IDs::pitch,        "Pitch",         R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::pitchTension, "Pitch Tension", R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::transpose,    "Transpose",     R { -48.0f, 48.0f, 0.01f }, 0.0f, "st");
    add (IDs::volStart,     "Vol Start",     R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::volEnd,       "Vol End",       R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::volTension,   "Vol Tension",   R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::panStart,     "Pan Start",     R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::panEnd,       "Pan End",       R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::panTension,   "Pan Tension",   R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::stretch,      "Stretch",       R { 1.0f, 64.0f, 0.001f, 0.25f }, 1.0f, "x");
    add (IDs::outputGain,   "Output Gain",   R { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");
    add (IDs::lfoRate,      "LFO Rate",      R { 0.02f, 20.0f, 0.001f, 0.3f }, 2.0f, "Hz");
    add (IDs::lfoDepth,     "LFO Depth",     R { 0.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::lfoShape,     "LFO Shape",     R { 0.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::manualBpm,    "BPM",           R { 20.0f, 300.0f, 0.01f }, 120.0f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::lfoTarget, 1 }, "LFO Target",
                     juce::StringArray { "Volume", "Pan" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::align, 1 }, "Hit on note (PDC)", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::sync, 1 }, "Sync to BPM", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::bpmSync, 1 }, "Sync BPM to Host", true));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::direction, 1 }, "Direction",
                     juce::StringArray { "Rise", "Fall" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::syncLen, 1 }, "Sync Length",
                     juce::StringArray { "1 beat", "2 beats", "4 beats", "8 beats", "1 bar", "2 bars", "4 bars" }, 2));
    juce::StringArray v2Divisions;
    for (const auto division : rv::allDivisions)
    {
        const auto label = rv::divisionLabel (division);
        v2Divisions.add (juce::String::fromUTF8 (label.data(), (int) label.size()));
    }
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::syncDivisionV2, 1 }, "Sync Division",
                     v2Divisions, (int) rv::Division::oneBar));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::pitchRange, 1 }, "Pitch Range",
                     juce::StringArray { "1 oct", "2 oct", "4 oct" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::gateEnabled, 1 }, "Gator Enabled", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::gateSteps, 1 }, "Gator Steps",
                     juce::StringArray { "16", "32" }, 0));
    juce::StringArray gateRates;
    for (const auto division : rv::gateRateDivisions)
    {
        const auto label = rv::divisionLabel (division);
        gateRates.add (juce::String::fromUTF8 (label.data(), (int) label.size()));
    }
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::gateRate, 1 }, "Gator Rate",
                     gateRates, 7));
    add (IDs::gateDepth, "Gator Depth", R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::gateSmooth, "Gator Smooth", R { 0.0f, 100.0f, 0.1f, 0.5f }, 3.0f, "ms");
    add (IDs::gateSwing, "Gator Swing", R { 0.0f, 0.75f, 0.001f }, 0.0f);
    add (IDs::gatePhase, "Gator Phase", R { 0.0f, 1.0f, 0.001f }, 0.0f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::gateRetrigger, 1 }, "Gator Retrigger",
                     juce::StringArray { "Note", "Host" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::gateTarget, 1 }, "Gator Target",
                     juce::StringArray { "Swell", "Hit", "Both" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::gateShape, 1 }, "Gator Shape",
                     juce::StringArray { "Square", "Smooth", "Ramp Up", "Ramp Down", "Triangle", "Sine", "Curved" }, 0));
    return { p.begin(), p.end() };
}

ReverseVerbProcessor::ReverseVerbProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();
    for (auto* id : { &IDs::size, &IDs::decay, &IDs::damp, &IDs::diff, &IDs::er, &IDs::sep, &IDs::width, &IDs::gap,
                      &IDs::tail, &IDs::shape, &IDs::tone, &IDs::basscut, &IDs::align, &IDs::trimStart, &IDs::trimEnd,
                      &IDs::sync, &IDs::syncLen, &IDs::syncDivisionV2, &IDs::pitch, &IDs::pitchRange, &IDs::pitchTension,
                      &IDs::transpose, &IDs::stretch, &IDs::volStart, &IDs::volEnd, &IDs::volTension,
                      &IDs::panStart, &IDs::panEnd, &IDs::panTension,
                      &IDs::lfoRate, &IDs::lfoDepth, &IDs::lfoShape, &IDs::lfoTarget, &IDs::direction })
        apvts.addParameterListener (*id, this);
    for (auto& cc : ccToParamIndex)
        cc = -1;
    dryParam = apvts.getRawParameterValue (IDs::dry);
    wetParam = apvts.getRawParameterValue (IDs::wet);
    gateEnabledParam = apvts.getRawParameterValue (IDs::gateEnabled);
    gateStepsParam = apvts.getRawParameterValue (IDs::gateSteps);
    gateRateParam = apvts.getRawParameterValue (IDs::gateRate);
    gateDepthParam = apvts.getRawParameterValue (IDs::gateDepth);
    gateSmoothParam = apvts.getRawParameterValue (IDs::gateSmooth);
    gateSwingParam = apvts.getRawParameterValue (IDs::gateSwing);
    gatePhaseParam = apvts.getRawParameterValue (IDs::gatePhase);
    gateRetriggerParam = apvts.getRawParameterValue (IDs::gateRetrigger);
    gateTargetParam = apvts.getRawParameterValue (IDs::gateTarget);
    gateShapeParam = apvts.getRawParameterValue (IDs::gateShape);
    rendered = std::make_shared<RenderedSample>();
    const rv::GatePattern initialPattern;
    publishGatePattern (initialPattern);
    storeGatePatternInState (initialPattern, nullptr);
    const rv::Envelope initialEnvelope;
    publishVolumeEnvelope (initialEnvelope);
    storeVolumeEnvelopeInState (initialEnvelope, nullptr);
    publishPanEnvelope (initialEnvelope);
    storePanEnvelopeInState (initialEnvelope, nullptr);
    undoManager.clearUndoHistory();
    startTimer (60);
}

ReverseVerbProcessor::~ReverseVerbProcessor() { stopTimer(); }

void ReverseVerbProcessor::parameterChanged (const juce::String& parameterId, float)
{
    if (parameterId == IDs::syncDivisionV2)
        useV2SyncDivision = true;
    dirty = true;
}

RenderDirection ReverseVerbProcessor::getDirection() const noexcept
{
    return param (IDs::direction) >= 0.5f ? RenderDirection::fall : RenderDirection::rise;
}

rv::HostTiming ReverseVerbProcessor::getHostTiming() const noexcept
{
    auto timing = rv::sanitiseTiming (hostBpm.load(),
                                      hostTimeSigNumerator.load(),
                                      hostTimeSigDenominator.load(),
                                      hostHasPpq.load() ? hostPpqPosition.load()
                                                        : std::numeric_limits<double>::quiet_NaN());
    const auto sampleRate = hostSampleRate.load();
    timing.sampleRate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 44100.0;
    timing.isPlaying = hostIsPlaying.load();
    timing.isLooping = hostIsLooping.load();
    timing.hasLoopRange = hostHasLoopRange.load();
    timing.loopStartPpq = hostLoopStartPpq.load();
    timing.loopEndPpq = hostLoopEndPpq.load();
    return timing;
}

rv::Division ReverseVerbProcessor::getSyncDivision() const noexcept
{
    if (! useV2SyncDivision.load())
        return rv::legacyDivision ((int) param (IDs::syncLen));

    const auto choice = juce::jlimit (0, (int) rv::Division::count - 1,
                                      (int) param (IDs::syncDivisionV2));
    return static_cast<rv::Division> (choice);
}

rv::GateSettings ReverseVerbProcessor::getGateSettings() const noexcept
{
    rv::GateSettings settings;
    const auto rateIndex = juce::jlimit (0, (int) rv::gateRateDivisions.size() - 1,
                                         (int) gateRateParam->load());
    settings.rate = rv::gateRateDivisions[(size_t) rateIndex];
    settings.activeSteps = gateStepsParam->load() < 0.5f ? 16 : 32;
    settings.depth = gateDepthParam->load();
    settings.smoothingMilliseconds = gateSmoothParam->load();
    settings.swing = gateSwingParam->load();
    settings.phase = gatePhaseParam->load();
    settings.retrigger = gateRetriggerParam->load() < 0.5f ? rv::GateRetrigger::note
                                                            : rv::GateRetrigger::host;
    const auto target = juce::jlimit (0, 2, (int) gateTargetParam->load());
    settings.target = static_cast<rv::GateTarget> (target);
    const auto shape = juce::jlimit (0, 6, (int) gateShapeParam->load());
    settings.shape = static_cast<rv::GateShape> (shape);
    return settings;
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

int ReverseVerbProcessor::getMidiCCForParameter (int parameterIndex) const noexcept
{
    if (parameterIndex < 0) return -1;
    for (int cc = 0; cc < numMidiCCs; ++cc)
        if (ccToParamIndex[(size_t) cc].load() == parameterIndex)
            return cc;
    return -1;
}

void ReverseVerbProcessor::clearMidiMapping (int parameterIndex) noexcept
{
    for (auto& cc : ccToParamIndex)
        if (cc.load() == parameterIndex)
            cc = -1;
}

void ReverseVerbProcessor::handleIncomingMidiCC (const juce::MidiMessage& msg) noexcept
{
    const auto cc = msg.getControllerNumber();
    if (cc < 0 || cc >= numMidiCCs) return;

    const auto armed = midiLearnArmedParamIndex.exchange (-1);
    if (armed >= 0)
    {
        ccToParamIndex[(size_t) cc] = armed;
        return;
    }

    const auto paramIndex = ccToParamIndex[(size_t) cc].load();
    if (paramIndex < 0) return;
    auto& params = getParameters();
    if (paramIndex >= params.size()) return;
    params.getUnchecked (paramIndex)->setValueNotifyingHost ((float) msg.getControllerValue() / 127.0f);
}

void ReverseVerbProcessor::resetEdits()
{
    setParam (IDs::trimStart, 0.0f);  setParam (IDs::trimEnd, 1.0f);
    setParam (IDs::pitch, 0.0f);      setParam (IDs::pitchTension, 0.0f);
    setParam (IDs::transpose, 0.0f);
    setParam (IDs::stretch, 1.0f);
    setParam (IDs::volStart, 1.0f);   setParam (IDs::volEnd, 1.0f);  setParam (IDs::volTension, 0.0f);
    setParam (IDs::panStart, 0.0f);   setParam (IDs::panEnd, 0.0f);  setParam (IDs::panTension, 0.0f);
    setParam (IDs::lfoDepth, 0.0f);
    replaceVolumeEnvelope (rv::Envelope {}, "Reset edits");
    replacePanEnvelope (rv::Envelope {}, "Reset edits");
    stopAll(); // silence any currently-playing preview so the reset is heard cleanly on the next trigger
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

std::shared_ptr<const rv::GatePattern> ReverseVerbProcessor::getGatePattern() const
{
    return std::atomic_load_explicit (&gatePattern, std::memory_order_acquire);
}

void ReverseVerbProcessor::publishGatePattern (const rv::GatePattern& pattern)
{
    std::shared_ptr<const rv::GatePattern> immutable =
        std::make_shared<const rv::GatePattern> (rv::sanitiseGatePattern (pattern));
    std::atomic_store_explicit (&gatePattern, std::move (immutable), std::memory_order_release);
}

void ReverseVerbProcessor::storeGatePatternInState (const rv::GatePattern& pattern,
                                                     juce::UndoManager* undo)
{
    const auto existing = apvts.state.getChildWithName (rv::gatePatternStateType);
    if (existing.isValid())
        apvts.state.removeChild (existing, undo);
    apvts.state.addChild (rv::gatePatternToValueTree (pattern), -1, undo);
}

void ReverseVerbProcessor::replaceGatePattern (const rv::GatePattern& pattern,
                                               const juce::String& transactionName)
{
    const auto clean = rv::sanitiseGatePattern (pattern);
    undoManager.beginNewTransaction (transactionName);
    const auto requestedStepChoice = clean.activeSteps == 32 ? 1.0f : 0.0f;
    if (std::abs (gateStepsParam->load() - requestedStepChoice) > 0.1f)
        if (auto* parameter = apvts.getParameter (IDs::gateSteps))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (requestedStepChoice));
            parameter->endChangeGesture();
        }
    storeGatePatternInState (clean, &undoManager);
    publishGatePattern (clean);
}

std::shared_ptr<const rv::Envelope> ReverseVerbProcessor::getVolumeEnvelope() const
{
    return std::atomic_load_explicit (&volumeEnvelope, std::memory_order_acquire);
}

std::shared_ptr<const rv::Envelope> ReverseVerbProcessor::getPanEnvelope() const
{
    return std::atomic_load_explicit (&panEnvelope, std::memory_order_acquire);
}

void ReverseVerbProcessor::publishVolumeEnvelope (const rv::Envelope& env)
{
    auto immutable = std::make_shared<const rv::Envelope> (rv::sanitiseEnvelope (env, 0.0f, 1.0f));
    std::atomic_store_explicit (&volumeEnvelope, std::move (immutable), std::memory_order_release);
}

void ReverseVerbProcessor::publishPanEnvelope (const rv::Envelope& env)
{
    auto immutable = std::make_shared<const rv::Envelope> (rv::sanitiseEnvelope (env, -1.0f, 1.0f));
    std::atomic_store_explicit (&panEnvelope, std::move (immutable), std::memory_order_release);
}

void ReverseVerbProcessor::storeVolumeEnvelopeInState (const rv::Envelope& env, juce::UndoManager* undo)
{
    const auto existing = apvts.state.getChildWithName (rv::volumeEnvelopeStateType);
    if (existing.isValid())
        apvts.state.removeChild (existing, undo);
    apvts.state.addChild (rv::envelopeToValueTree (rv::volumeEnvelopeStateType, env, 0.0f, 1.0f), -1, undo);
}

void ReverseVerbProcessor::storePanEnvelopeInState (const rv::Envelope& env, juce::UndoManager* undo)
{
    const auto existing = apvts.state.getChildWithName (rv::panEnvelopeStateType);
    if (existing.isValid())
        apvts.state.removeChild (existing, undo);
    apvts.state.addChild (rv::envelopeToValueTree (rv::panEnvelopeStateType, env, -1.0f, 1.0f), -1, undo);
}

void ReverseVerbProcessor::replaceVolumeEnvelope (const rv::Envelope& env, const juce::String& transactionName)
{
    const auto clean = rv::sanitiseEnvelope (env, 0.0f, 1.0f);
    undoManager.beginNewTransaction (transactionName);
    storeVolumeEnvelopeInState (clean, &undoManager);
    publishVolumeEnvelope (clean);
}

void ReverseVerbProcessor::replacePanEnvelope (const rv::Envelope& env, const juce::String& transactionName)
{
    const auto clean = rv::sanitiseEnvelope (env, -1.0f, 1.0f);
    undoManager.beginNewTransaction (transactionName);
    storePanEnvelopeInState (clean, &undoManager);
    publishPanEnvelope (clean);
}

void ReverseVerbProcessor::normalize()
{
    const auto r = getRendered();
    if (r == nullptr || r->getNumSamples() <= 0)
        return;
    const float dry = param (IDs::dry), wet = param (IDs::wet);
    const int n = r->getNumSamples();
    float peak = 0.0001f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const bool hasWet = ch < r->wetAudio.getNumChannels() && r->wetAudio.getNumSamples() >= n;
        const bool hasDry = ch < r->dryAudio.getNumChannels() && r->dryAudio.getNumSamples() >= n;
        const auto* w = hasWet ? r->wetAudio.getReadPointer (ch) : nullptr;
        const auto* d = hasDry ? r->dryAudio.getReadPointer (ch) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float sample = (w != nullptr ? w[i] * wet : 0.0f) + (d != nullptr ? d[i] * dry : 0.0f);
            peak = juce::jmax (peak, std::abs (sample));
        }
    }
    setParam (IDs::outputGain, juce::jlimit (-24.0f, 24.0f, juce::Decibels::gainToDecibels (0.9f / peak)));
}

void ReverseVerbProcessor::setGateStep (int step, float value,
                                        const juce::String& transactionName)
{
    auto current = getGatePattern();
    auto edited = current != nullptr ? *current : rv::GatePattern {};
    if (step < 0 || step >= (int) edited.steps.size())
        return;
    edited.activeSteps = getGateSettings().activeSteps;
    edited.steps[(size_t) step] = value;
    replaceGatePattern (edited, transactionName);
}

void ReverseVerbProcessor::clearGatePattern()
{
    auto current = getGatePattern();
    auto edited = current != nullptr ? *current : rv::GatePattern {};
    edited.activeSteps = getGateSettings().activeSteps;
    edited.steps.fill (0.0f);
    replaceGatePattern (edited, "Clear gate pattern");
}

void ReverseVerbProcessor::fillGatePattern()
{
    auto current = getGatePattern();
    auto edited = current != nullptr ? *current : rv::GatePattern {};
    edited.activeSteps = getGateSettings().activeSteps;
    edited.steps.fill (1.0f);
    replaceGatePattern (edited, "Fill gate pattern");
}

void ReverseVerbProcessor::invertGatePattern()
{
    auto current = getGatePattern();
    auto edited = current != nullptr ? *current : rv::GatePattern {};
    edited.activeSteps = getGateSettings().activeSteps;
    for (auto& step : edited.steps)
        step = 1.0f - step;
    replaceGatePattern (edited, "Invert gate pattern");
}

void ReverseVerbProcessor::randomizeGatePattern()
{
    auto current = getGatePattern();
    auto edited = current != nullptr ? *current : rv::GatePattern {};
    edited.activeSteps = getGateSettings().activeSteps;
    auto value = edited.seed != 0 ? edited.seed : 0x52564732u;
    for (auto& step : edited.steps)
    {
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        step = (float) (value & 0x00ffffffu) / (float) 0x00ffffffu;
    }
    edited.seed = value;
    replaceGatePattern (edited, "Randomize gate pattern");
}

void ReverseVerbProcessor::rotateGatePattern (int amount)
{
    auto current = getGatePattern();
    auto edited = current != nullptr ? *current : rv::GatePattern {};
    const auto active = getGateSettings().activeSteps;
    edited.activeSteps = active;
    const auto rotation = active > 0 ? amount % active : 0;
    if (rotation == 0)
        return;
    if (rotation > 0)
        std::rotate (edited.steps.begin(), edited.steps.begin() + active - rotation,
                     edited.steps.begin() + active);
    else
        std::rotate (edited.steps.begin(), edited.steps.begin() - rotation,
                     edited.steps.begin() + active);
    replaceGatePattern (edited, rotation > 0 ? "Rotate gate right" : "Rotate gate left");
}

void ReverseVerbProcessor::refreshGatePatternFromState()
{
    const auto current = getGatePattern();
    const auto fallback = current != nullptr ? *current : rv::GatePattern {};
    publishGatePattern (rv::gatePatternFromValueTree (
        apvts.state.getChildWithName (rv::gatePatternStateType), fallback));
}

void ReverseVerbProcessor::prepareToPlay (double sampleRate, int)
{
    if (std::abs (sampleRate - hostSampleRate.load()) > 0.5) dirty = true;
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
    target->active = true;
    target->pos = 0;
    target->gain = gain;
    target->id = ++voiceCounter;
    target->gate.reset();
}

void ReverseVerbProcessor::renderRange (juce::AudioBuffer<float>& out, const RenderedSample& renderedSample,
                                        int start, int num, float dry, float wet,
                                        const rv::GatePattern& pattern,
                                        const rv::GateSettings& settings,
                                        const rv::HostTiming& timing,
                                        bool gateEnabled)
{
    if (num <= 0) return;
    const int total = renderedSample.getNumSamples();
    for (auto& v : voices)
    {
        if (! v.active) continue;
        rv::mixGatedRenderedRange (out, renderedSample, v.pos, start, num,
                                   wet * v.gain, dry * v.gain, v.gate,
                                   pattern, settings, timing, v.pos, start, gateEnabled);
        v.pos += num;
        if (v.pos >= total) v.active = false;
    }
}

void ReverseVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    double bpm = rv::defaultBpm;
    int numerator = 4, denominator = 4;
    double ppq = std::numeric_limits<double>::quiet_NaN();
    bool isPlaying = false, isLooping = false, validLoop = false;
    double loopStart = 0.0, loopEnd = 0.0;

    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
        {
            if (const auto value = position->getBpm()) bpm = *value;
            if (const auto signature = position->getTimeSignature())
            {
                numerator = signature->numerator;
                denominator = signature->denominator;
            }
            if (const auto value = position->getPpqPosition()) ppq = *value;
            isPlaying = position->getIsPlaying();
            isLooping = position->getIsLooping();
            if (const auto loop = position->getLoopPoints())
            {
                validLoop = std::isfinite (loop->ppqStart) && std::isfinite (loop->ppqEnd)
                         && loop->ppqEnd > loop->ppqStart;
                if (validLoop) { loopStart = loop->ppqStart; loopEnd = loop->ppqEnd; }
            }
        }

    if (param (IDs::bpmSync) < 0.5f)
        bpm = param (IDs::manualBpm);

    const auto timing = rv::sanitiseTiming (bpm, numerator, denominator, ppq);
    hostBpm = timing.bpm;
    hostTimeSigNumerator = timing.timeSignature.numerator;
    hostTimeSigDenominator = timing.timeSignature.denominator;
    hostPpqPosition = timing.ppqPosition;
    hostHasPpq = timing.hasPpqPosition;
    hostIsPlaying = isPlaying;
    hostIsLooping = isLooping;
    hostLoopStartPpq = loopStart;
    hostLoopEndPpq = loopEnd;
    hostHasLoopRange = validLoop;

    const auto r = std::atomic_load_explicit (&rendered, std::memory_order_acquire);
    if (r == nullptr || r->getNumSamples() == 0) { playhead = -1; return; }

    if (stopRequest.exchange (0) != 0) for (auto& v : voices) v.active = false;
    if (triggerRequest.exchange (0) != 0) startVoice (1.0f);

    const float outGainLin = juce::Decibels::decibelsToGain (param (IDs::outputGain));
    const float dry = dryParam->load() * outGainLin, wet = wetParam->load() * outGainLin;
    const auto patternSnapshot = getGatePattern();
    const rv::GatePattern fallbackPattern;
    const auto& pattern = patternSnapshot != nullptr ? *patternSnapshot : fallbackPattern;
    const auto gateSettings = getGateSettings();
    const auto timingSnapshot = getHostTiming();
    const bool gateEnabled = gateEnabledParam->load() >= 0.5f;
    const int numSamples = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        renderRange (buffer, *r, pos, at - pos, dry, wet,
                     pattern, gateSettings, timingSnapshot, gateEnabled);
        pos = at;
        if (msg.isNoteOn()) startVoice (msg.getFloatVelocity());
        else if (msg.isController()) handleIncomingMidiCC (msg);
    }
    renderRange (buffer, *r, pos, numSamples - pos, dry, wet,
                 pattern, gateSettings, timingSnapshot, gateEnabled);

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
    const auto timing = getHostTiming();
    if (param (IDs::sync) > 0.5f
        && (std::abs (timing.bpm - lastRenderBpm) > 0.01
            || timing.timeSignature.numerator != lastRenderTimeSignature.numerator
            || timing.timeSignature.denominator != lastRenderTimeSignature.denominator))
        dirty = true;
    if (dirty.exchange (false)) render();
    if (previewAfterRender.exchange (false)) triggerPreview();
}

// ---------------- render ----------------

void ReverseVerbProcessor::render()
{
    juce::AudioBuffer<float> src; double srcSR;
    { const juce::ScopedLock sl (sourceLock); src.makeCopyOf (sourceBuffer); srcSR = sourceSR; }

    auto out = std::make_shared<RenderedSample>();
    const auto hostTiming = getHostTiming();
    const double sr = hostTiming.sampleRate;
    const double bpm = hostTiming.bpm;
    const auto direction = getDirection();
    lastRenderBpm = bpm;
    lastRenderTimeSignature = hostTiming.timeSignature;
    out->sampleRate = sr;
    out->bpm = bpm;
    out->direction = direction;
    out->timeSignatureNumerator = hostTiming.timeSignature.numerator;
    out->timeSignatureDenominator = hostTiming.timeSignature.denominator;

    if (src.getNumSamples() > 0 && srcSR > 0)
    {
        // 1. resample hit to host rate, stereo
        const int srcLen = src.getNumSamples();
        juce::AudioBuffer<float> padded (src.getNumChannels(), srcLen + 16);
        padded.clear();
        for (int ch = 0; ch < src.getNumChannels(); ++ch) padded.copyFrom (ch, 0, src, ch, 0, srcLen);
        const double ratio = srcSR / sr;
        int hitLen = juce::jmax (1, (int) std::floor (srcLen / ratio));
        juce::AudioBuffer<float> hit (2, hitLen);
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, padded.getReadPointer (juce::jmin (ch, padded.getNumChannels() - 1)), hit.getWritePointer (ch), hitLen);
        }
        const float hitMag = hit.getMagnitude (0, hitLen);
        if (hitMag > 0.0f) hit.applyGain (0.9f / hitMag);

        // 1b. optional time-stretch: spreads the source material itself across a
        // much longer span (independent of pitch), so full-length risers/fallers
        // can be built from real sample content rather than just a long reverb
        // tail appended to a short hit. Everything downstream (reverb, filters,
        // timeline placement) just keeps working off whatever hitLen now is.
        const float stretchAmt = param (IDs::stretch);
        if (stretchAmt > 1.001f)
        {
            hit = rv::timeStretch (hit, sr, (double) stretchAmt);
            hitLen = juce::jmin (hit.getNumSamples(), (int) (sr * 300.0));
            hit.setSize (2, hitLen, true, true, true);
            const float stretchedMag = hit.getMagnitude (0, hitLen);
            if (stretchedMag > 0.0f) hit.applyGain (0.9f / stretchedMag);
        }

        // 2. tail length (free or synced to BPM)
        const int gapLen = (int) (param (IDs::gap) * 0.001f * sr);
        const bool sync = param (IDs::sync) > 0.5f;
        int tailLen = juce::jmax (1, (int) std::llround (param (IDs::tail) * sr));
        if (sync)
        {
            const auto division = getSyncDivision();
            const auto targetLength = rv::durationSamples (division, bpm, sr, hostTiming.timeSignature);
            const auto minimumTail = (std::int64_t) std::llround (0.05 * sr);
            tailLen = wetTailSamplesForTimeline (direction, targetLength, hitLen, gapLen, minimumTail);
            out->musicalQuarterNotes = rv::quarterNotes (division, hostTiming.timeSignature);
            out->gridQuarterNotes = juce::jmin (1.0, out->musicalQuarterNotes);
        }
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
        out->beats = (int) std::ceil (out->musicalQuarterNotes);
        out->beatsPerBar = hostTiming.timeSignature.numerator;

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
        // Transpose is a constant offset added into the same varispeed engine as the
        // sweep below, so it re-uses that resampling pass instead of a second one.
        const float transposeSemis = param (IDs::transpose);
        juce::AudioBuffer<float> wetBuffer;
        juce::AudioBuffer<float> dryBuffer;
        std::vector<float> semiPerSample;
        int hitOut = -1;
        int dryOutStart = -1, dryOutEnd = -1;
        int wetOutStart = -1, wetOutEnd = -1;
        if (std::abs (pitchAmt) > 0.001f || std::abs (transposeSemis) > 0.001f)
        {
            std::array<std::vector<float>, 2> wetSamples, drySamples;
            for (auto& samples : wetSamples) samples.reserve ((size_t) trimLen * 2);
            for (auto& samples : drySamples) samples.reserve ((size_t) trimLen * 2);
            double p = 0.0;
            // Pitching down stretches the timeline; the sweep and Transpose can now
            // combine to as much as -8 octaves (256x slowdown at the extreme), so the
            // cap must scale with trimLen rather than a fixed duration or long/slow
            // renders get truncated. An absolute ceiling still guards against runaway
            // memory use for genuinely pathological combinations.
            const auto sampleCap = (size_t) juce::jmin<juce::int64> ((juce::int64) trimLen * 300 + 8,
                                                                     (juce::int64) (sr * 600.0));
            while (p < trimLen - 1 && semiPerSample.size() < sampleCap)
            {
                const int i0 = (int) p; const float frac = (float) (p - i0);
                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto* wet = fullWet.getReadPointer (ch) + tStart;
                    const auto* dry = fullDry.getReadPointer (ch) + tStart;
                    wetSamples[(size_t) ch].push_back (wet[i0] + (wet[i0 + 1] - wet[i0]) * frac);
                    drySamples[(size_t) ch].push_back (dry[i0] + (dry[i0 + 1] - dry[i0]) * frac);
                }
                const float semis = pitchAmt * octaves * 12.0f * tensionCurve ((float) p / (float) trimLen, pitchT)
                                  + transposeSemis;
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

        // 10. volume + pan envelopes (each may carry extra hand-placed points
        // beyond the Start/End knobs), plus an optional LFO nudging whichever
        // of the two it's aimed at.
        const int n = wetBuffer.getNumSamples();
        const float v0 = param (IDs::volStart), v1 = param (IDs::volEnd), vt = param (IDs::volTension);
        const float p0 = param (IDs::panStart), p1 = param (IDs::panEnd), pt = param (IDs::panTension);
        const rv::Envelope emptyEnvelope;
        const auto volEnvPtr = getVolumeEnvelope();
        const auto panEnvPtr = getPanEnvelope();
        const auto& volE = volEnvPtr != nullptr ? *volEnvPtr : emptyEnvelope;
        const auto& panE = panEnvPtr != nullptr ? *panEnvPtr : emptyEnvelope;
        const float lfoRateHz = param (IDs::lfoRate), lfoDepthAmt = param (IDs::lfoDepth), lfoShapeAmt = param (IDs::lfoShape);
        const int lfoTargetIdx = (int) param (IDs::lfoTarget); // 0 = Volume, 1 = Pan
        const bool lfoActive = lfoDepthAmt > 0.0005f;
        const float lfoExponent = juce::jmap (juce::jlimit (0.0f, 1.0f, lfoShapeAmt), 1.0f, 0.05f);
        const bool flatVol = std::abs (v0 - 1.0f) < 0.001f && std::abs (v1 - 1.0f) < 0.001f && volE.numInterior == 0
                          && ! (lfoActive && lfoTargetIdx == 0);
        const bool flatPan = std::abs (p0) < 0.001f && std::abs (p1) < 0.001f && panE.numInterior == 0
                          && ! (lfoActive && lfoTargetIdx == 1);
        for (int i = 0; i < n; ++i)
        {
            float lfoVal = 0.0f;
            if (lfoActive)
            {
                const float phase = std::fmod ((float) i / (float) sr * lfoRateHz, 1.0f);
                const float osc = std::sin (juce::MathConstants<float>::twoPi * phase);
                lfoVal = (osc < 0.0f ? -1.0f : 1.0f) * std::pow (std::abs (osc), lfoExponent) * lfoDepthAmt;
            }

            float g = 1.0f;
            if (! flatVol)
            {
                const float t = (float) i / (float) juce::jmax (1, n - 1);
                float lvl = rv::envelopeValueAt (volE, t, v0, v1, vt);
                if (lfoActive && lfoTargetIdx == 0) lvl *= (1.0f + lfoVal);
                g = juce::jmax (0.0f, lvl * lvl);
                for (int ch = 0; ch < 2; ++ch)
                {
                    wetBuffer.getWritePointer (ch)[i] *= g;
                    dryBuffer.getWritePointer (ch)[i] *= g;
                }
            }
            if (! flatPan)
            {
                const float t = (float) i / (float) juce::jmax (1, n - 1);
                float pan = rv::envelopeValueAt (panE, t, p0, p1, pt);
                if (lfoActive && lfoTargetIdx == 1) pan += lfoVal;
                pan = juce::jlimit (-1.0f, 1.0f, pan);
                // Balance law (not equal-power): unity on the side panned toward,
                // the other side attenuates. Matches pan = 0 exactly so there's no
                // level jump the moment this control is first touched.
                const float panGainL = juce::jmin (1.0f, 1.0f - pan);
                const float panGainR = juce::jmin (1.0f, 1.0f + pan);
                wetBuffer.getWritePointer (0)[i] *= panGainL; wetBuffer.getWritePointer (1)[i] *= panGainR;
                dryBuffer.getWritePointer (0)[i] *= panGainL; dryBuffer.getWritePointer (1)[i] *= panGainR;
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
    generatedLabel.clear();
    refreshFolderList (f);
    if (previewAfter) previewAfterRender = true;
    dirty = true;
    return true;
}

bool ReverseVerbProcessor::generateSample (rv::GeneratedSampleType type)
{
    const auto sr = hostSampleRate.load();
    const double sampleRate = sr > 0.0 ? sr : 44100.0;
    const auto seed = (juce::uint32) juce::Random::getSystemRandom().nextInt();
    auto buf = rv::generateSample (type, sampleRate, seed);
    if (buf.getNumSamples() <= 0) return false;
    { const juce::ScopedLock sl (sourceLock); sourceBuffer = std::move (buf); sourceSR = sampleRate; }
    currentFile = juce::File();
    folderFiles.clear();
    currentIndex = -1;
    generatedLabel = rv::generatedSampleName (type);
    previewAfterRender = true;
    dirty = true;
    return true;
}

juce::String ReverseVerbProcessor::getDisplayLabel() const
{
    if (generatedLabel.isNotEmpty()) return generatedLabel;
    return currentFile.existsAsFile() ? currentFile.getFileName() : juce::String();
}

// ---------------- presets ----------------

juce::ValueTree ReverseVerbProcessor::buildPresetState()
{
    return apvts.copyState();
}

void ReverseVerbProcessor::applyPresetState (const juce::ValueTree& state, const juce::String& presetName)
{
    if (! state.isValid())
        return;

    const auto current = getGatePattern();
    const auto fallback = current != nullptr ? *current : rv::GatePattern {};
    const auto restoredPattern = rv::gatePatternFromValueTree (
        state.getChildWithName (rv::gatePatternStateType), fallback);
    const auto restoredVolEnvelope = rv::envelopeFromValueTree (rv::volumeEnvelopeStateType,
        state.getChildWithName (rv::volumeEnvelopeStateType), 0.0f, 1.0f);
    const auto restoredPanEnvelope = rv::envelopeFromValueTree (rv::panEnvelopeStateType,
        state.getChildWithName (rv::panEnvelopeStateType), -1.0f, 1.0f);

    apvts.replaceState (state);
    publishGatePattern (restoredPattern);
    if (! apvts.state.getChildWithName (rv::gatePatternStateType).isValid())
        storeGatePatternInState (restoredPattern, nullptr);
    publishVolumeEnvelope (restoredVolEnvelope);
    if (! apvts.state.getChildWithName (rv::volumeEnvelopeStateType).isValid())
        storeVolumeEnvelopeInState (restoredVolEnvelope, nullptr);
    publishPanEnvelope (restoredPanEnvelope);
    if (! apvts.state.getChildWithName (rv::panEnvelopeStateType).isValid())
        storePanEnvelopeInState (restoredPanEnvelope, nullptr);

    currentPresetName = presetName;
    dirty = true;
}

void ReverseVerbProcessor::applyFactoryPreset (const rv::FactoryPreset& preset)
{
    for (const auto& [id, value] : preset.params)
        setParam (id, value);
    if (preset.gatePattern.has_value())
        replaceGatePattern (*preset.gatePattern, "Load preset: " + preset.name);
    replaceVolumeEnvelope (rv::Envelope {}, "Load preset: " + preset.name);
    replacePanEnvelope (rv::Envelope {}, "Load preset: " + preset.name);
    currentPresetName = preset.name;
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
    if (r == nullptr || r->getNumSamples() == 0) return false;
    const int n = r->getNumSamples();
    juce::AudioBuffer<float> mix (2, n);
    mix.clear();
    const auto patternSnapshot = getGatePattern();
    const rv::GatePattern fallbackPattern;
    const auto& pattern = patternSnapshot != nullptr ? *patternSnapshot : fallbackPattern;
    auto gateSettings = getGateSettings();
    gateSettings.retrigger = rv::GateRetrigger::note;
    auto timing = getHostTiming();
    timing.hasPpqPosition = false;
    rv::GateEngine exportGate;
    exportGate.reset();
    rv::mixGatedRenderedRange (mix, *r, 0, 0, n, wetParam->load(), dryParam->load(),
                               exportGate, pattern, gateSettings, timing, 0, 0,
                               gateEnabledParam->load() >= 0.5f);
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
    state.setProperty ("useV2SyncDivision", useV2SyncDivision.load(), nullptr);
    state.setProperty ("file", currentFile.getFullPathName(), nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    {
        juce::StringArray ccEntries;
        for (auto& cc : ccToParamIndex)
            ccEntries.add (juce::String (cc.load()));
        state.setProperty ("midiCCMap", ccEntries.joinIntoString (","), nullptr);
    }
    const auto existingPattern = state.getChildWithName (rv::gatePatternStateType);
    if (existingPattern.isValid())
        state.removeChild (existingPattern, nullptr);
    if (const auto pattern = getGatePattern())
    {
        auto savedPattern = *pattern;
        savedPattern.activeSteps = getGateSettings().activeSteps;
        state.addChild (rv::gatePatternToValueTree (savedPattern), -1, nullptr);
    }
    for (auto* type : { &rv::volumeEnvelopeStateType, &rv::panEnvelopeStateType })
    {
        const auto existing = state.getChildWithName (*type);
        if (existing.isValid())
            state.removeChild (existing, nullptr);
    }
    if (const auto volEnv = getVolumeEnvelope())
        state.addChild (rv::envelopeToValueTree (rv::volumeEnvelopeStateType, *volEnv, 0.0f, 1.0f), -1, nullptr);
    if (const auto panEnv = getPanEnvelope())
        state.addChild (rv::envelopeToValueTree (rv::panEnvelopeStateType, *panEnv, -1.0f, 1.0f), -1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ReverseVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        const bool hasDirection = stateContainsParameter (state, IDs::direction);
        const bool restoreV2Sync = (bool) state.getProperty ("useV2SyncDivision", false)
                                && stateContainsParameter (state, IDs::syncDivisionV2);
        const auto restoredPattern = rv::gatePatternFromValueTree (
            state.getChildWithName (rv::gatePatternStateType));
        const auto restoredVolEnvelope = rv::envelopeFromValueTree (rv::volumeEnvelopeStateType,
            state.getChildWithName (rv::volumeEnvelopeStateType), 0.0f, 1.0f);
        const auto restoredPanEnvelope = rv::envelopeFromValueTree (rv::panEnvelopeStateType,
            state.getChildWithName (rv::panEnvelopeStateType), -1.0f, 1.0f);
        apvts.replaceState (state);
        publishGatePattern (restoredPattern);
        if (! apvts.state.getChildWithName (rv::gatePatternStateType).isValid())
            storeGatePatternInState (restoredPattern, nullptr);
        publishVolumeEnvelope (restoredVolEnvelope);
        if (! apvts.state.getChildWithName (rv::volumeEnvelopeStateType).isValid())
            storeVolumeEnvelopeInState (restoredVolEnvelope, nullptr);
        publishPanEnvelope (restoredPanEnvelope);
        if (! apvts.state.getChildWithName (rv::panEnvelopeStateType).isValid())
            storePanEnvelopeInState (restoredPanEnvelope, nullptr);
        if (! hasDirection)
            if (auto* direction = apvts.getParameter (IDs::direction))
                direction->setValueNotifyingHost (direction->convertTo0to1 (0.0f));
        if (! restoreV2Sync)
            if (auto* division = apvts.getParameter (IDs::syncDivisionV2))
            {
                const auto mapped = (float) static_cast<int> (rv::legacyDivision ((int) param (IDs::syncLen)));
                division->setValueNotifyingHost (division->convertTo0to1 (mapped));
            }
        useV2SyncDivision = restoreV2Sync;
        currentPresetName = state.getProperty ("presetName", "").toString();
        {
            const auto ccMap = juce::StringArray::fromTokens (
                state.getProperty ("midiCCMap", "").toString(), ",", "");
            for (int cc = 0; cc < numMidiCCs; ++cc)
                ccToParamIndex[(size_t) cc] = cc < ccMap.size() ? ccMap[cc].getIntValue() : -1;
        }
        juce::File f (state.getProperty ("file", "").toString());
        if (f.existsAsFile()) loadSampleFile (f);
        undoManager.clearUndoHistory();
        dirty = true;
    }
}

juce::AudioProcessorEditor* ReverseVerbProcessor::createEditor() { return new ReverseVerbEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverseVerbProcessor(); }
