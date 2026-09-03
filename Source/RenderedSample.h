#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

enum class RenderDirection
{
    rise,
    fall
};

struct LayerTimeline
{
    int wetStart = 0;
    int dryStart = 0;
    int totalLength = 0;
};

[[nodiscard]] LayerTimeline calculateLayerTimeline (RenderDirection direction,
                                                     int wetLength,
                                                     int dryLength,
                                                     int delaySamples) noexcept;

struct RenderedSample
{
    // Wet and dry remain aligned on the same rendered timeline. This is
    // essential in Fall mode, where the two layers can overlap.
    juce::AudioBuffer<float> wetAudio;
    juce::AudioBuffer<float> dryAudio;
    juce::AudioBuffer<float> displayAudio;

    int dryHitIndex = -1;
    int dryStart = -1;
    int dryEnd = -1;
    int wetStart = -1;
    int wetEnd = -1;
    RenderDirection direction = RenderDirection::rise;
    double sampleRate = 44100.0;
    int beats = 0;
    int beatsPerBar = 4;
    double fullLengthSec = 0.0;
    double trimStartSec = 0.0;
    double trimEndSec = 0.0;
    std::vector<float> pitchSemi;
    std::vector<float> gainLin;
    static constexpr int envStep = 256;

    [[nodiscard]] int getNumSamples() const noexcept
    {
        return juce::jmax (wetAudio.getNumSamples(), dryAudio.getNumSamples());
    }
};

[[nodiscard]] int latencySamplesFor (const RenderedSample& rendered,
                                     bool alignDryHit) noexcept;

// Adds an aligned range from both layers into the destination. All ranges are
// clipped, so callers may pass blocks that cross the beginning or end.
void mixRenderedRange (juce::AudioBuffer<float>& destination,
                       const RenderedSample& rendered,
                       int sourceStart,
                       int destinationStart,
                       int count,
                       float wetGain,
                       float dryGain) noexcept;
