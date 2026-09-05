#include "GatedMixer.h"

#include <algorithm>
#include <array>

namespace rv
{
namespace
{
float sampleFrom (const juce::AudioBuffer<float>& source, int channel, int sample) noexcept
{
    if (source.getNumChannels() <= 0 || sample < 0 || sample >= source.getNumSamples())
        return 0.0f;
    return source.getSample (std::min (channel, source.getNumChannels() - 1), sample);
}
}

void mixGatedRenderedRange (juce::AudioBuffer<float>& destination,
                            const RenderedSample& rendered,
                            int sourceStart,
                            int destinationStart,
                            int count,
                            float wetGain,
                            float dryGain,
                            GateEngine& gate,
                            const GatePattern& pattern,
                            const GateSettings& settings,
                            const HostTiming& timing,
                            std::int64_t noteSampleStart,
                            std::int64_t hostSampleStart,
                            bool gateEnabled) noexcept
{
    if (count <= 0 || destination.getNumChannels() <= 0 || destination.getNumSamples() <= 0)
        return;

    if (sourceStart < 0)
    {
        const auto skipped = -sourceStart;
        sourceStart = 0;
        destinationStart += skipped;
        noteSampleStart += skipped;
        hostSampleStart += skipped;
        count -= skipped;
    }
    if (destinationStart < 0)
    {
        const auto skipped = -destinationStart;
        destinationStart = 0;
        sourceStart += skipped;
        noteSampleStart += skipped;
        hostSampleStart += skipped;
        count -= skipped;
    }

    count = std::min (count, destination.getNumSamples() - destinationStart);
    count = std::min (count, rendered.getNumSamples() - sourceStart);
    if (count <= 0)
        return;

    if (! gateEnabled)
    {
        mixRenderedRange (destination, rendered, sourceStart, destinationStart, count, wetGain, dryGain);
        return;
    }

    constexpr int chunkCapacity = 512;
    std::array<float, chunkCapacity> gateGains {};
    const bool gateWet = settings.target == GateTarget::swell || settings.target == GateTarget::both;
    const bool gateDry = settings.target == GateTarget::hit || settings.target == GateTarget::both;

    for (int offset = 0; offset < count; offset += chunkCapacity)
    {
        const auto chunk = std::min (chunkCapacity, count - offset);
        gate.processGains (gateGains.data(), chunk, pattern, settings, timing,
                           noteSampleStart + offset, hostSampleStart + offset);

        for (int channel = 0; channel < destination.getNumChannels(); ++channel)
        {
            auto* output = destination.getWritePointer (channel, destinationStart + offset);
            for (int sample = 0; sample < chunk; ++sample)
            {
                const auto sourceSample = sourceStart + offset + sample;
                const auto gain = gateGains[(size_t) sample];
                const auto wet = sampleFrom (rendered.wetAudio, channel, sourceSample)
                               * wetGain * (gateWet ? gain : 1.0f);
                const auto dry = sampleFrom (rendered.dryAudio, channel, sourceSample)
                               * dryGain * (gateDry ? gain : 1.0f);
                output[sample] += wet + dry;
            }
        }
    }
}
}
