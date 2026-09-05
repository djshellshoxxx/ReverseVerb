#pragma once
#include <JuceHeader.h>
#include "GateEngine.h"
#include "GatePatternState.h"
#include "GatedMixer.h"
#include "MusicalTime.h"
#include "RenderedSample.h"
#include "SampleGenerator.h"

namespace IDs
{
    static const juce::String dry = "dry", wet = "wet";
    static const juce::String size = "size", decay = "decay", damp = "damp", diff = "diff", er = "er", sep = "sep", width = "width", gap = "gap";
    static const juce::String tail = "tail", shape = "shape", tone = "tone", basscut = "basscut";
    static const juce::String align = "align";
    static const juce::String trimStart = "trimStart", trimEnd = "trimEnd";
    static const juce::String sync = "sync", syncLen = "syncLen", syncDivisionV2 = "syncDivisionV2";
    static const juce::String direction = "direction";
    static const juce::String pitch = "pitch", pitchRange = "pitchRange", pitchTension = "pitchTension";
    static const juce::String volStart = "volStart", volEnd = "volEnd", volTension = "volTension";
    static const juce::String gateEnabled = "gateEnabled", gateSteps = "gateSteps", gateRate = "gateRate";
    static const juce::String gateDepth = "gateDepth", gateSmooth = "gateSmooth", gateSwing = "gateSwing", gatePhase = "gatePhase";
    static const juce::String gateRetrigger = "gateRetrigger", gateTarget = "gateTarget", gateShape = "gateShape";
}

// FL-style tension curve: x in 0..1 -> 0..1. t>0 = slow start, t<0 = fast start.
inline float tensionCurve (float x, float t)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    if (std::abs (t) < 0.001f) return x;
    const float k = 1.0f + 5.0f * std::abs (t);
    return t > 0.0f ? std::pow (x, k) : 1.0f - std::pow (1.0f - x, k);
}

class ReverseVerbProcessor : public juce::AudioProcessor,
                             private juce::Timer,
                             private juce::AudioProcessorValueTreeState::Listener
{
private:
    juce::UndoManager undoManager;

public:
    ReverseVerbProcessor();
    ~ReverseVerbProcessor() override;

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
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool loadSampleFile (const juce::File& f, bool previewAfter = false);
    void nextSample();
    void prevSample();
    juce::File getCurrentFile() const { return currentFile; }
    int getSampleIndex() const { return currentIndex; }
    int getSampleCount() const { return folderFiles.size(); }

    bool generateSample (rv::GeneratedSampleType type);
    juce::String getDisplayLabel() const;

    void triggerPreview() { triggerRequest = 1; }
    void stopAll() { stopRequest = 1; }
    bool exportWav (const juce::File& dest);
    void resetEdits();
    void randomizeReverb();
    std::shared_ptr<const rv::GatePattern> getGatePattern() const;
    void setGateStep (int step, float value, const juce::String& transactionName = "Edit gate step");
    void replaceGatePattern (const rv::GatePattern&, const juce::String& transactionName = "Edit gate pattern");
    void clearGatePattern();
    void fillGatePattern();
    void invertGatePattern();
    void randomizeGatePattern();
    void rotateGatePattern (int amount);
    void refreshGatePatternFromState();
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }

    std::shared_ptr<const RenderedSample> getRendered() const;
    int getPlayheadPosition() const { return playhead.load(); }
    double getHostBpm() const { return hostBpm.load(); }
    rv::HostTiming getHostTiming() const noexcept;
    rv::Division getSyncDivision() const noexcept;
    rv::GateSettings getGateSettings() const noexcept;
    bool isUsingV2SyncDivision() const noexcept { return useV2SyncDivision.load(); }
    RenderDirection getDirection() const noexcept;
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float value);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    void render();
    void refreshFolderList (const juce::File& f);
    void publishGatePattern (const rv::GatePattern&);
    void storeGatePatternInState (const rv::GatePattern&, juce::UndoManager*);

    struct Voice
    {
        bool active = false;
        int pos = 0;
        float gain = 1.0f;
        juce::uint32 id = 0;
        rv::GateEngine gate;
    };
    void startVoice (float gain);
    void renderRange (juce::AudioBuffer<float>& out, const RenderedSample& r,
                      int start, int num, float dry, float wet,
                      const rv::GatePattern&, const rv::GateSettings&,
                      const rv::HostTiming&, bool gateEnabled);

    juce::AudioFormatManager formatManager;
    juce::CriticalSection sourceLock;
    juce::AudioBuffer<float> sourceBuffer;
    double sourceSR = 44100.0;
    juce::File currentFile;
    juce::Array<juce::File> folderFiles;
    int currentIndex = -1;
    juce::String generatedLabel;

    std::shared_ptr<const RenderedSample> rendered;
    std::shared_ptr<const rv::GatePattern> gatePattern;

    std::atomic<double> hostSampleRate { 44100.0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<int> hostTimeSigNumerator { 4 }, hostTimeSigDenominator { 4 };
    std::atomic<double> hostPpqPosition { 0.0 }, hostLoopStartPpq { 0.0 }, hostLoopEndPpq { 0.0 };
    std::atomic<bool> hostHasPpq { false }, hostIsPlaying { false }, hostIsLooping { false }, hostHasLoopRange { false };
    std::atomic<bool> useV2SyncDivision { true };
    double lastRenderBpm = 0.0;
    rv::TimeSignature lastRenderTimeSignature {};
    std::atomic<bool> dirty { false }, previewAfterRender { false };
    std::atomic<int> triggerRequest { 0 }, stopRequest { 0 }, playhead { -1 };

    std::array<Voice, 8> voices;
    juce::uint32 voiceCounter = 0;
    std::atomic<float>* dryParam = nullptr;
    std::atomic<float>* wetParam = nullptr;
    std::atomic<float>* gateEnabledParam = nullptr;
    std::atomic<float>* gateStepsParam = nullptr;
    std::atomic<float>* gateRateParam = nullptr;
    std::atomic<float>* gateDepthParam = nullptr;
    std::atomic<float>* gateSmoothParam = nullptr;
    std::atomic<float>* gateSwingParam = nullptr;
    std::atomic<float>* gatePhaseParam = nullptr;
    std::atomic<float>* gateRetriggerParam = nullptr;
    std::atomic<float>* gateTargetParam = nullptr;
    std::atomic<float>* gateShapeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseVerbProcessor)
};
