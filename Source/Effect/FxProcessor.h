#pragma once
#include <JuceHeader.h>
#include "../Common/Core.h"

class FxProcessor : public juce::AudioProcessor,
                    public RVHost,
                    private juce::Timer,
                    private juce::AudioProcessorValueTreeState::Listener,
                    private juce::Thread
{
public:
    FxProcessor();
    ~FxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return latency.load() / juce::jmax (1.0, hostSampleRate) + 21.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // RVHost
    std::shared_ptr<const RenderedSample> getRendered() const override;
    int getPlayheadPosition() const override { return playhead.load(); }
    double getHostBpm() const override { return hostBpm.load(); }
    void triggerPreview() override { auditionRequest = 1; }
    bool exportWav (const juce::File& dest) override;
    juce::String exportBaseName() const override { return "swell"; }
    float param (const juce::String& id) const override { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float value) override;
    void setParamValue (const juce::String& id, float value) override { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value)); }
    void beginGesture (const juce::String& id) override { if (auto* p = apvts.getParameter (id)) p->beginChangeGesture(); }
    void endGesture (const juce::String& id) override { if (auto* p = apvts.getParameter (id)) p->endChangeGesture(); }
    bool trimIncludesHit() const override { return false; }
    bool isBeatEnabled (int beat) const override { return gateMask.get (beat); }
    void toggleBeat (int beat) override { gateMask.set (beat, ! gateMask.get (beat)); }
    void setAllBeats (bool on) override { for (int i = 0; i < kMaxBeats; ++i) gateMask.set (i, on); }
    void shuffleBeats (bool restore) override;
    bool isShuffled() const override { return ! beatOrder.isIdentity(); }

    // stats for the UI
    struct Stats { int latency = 0; int captured = 0; int missed = 0; int dropped = 0; int active = 0; juce::int64 lastTrigger = -1; juce::int64 clock = 0; float inputDb = -100.0f; };
    Stats getStats() const;
    static constexpr int historyLen = 512;
    void copyHistory (std::vector<float>& db, std::vector<unsigned char>& trig) const;   // rolling meter
    void manualTrigger() { manualRequest = 1; }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

private:
    enum State { Free = 0, Capturing, Captured, Rendering, Ready, Playing };
    struct Job
    {
        std::atomic<int> state { Free };
        juce::AudioBuffer<float> hit;
        int hitLen = 0;
        float peakEnv = 0.0f;
        juce::int64 captureStart = 0;
        std::shared_ptr<juce::AudioBuffer<float>> swell;   // owned/replaced on render thread only
        const juce::AudioBuffer<float>* play = nullptr;      // what the audio thread reads
        int swellLen = 0;
        float gain = 1.0f;
        juce::int64 playStart = 0;
        int playPos = 0;
        bool frozen = false;
        int fadeRemaining = -1;   // >=0 while being stolen: fades out then frees
    };
    struct Voice { bool active = false; int pos = 0; float gain = 1.0f; juce::uint32 id = 0; const RenderedSample* render = nullptr; };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override { paramsDirty = true; }
    void timerCallback() override;
    void run() override;
    void renderJob (Job&);
    void updateLatency();
    void startCapture (juce::int64 c, int prerollAvail);
    void finishCapture();
    void startAudition (float gain, const RenderedSample* r);

    GateMask gateMask;
    BeatOrder beatOrder;
    juce::AudioBuffer<float> ring;
    int ringMask = 0;
    juce::int64 clock = 0;
    double hostSampleRate = 44100.0;
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<int> latency { 0 };
    std::atomic<int> maxSwellSamples { 44100 };
    int prerollSamples = 0;

    static constexpr int numJobs = 8;
    std::array<Job, numJobs> jobs;
    int capturingJob = -1;
    float fastEnv = 0.0f, slowEnv = 0.0f;
    float fastCoef = 0.0f, slowCoef = 0.0f;
    juce::int64 lastTrig = -1000000;
    bool armed = true;
    float lastTrigPeak = 0.0f;

    mutable juce::SpinLock renderLock;
    std::shared_ptr<RenderedSample> lastRendered;
    std::vector<std::shared_ptr<RenderedSample>> renderHistory;
    std::shared_ptr<juce::AudioBuffer<float>> frozenSwell;
    std::vector<std::shared_ptr<juce::AudioBuffer<float>>> frozenHistory;   // keeps frozen buffers alive until no job uses them
    bool lastFreeze = false;
    double lastBpm = 0.0;
    int frozenLen = 0;
    float frozenGain = 1.0f;
    std::atomic<bool> paramsDirty { false };
    std::atomic<bool> wantFreezeSnapshot { false };

    std::array<Voice, 4> voices;
    juce::uint32 voiceCounter = 0;
    std::atomic<int> auditionRequest { 0 }, manualRequest { 0 }, playhead { -1 };

    // stats / meter history
    std::atomic<int> captured { 0 }, missed { 0 }, dropped { 0 }, activeCount { 0 };
    std::atomic<juce::int64> lastTriggerClock { -1 }, clockShared { 0 };
    std::atomic<float> inputDb { -100.0f };
    std::array<std::atomic<float>, historyLen> histDb;
    std::array<std::atomic<unsigned char>, historyLen> histTrig;
    std::atomic<int> histWrite { 0 };
    int histAccum = 0; float histMax = 0.0f; bool histTrigFlag = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxProcessor)
};
