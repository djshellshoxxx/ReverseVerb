#include "RenderedSample.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
void addLayer (juce::AudioBuffer<float>& destination,
               const juce::AudioBuffer<float>& source,
               int sourceStart,
               int destinationStart,
               int count,
               float gain) noexcept
{
    if (count <= 0 || gain == 0.0f || ! std::isfinite (gain)
        || source.getNumChannels() <= 0 || source.getNumSamples() <= 0)
        return;

    const auto available = juce::jmin (count, source.getNumSamples() - sourceStart);
    if (available <= 0)
        return;

    for (int channel = 0; channel < destination.getNumChannels(); ++channel)
    {
        const auto sourceChannel = juce::jmin (channel, source.getNumChannels() - 1);
        destination.addFrom (channel, destinationStart,
                             source, sourceChannel, sourceStart, available, gain);
    }
}
}

LayerTimeline calculateLayerTimeline (RenderDirection direction,
                                      int wetLength,
                                      int dryLength,
                                      int delaySamples) noexcept
{
    const auto wet = std::max (0, wetLength);
    const auto dry = std::max (0, dryLength);
    const auto delay = std::max (0, delaySamples);
    const auto limit = static_cast<long long> (std::numeric_limits<int>::max());

    LayerTimeline result;
    if (direction == RenderDirection::rise)
    {
        result.wetStart = 0;
        result.dryStart = static_cast<int> (std::min (limit,
                                                      static_cast<long long> (wet) + delay));
    }
    else
    {
        result.dryStart = 0;
        result.wetStart = delay;
    }

    const auto wetEnd = static_cast<long long> (result.wetStart) + wet;
    const auto dryEnd = static_cast<long long> (result.dryStart) + dry;
    result.totalLength = static_cast<int> (std::min (limit, std::max (wetEnd, dryEnd)));
    return result;
}

int wetTailSamplesForTimeline (RenderDirection direction,
                               std::int64_t targetLength,
                               int dryLength,
                               int delaySamples,
                               std::int64_t minimumTailLength) noexcept
{
    const auto dry = static_cast<std::int64_t> (std::max (0, dryLength));
    const auto delay = static_cast<std::int64_t> (std::max (0, delaySamples));
    const auto fixedLength = direction == RenderDirection::rise ? dry * 2 + delay
                                                                 : dry + delay;
    const auto requested = std::max<std::int64_t> ({ 1, minimumTailLength,
                                                     std::max<std::int64_t> (0, targetLength) - fixedLength });
    return static_cast<int> (std::min<std::int64_t> (requested,
                                                     std::numeric_limits<int>::max() - dry));
}

int latencySamplesFor (const RenderedSample& rendered, bool alignDryHit) noexcept
{
    if (! alignDryHit || rendered.direction != RenderDirection::rise)
        return 0;

    return std::max (0, rendered.dryHitIndex);
}

void mixRenderedRange (juce::AudioBuffer<float>& destination,
                       const RenderedSample& rendered,
                       int sourceStart,
                       int destinationStart,
                       int count,
                       float wetGain,
                       float dryGain) noexcept
{
    if (count <= 0 || destination.getNumChannels() <= 0 || destination.getNumSamples() <= 0)
        return;

    if (sourceStart < 0)
    {
        const auto skipped = -sourceStart;
        sourceStart = 0;
        destinationStart += skipped;
        count -= skipped;
    }

    if (destinationStart < 0)
    {
        const auto skipped = -destinationStart;
        destinationStart = 0;
        sourceStart += skipped;
        count -= skipped;
    }

    count = juce::jmin (count, destination.getNumSamples() - destinationStart);
    if (count <= 0)
        return;

    addLayer (destination, rendered.wetAudio, sourceStart, destinationStart, count, wetGain);
    addLayer (destination, rendered.dryAudio, sourceStart, destinationStart, count, dryGain);
}
