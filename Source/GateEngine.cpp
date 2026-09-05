#include "GateEngine.h"
#include <algorithm>
#include <cmath>

namespace rv
{
namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
struct StepPosition { int index = 0; double phase = 0.0; };

double wrapPositive (double value, double length) noexcept
{
    if (! std::isfinite (value) || ! std::isfinite (length) || length <= 0.0) return 0.0;
    const auto wrapped = std::fmod (value, length);
    return wrapped < 0.0 ? wrapped + length : wrapped;
}

StepPosition stepPosition (double position, double baseLength, int activeSteps, float swing) noexcept
{
    const auto steps = std::clamp (activeSteps, 1, 32);
    const auto length = std::isfinite (baseLength) && baseLength > 0.0 ? baseLength : 0.25;
    const auto swingAmount = std::clamp (std::isfinite (swing) ? (double) swing : 0.0, 0.0, 0.75);
    const auto wrapped = wrapPositive (position, steps * length);
    const auto pairLength = 2.0 * length;
    const auto pair = static_cast<int> (std::floor (wrapped / pairLength));
    const auto within = wrapped - pair * pairLength;
    const auto first = length * (1.0 + swingAmount);
    const auto second = length * (1.0 - swingAmount);
    StepPosition result;
    if (within < first) { result.index = pair * 2; result.phase = within / first; }
    else { result.index = pair * 2 + 1; result.phase = (within - first) / second; }
    result.index %= steps;
    result.phase = std::clamp (result.phase, 0.0, 1.0);
    return result;
}

float shapeGain (GateShape shape, double phase) noexcept
{
    const auto x = std::clamp (phase, 0.0, 1.0);
    switch (shape)
    {
        case GateShape::square: return 1.0f;
        case GateShape::smoothSquare:
        {
            constexpr double edge = 0.15;
            if (x < edge) return (float) (0.5 - 0.5 * std::cos (pi * x / edge));
            if (x > 1.0 - edge) return (float) (0.5 - 0.5 * std::cos (pi * (1.0 - x) / edge));
            return 1.0f;
        }
        case GateShape::rampUp: return (float) x;
        case GateShape::rampDown: return (float) (1.0 - x);
        case GateShape::triangle: return (float) (1.0 - std::abs (2.0 * x - 1.0));
        case GateShape::sine: return (float) std::sin (pi * x);
        case GateShape::curved: return (float) std::pow (std::max (0.0, std::sin (pi * x)), 0.35);
    }
    return 1.0f;
}
}

GatePattern sanitiseGatePattern (const GatePattern& pattern) noexcept
{
    auto result = pattern;
    result.activeSteps = pattern.activeSteps <= 16 ? 16 : 32;
    for (auto& step : result.steps)
        step = std::clamp (std::isfinite (step) ? step : 0.0f, 0.0f, 1.0f);
    return result;
}

float GateEngine::gainAt (const GatePattern& pattern, const GateSettings& settings,
                          const HostTiming& timing, std::int64_t noteSample,
                          std::int64_t hostSample) const noexcept
{
    const auto validTiming = sanitiseTiming (timing.bpm, timing.timeSignature.numerator,
                                             timing.timeSignature.denominator,
                                             timing.hasPpqPosition ? timing.ppqPosition : NAN);
    const auto sampleRate = std::isfinite (timing.sampleRate) && timing.sampleRate > 0.0 ? timing.sampleRate : 44100.0;
    const auto samplesToQuarterNotes = validTiming.bpm / (60.0 * sampleRate);
    const bool useHost = settings.retrigger == GateRetrigger::host && validTiming.hasPpqPosition;
    const auto position = useHost ? validTiming.ppqPosition + std::max<std::int64_t> (0, hostSample) * samplesToQuarterNotes
                                  : std::max<std::int64_t> (0, noteSample) * samplesToQuarterNotes;
    const auto length = quarterNotes (settings.rate, validTiming.timeSignature);
    const auto steps = settings.activeSteps <= 16 ? 16 : 32;
    const auto phase = std::clamp (std::isfinite (settings.phase) ? (double) settings.phase : 0.0, 0.0, 1.0);
    const auto step = stepPosition (position + phase * steps * length, length, steps, settings.swing);
    const auto raw = pattern.steps[(size_t) step.index];
    const auto level = std::clamp (std::isfinite (raw) ? raw : 0.0f, 0.0f, 1.0f) * shapeGain (settings.shape, step.phase);
    const auto depth = std::clamp (std::isfinite (settings.depth) ? settings.depth : 1.0f, 0.0f, 1.0f);
    return std::clamp (1.0f - depth + depth * level, 0.0f, 1.0f);
}

void GateEngine::reset (float gain) noexcept
{
    smoothedGain = std::clamp (std::isfinite (gain) ? gain : 1.0f, 0.0f, 1.0f);
}

void GateEngine::processGains (float* destination, int count, const GatePattern& pattern,
                               const GateSettings& settings, const HostTiming& timing,
                               std::int64_t noteStart, std::int64_t hostStart) noexcept
{
    if (destination == nullptr || count <= 0) return;
    const auto sr = std::isfinite (timing.sampleRate) && timing.sampleRate > 0.0 ? timing.sampleRate : 44100.0;
    const auto ms = std::clamp (std::isfinite (settings.smoothingMilliseconds)
                                  ? (double) settings.smoothingMilliseconds : 0.0, 0.0, 500.0);
    const auto coefficient = ms > 0.0 ? (float) std::exp (-1.0 / (ms * 0.001 * sr)) : 0.0f;
    for (int sample = 0; sample < count; ++sample)
    {
        const auto target = gainAt (pattern, settings, timing, noteStart + sample, hostStart + sample);
        smoothedGain = target + coefficient * (smoothedGain - target);
        if (! std::isfinite (smoothedGain)) smoothedGain = target;
        destination[sample] = std::clamp (smoothedGain, 0.0f, 1.0f);
    }
}
}
