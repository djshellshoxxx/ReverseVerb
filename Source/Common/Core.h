#pragma once
#include <JuceHeader.h>
#include <map>

namespace IDs
{
    static const juce::String dry = "dry", wet = "wet";
    static const juce::String size = "size", decay = "decay", damp = "damp", diff = "diff", er = "er", sep = "sep", width = "width", gap = "gap";
    static const juce::String tail = "tail", shape = "shape", tone = "tone", basscut = "basscut";
    static const juce::String align = "align";
    static const juce::String trimStart = "trimStart", trimEnd = "trimEnd";
    static const juce::String sync = "sync", syncLen = "syncLen";
    static const juce::String pitch = "pitch", pitchRange = "pitchRange", pitchTension = "pitchTension";
    static const juce::String volStart = "volStart", volEnd = "volEnd", volTension = "volTension";
    // FX only
    static const juce::String threshold = "threshold", sens = "sens", hold = "hold", hitLen = "hitLen", gateHit = "gateHit";
    static const juce::String trigMode = "trigMode", followLevel = "followLevel", maxVoices = "maxVoices", swellGain = "swellGain";
    static const juce::String freeze = "freeze", listen = "listen";
    // v1.2 additions (shared)
    static const juce::String panStart = "panStart", panEnd = "panEnd", panTension = "panTension";
    static const juce::String fxType = "fxType", fxTime = "fxTime", fxFeedback = "fxFeedback", fxDepth = "fxDepth", fxMix = "fxMix";
    static const juce::String normalize = "normalize", swapStereo = "swapStereo", invertPhase = "invertPhase";
    static const juce::String gateShape = "gateShape", gateDepth = "gateDepth";
    static const juce::String stretch = "stretch";
    static const juce::String bassBoost = "bassBoost", trebleBoost = "trebleBoost";
    // instrument only
    static const juce::String mode = "mode";   // 0 reverse reverb, 1 forward reverb, 2 dry
}

static const int kSyncBeats[] = { 1, 2, 4, 8, 4, 8, 16, 32, 64, 128 };   // 1,2,4,8 beats / 1,2,4,8,16,32 bars
static const int kNumSync = 10;
static const int kNumSyncChoices = 10;
static const char* const kSyncNames[] = { "1 beat", "2 beats", "4 beats", "8 beats", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars" };
static const int kMaxBeats = 128;
inline juce::StringArray syncChoiceNames() { return { "1 beat", "2 beats", "4 beats", "8 beats", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars" }; }
static const float kPitchOct[] = { 1.0f, 2.0f, 4.0f };

// FL-style tension curve: x in 0..1 -> 0..1. t>0 = slow start, t<0 = fast start.
inline float tensionCurve (float x, float t)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    if (std::abs (t) < 0.001f) return x;
    const float k = 1.0f + 5.0f * std::abs (t);
    return t > 0.0f ? std::pow (x, k) : 1.0f - std::pow (1.0f - x, k);
}

struct RenderedSample
{
    juce::AudioBuffer<float> audio;     // stereo, playable
    int hitIndex = -1;                  // where the dry hit starts inside audio, -1 if absent
    double sampleRate = 44100.0;
    int beats = 0;
    int beatsPerBar = 4;
    double fullLengthSec = 0.0;         // untrimmed length of the trimmable region
    double trimStartSec = 0.0, trimEndSec = 0.0;
    std::vector<float> pitchSemi;       // one per envStep samples
    std::vector<float> gainLin;
    std::vector<float> panPos;
    static constexpr int envStep = 256;
    bool gainsBaked = false;            // playback applies no dry/wet
    std::vector<float> ghostMin, ghostMax;   // overview of the full untrimmed material
    static constexpr int ghostRes = 1024;
    std::vector<unsigned char> gateMask;     // per beat, 1 = on (only when beats > 0)
};

// ---------------- Freeverb-style reverb ----------------
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
    void setup (double sr, float size, float decay, float damp, float diff, float sep, float width, float er);
    void process (float* l, float* r, int n);
};

// Parameter access abstraction so shared code works for both plugins.
struct ParamSource
{
    virtual ~ParamSource() = default;
    virtual float param (const juce::String& id) const = 0;
    virtual void setParam (const juce::String& id, float value) = 0;        // begin gesture + set + end gesture
    virtual void setParamValue (const juce::String& id, float value) = 0;   // set only (inside a gesture)
    virtual void beginGesture (const juce::String& id) = 0;
    virtual void endGesture (const juce::String& id) = 0;
};

// Implements the gesture plumbing on top of an APVTS.
struct ApvtsParamSource : public ParamSource
{
    explicit ApvtsParamSource (juce::AudioProcessorValueTreeState& s) : apvtsRef (s) {}
    float param (const juce::String& id) const override { auto* v = apvtsRef.getRawParameterValue (id); return v ? v->load() : 0.0f; }
    void setParam (const juce::String& id, float value) override { beginGesture (id); setParamValue (id, value); endGesture (id); }
    void setParamValue (const juce::String& id, float value) override { if (auto* p = apvtsRef.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value)); }
    void beginGesture (const juce::String& id) override { if (auto* p = apvtsRef.getParameter (id)) p->beginChangeGesture(); }
    void endGesture (const juce::String& id) override { if (auto* p = apvtsRef.getParameter (id)) p->endChangeGesture(); }
    juce::AudioProcessorValueTreeState& apvtsRef;
};

