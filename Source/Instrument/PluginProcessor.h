#pragma once
#include <JuceHeader.h>
#include "../Common/Core.h"

class ReverseVerbProcessor : public juce::AudioProcessor,
                             public RVHost,
                             private juce::Timer,
                             private juce::AudioProcessorValueTreeState::Listener
{
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
    double getTailLengthSeconds() const override { return tailSeconds.load(); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool loadSampleFile (const juce::File& f, bool previewAfter = false);
    void generate (int type);            // 0 hat 1 snare 2 clap 3 kick 4 rim
    void nextSample();
    void prevSample();
    juce::File getCurrentFile() const { return currentFile; }
    juce::String getSourceName() const { return currentFile.existsAsFile() ? currentFile.getFileName() : generatedName; }
    int getSampleIndex() const { return currentIndex; }
    int getSampleCount() const { return folderFiles.size(); }

    // RVHost
    void triggerPreview() override { triggerRequest = 1; }
    bool exportWav (const juce::File& dest) override;
    juce::String exportBaseName() const override { return currentFile.existsAsFile() ? currentFile.getFileNameWithoutExtension() : generatedName.isNotEmpty() ? generatedName : juce::String(); }
    std::shared_ptr<const RenderedSample> getRendered() const override;
    int getPlayheadPosition() const override { return playhead.load(); }
    double getHostBpm() const override { return hostBpm.load(); }
    float param (const juce::String& id) const override { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float value) override;
    void setParamValue (const juce::String& id, float value) override { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value)); }
    void beginGesture (const juce::String& id) override { if (auto* p = apvts.getParameter (id)) p->beginChangeGesture(); }
    void endGesture (const juce::String& id) override { if (auto* p = apvts.getParameter (id)) p->endChangeGesture(); }
    bool isBeatEnabled (int beat) const override { return gateMask.get (beat); }
    void toggleBeat (int beat) override { gateMask.set (beat, ! gateMask.get (beat)); dirty = true; }
    void setAllBeats (bool on) override { for (int i = 0; i < kMaxBeats; ++i) gateMask.set (i, on); dirty = true; }
    void shuffleBeats (bool restore) override;
    bool isShuffled() const override { return ! beatOrder.isIdentity(); }
    void stopAll() { stopRequest = 1; }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override { dirty = true; }
    void timerCallback() override;
    void render();
    void refreshFolderList (const juce::File& f);
    int currentBeats() const;

    struct Voice { bool active = false; int pos = 0; float gain = 1.0f; juce::uint32 id = 0; const RenderedSample* render = nullptr; };
    void startVoice (float gain, const RenderedSample* r);
    void renderRange (juce::AudioBuffer<float>& out, int start, int num, float dry, float wet);

    juce::AudioFormatManager formatManager;
    juce::CriticalSection sourceLock;
    juce::AudioBuffer<float> sourceBuffer;
    double sourceSR = 44100.0;
    juce::File currentFile;
    juce::String generatedName;
    juce::File missingFile;
public:
    juce::File getMissingFile() const { return missingFile; }
private:
    int generatedType = -1;
    juce::Array<juce::File> folderFiles;
    int currentIndex = -1;

    GateMask gateMask;
    BeatOrder beatOrder;

    mutable juce::SpinLock renderLock;
    std::shared_ptr<RenderedSample> rendered;
    std::vector<std::shared_ptr<RenderedSample>> renderHistory;   // keeps renders alive while voices use them; pruned on the message thread
    std::atomic<double> tailSeconds { 0.0 };

    double hostSampleRate = 44100.0;
    std::atomic<double> hostBpm { 120.0 };
    double lastRenderBpm = 0.0;
    std::atomic<bool> dirty { false }, previewAfterRender { false };
    std::atomic<int> triggerRequest { 0 }, stopRequest { 0 }, playhead { -1 };

    std::array<Voice, 8> voices;
    juce::uint32 voiceCounter = 0;
    std::atomic<float>* dryParam = nullptr;
    std::atomic<float>* wetParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseVerbProcessor)
};
