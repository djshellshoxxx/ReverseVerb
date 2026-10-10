// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "OutputStage.h"
#include "PresetManager.h"
#include "Voices.h"
#include "dsp/RenderEngine.h"

class ReverseVerbProcessor : public juce::AudioProcessor,
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

    PresetManager& getPresets() { return *presets; }

    int getUiWidth() const { return uiW.load(); }
    int getUiHeight() const { return uiH.load(); }
    void setUiSize (int w, int h) { uiW = w; uiH = h; }

    void renderBlocking();                          // render now with current parameters (message / non-realtime thread)
    float takePeak (int ch) { return outputStage.takePeak (ch); }
    bool clipLatched() const { return outputStage.clipLatched(); }
    void clearClip() { outputStage.clearClip(); }

    void triggerPreview() { triggerRequest = 1; }
    void stopAll() { stopRequest = 1; }
    bool exportWav (const juce::File& dest);
    void resetEdits();
    void randomizeReverb();

    std::shared_ptr<const RenderedSample> getRendered() const;
    int getPlayheadPosition() const { return playhead.load(); }
    double getHostBpm() const { return hostBpm.load(); }
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float value);

    juce::AudioProcessorValueTreeState apvts;

private:
    void parameterChanged (const juce::String&, float) override { dirty = true; }
    void timerCallback() override;
    void render();
    void refreshFolderList (const juce::File& f);

    RenderSettings currentSettings() const;

    juce::AudioFormatManager formatManager;
    juce::CriticalSection sourceLock;
    std::shared_ptr<const juce::AudioBuffer<float>> sourceBuffer;
    double sourceSR = 44100.0;
    juce::File currentFile;
    juce::Array<juce::File> folderFiles;
    int currentIndex = -1;

    // Rendering runs on a background thread so the GUI never blocks on a reverb render.
    struct RenderThread : public juce::Thread
    {
        explicit RenderThread (ReverseVerbProcessor& o) : juce::Thread ("ReverseVerb render"), owner (o) {}
        void run() override { while (! threadShouldExit()) { wait (-1); if (threadShouldExit()) break; owner.render(); sleep (20); } }   // >=20 ms between swaps (crossfade safety)
        ReverseVerbProcessor& owner;
    };
    std::unique_ptr<RenderThread> renderThread;
    juce::CriticalSection renderMutex;                  // serialises render() (bg thread + export)

    RenderCache cache;
    int sourceVersion = 0;                              // guarded by sourceLock
    std::atomic<int> pendingLatency { -1 };             // applied on the message thread
    std::array<std::shared_ptr<RenderedSample>, 8> retired;  // old renders freed on the render thread, never the audio thread
    unsigned retireIdx = 0;

    mutable juce::SpinLock renderLock;
    std::shared_ptr<RenderedSample> rendered;

    std::atomic<double> hostSampleRate { 44100.0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> lastRenderBpm { 0.0 };
    std::atomic<bool> dirty { false }, previewAfterRender { false };
    std::atomic<int> triggerRequest { 0 }, stopRequest { 0 }, playhead { -1 };

    std::atomic<int> uiW { 1060 }, uiH { 720 };          // editor size, saved with the project
    std::unique_ptr<PresetManager> presets;
    VoiceBank voices;
    OutputStage outputStage;
    std::shared_ptr<const RenderedSample> lastBuffer;   // audio thread only: detects buffer swaps for the crossfade
    std::atomic<float>* dryParam = nullptr;
    std::atomic<float>* wetParam = nullptr;
    std::atomic<float>* alignParam = nullptr;
    std::atomic<float>* keytrackParam = nullptr;
    std::atomic<float>* rootParam = nullptr;
    std::atomic<float>* outGainParam = nullptr;
    std::atomic<float>* limiterParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseVerbProcessor)
};