// Host interface the shared UI talks to.
struct RVHost : public ParamSource
{
    virtual std::shared_ptr<const RenderedSample> getRendered() const = 0;
    virtual int getPlayheadPosition() const = 0;
    virtual double getHostBpm() const = 0;
    virtual void triggerPreview() = 0;
    virtual bool exportWav (const juce::File& dest) = 0;
    virtual juce::String exportBaseName() const = 0;
    virtual void resetEdits();
    virtual void randomizeReverb();
    virtual bool trimIncludesHit() const { return true; }
    virtual bool isBeatEnabled (int beat) const = 0;
    virtual void toggleBeat (int beat) = 0;
    virtual void setAllBeats (bool on) = 0;
    virtual void shuffleBeats (bool restore) = 0;
    virtual bool isShuffled() const = 0;
};

// Beat gate mask (128 beats) kept as a string of 0/1 in the plugin state.
struct GateMask
{
    std::array<std::atomic<unsigned char>, kMaxBeats> on;
    GateMask() { for (auto& b : on) b.store (1); }
    bool get (int i) const { return i >= 0 && i < kMaxBeats ? on[(size_t) i].load() != 0 : true; }
    void set (int i, bool v) { if (i >= 0 && i < kMaxBeats) on[(size_t) i].store (v ? 1 : 0); }
    juce::String toString() const { juce::String s; for (auto& b : on) s << (b.load() ? '1' : '0'); return s; }
    void fromString (const juce::String& s) { for (int i = 0; i < kMaxBeats; ++i) set (i, i >= s.length() || s[i] != '0'); }
};
// Playback order of beat segments: order[slot] = source beat. Identity = not shuffled.
struct BeatOrder
{
    std::array<std::atomic<unsigned char>, kMaxBeats> order;
    BeatOrder() { reset(); }
    void reset() { for (int i = 0; i < kMaxBeats; ++i) order[(size_t) i].store ((unsigned char) i); }
    bool isIdentity() const { for (int i = 0; i < kMaxBeats; ++i) if (order[(size_t) i].load() != i) return false; return true; }
    void shuffle (int beats)
    {
        std::vector<int> v; for (int i = 0; i < beats; ++i) v.push_back (i);
        auto& rng = juce::Random::getSystemRandom();
        for (int i = beats - 1; i > 0; --i) std::swap (v[(size_t) i], v[(size_t) rng.nextInt (i + 1)]);
        reset();
        for (int i = 0; i < beats; ++i) order[(size_t) i].store ((unsigned char) v[(size_t) i]);
    }
    juce::String toString() const { juce::String s; for (int i = 0; i < kMaxBeats; ++i) { if (i) s << ','; s << (int) order[(size_t) i].load(); } return s; }
    void fromString (const juce::String& s)
    {
        reset();
        auto parts = juce::StringArray::fromTokens (s, ",", "");
        for (int i = 0; i < juce::jmin (kMaxBeats, parts.size()); ++i) { const int v = parts[i].getIntValue(); if (v >= 0 && v < kMaxBeats) order[(size_t) i].store ((unsigned char) v); }
    }
};

