#pragma once

#include <JuceHeader.h>

namespace rv
{
struct SampleLoadResult
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 0.0;
    juce::File file;
    juce::Array<juce::File> folderFiles;
    int currentIndex = -1;
    juce::String error;

    bool succeeded() const noexcept { return audio.getNumSamples() > 0 && sampleRate > 0.0; }
};

SampleLoadResult loadSampleSnapshot (const juce::File&, juce::AudioFormatManager&);
}
