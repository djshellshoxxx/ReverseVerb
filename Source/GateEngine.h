#pragma once
#include "MusicalTime.h"
#include <array>
#include <cstdint>

namespace rv
{
struct GatePattern
{
    std::array<float, 32> steps { 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                  1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                  1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                  1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
    int activeSteps = 16;
    std::uint32_t seed = 0x52564732u;
};

[[nodiscard]] GatePattern sanitiseGatePattern (const GatePattern&) noexcept;

inline constexpr std::array<Division, 15> gateRateDivisions {
    Division::sixtyFourthTriplet, Division::sixtyFourth, Division::sixtyFourthDotted,
    Division::thirtySecondTriplet, Division::thirtySecond, Division::thirtySecondDotted,
    Division::sixteenthTriplet, Division::sixteenth, Division::sixteenthDotted,
    Division::eighthTriplet, Division::eighth, Division::eighthDotted,
    Division::quarterTriplet, Division::quarter, Division::quarterDotted
};

enum class GateShape : std::uint8_t { square, smoothSquare, rampUp, rampDown, triangle, sine, curved };
enum class GateRetrigger : std::uint8_t { note, host };
enum class GateTarget : std::uint8_t { swell, hit, both };

struct GateSettings
{
    Division rate = Division::sixteenth;
    int activeSteps = 16;
    float depth = 1.0f;
    float smoothingMilliseconds = 3.0f;
    float swing = 0.0f;
    float phase = 0.0f;
    GateRetrigger retrigger = GateRetrigger::note;
    GateTarget target = GateTarget::both;
    GateShape shape = GateShape::square;
};

class GateEngine
{
public:
    [[nodiscard]] float gainAt (const GatePattern&, const GateSettings&, const HostTiming&,
                                std::int64_t noteSample, std::int64_t hostSample) const noexcept;
    void reset (float gain = 1.0f) noexcept;
    void processGains (float* destination, int count, const GatePattern&, const GateSettings&,
                       const HostTiming&, std::int64_t noteSampleStart,
                       std::int64_t hostSampleStart) noexcept;
private:
    float smoothedGain = 1.0f;
};
}
