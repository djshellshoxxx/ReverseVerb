#pragma once

#include "GateEngine.h"
#include "RenderedSample.h"

namespace rv
{
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
                            bool gateEnabled) noexcept;
}
