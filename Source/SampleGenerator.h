#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>

namespace rv
{
enum class GeneratedSampleType : std::uint8_t { snare, hat, clap };

// Synthesizes a short stereo one-shot at the given sample rate. The same seed
// always reproduces the same hit; a fresh seed (e.g. juce::Random::getSystemRandom())
// gives a new variation. Intended as raw material for the Rise/Fall reverb engine,
// not as a finished drum sound on its own.
[[nodiscard]] juce::AudioBuffer<float> generateSample (GeneratedSampleType type,
                                                        double sampleRate,
                                                        juce::uint32 seed) noexcept;

[[nodiscard]] const char* generatedSampleName (GeneratedSampleType type) noexcept;
}