// Safe WAV write: temp file in the destination folder, renamed over dest only on success.
bool writeWavSafely (const juce::File& dest, const juce::AudioBuffer<float>& audio, double sr);
// Gentle soft clip used on summed outputs so stacked swells cannot slam past 0 dBFS.
inline float softClip (float x) { return x > 0.8f ? 0.8f + 0.2f * std::tanh ((x - 0.8f) * 5.0f) : (x < -0.8f ? -0.8f - 0.2f * std::tanh ((-x - 0.8f) * 5.0f) : x); }

// Shared parameter layout pieces.
void addReverbParams (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p);
void addSwellParams  (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& p);   // tail shape tone basscut trim sync pitch vol
juce::StringArray sharedParamIds();   // everything added by the two functions above (+ wet)

// ---- DSP helpers shared by both renderers ----
void applyEcho (juce::AudioBuffer<float>& b, double sr, float timeMs, float feedback, float mix);
void applyChorus (juce::AudioBuffer<float>& b, double sr, float rateHz, float depth, float mix);
void applySwellFx (juce::AudioBuffer<float>& b, double sr, const ParamSource& P, bool beforeReverse);   // picks echo / reverse echo / chorus
void applyBeatOrder (juce::AudioBuffer<float>& b, int gridLen, int beats, const BeatOrder& order, double sr);
void applyBeatGate (juce::AudioBuffer<float>& b, int gridLen, int beats, const GateMask& mask, int shape, float depth, double sr);
void applyPanEnvelope (juce::AudioBuffer<float>& b, const ParamSource& P, std::vector<float>* panOut);
void applyShelves (juce::AudioBuffer<float>& b, double sr, const ParamSource& P);   // bass / treble boost
void finalizeOutput (juce::AudioBuffer<float>& b, const ParamSource& P);   // swap, phase, normalize
void makeGhost (const juce::AudioBuffer<float>& b, RenderedSample& meta);
juce::AudioBuffer<float> stretchHit (const juce::AudioBuffer<float>& hit, double sr, float factor);   // granular time stretch + envelope smoothing
juce::AudioBuffer<float> generateHit (int type, double sr);   // 0 closed hat, 1 snare, 2 clap, 3 kick, 4 rim

// Renders reversed reverb swell from a hit (stereo, host SR). Returns swell only.
// Applies filters, shape, normalise, gap, trim (swell only), pitch sweep, volume envelope.
// Fills envelope info into meta. maxOutSamples caps the result (keeps the END of the swell).
juce::AudioBuffer<float> renderSwell (const juce::AudioBuffer<float>& hit, double sr, const ParamSource& P, double bpm,
                                      int maxOutSamples, RenderedSample& meta, const GateMask* gate = nullptr, const BeatOrder* order = nullptr);

// ---------------- presets ----------------
struct FactoryPreset { juce::String name; std::map<juce::String, float> values; };
std::vector<FactoryPreset> factoryPresets();

class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& apvts, const juce::String& subfolder);
    ~PresetManager() override;
    juce::StringArray getNames();                 // factory first, then user
    int getNumFactory() const { return (int) factory.size(); }
    bool load (int index);
    bool save (const juce::String& name);        // user preset
    bool remove (int index);                     // user only
    int getCurrentIndex() const { return current.load(); }
    juce::String getCurrentName() const { return currentName; }
    juce::File getFolder() const { return folder; }
private:
    void parameterChanged (const juce::String&, float) override { if (! applying.load()) current = -1; }
    void applyValues (const std::map<juce::String, float>& v);
    juce::AudioProcessorValueTreeState& apvts;
    juce::File folder;
    std::vector<FactoryPreset> factory;
    juce::Array<juce::File> userFiles;
    std::atomic<int> current { -1 };
    std::atomic<bool> applying { false };
    juce::String currentName;
};
