#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace rv
{
constexpr double defaultBpm = 120.0;
constexpr double minimumBpm = 20.0;
constexpr double maximumBpm = 999.0;

struct TimeSignature
{
    int numerator = 4;
    int denominator = 4;
};

struct HostTiming
{
    double bpm = defaultBpm;
    TimeSignature timeSignature {};
    double ppqPosition = 0.0;
    bool hasPpqPosition = false;

    // Filled by the processor when transport information is captured. Keeping
    // these fields in the snapshot lets the gate use the same timing model.
    double sampleRate = 44100.0;
    bool isPlaying = false;
    bool isLooping = false;
    bool hasLoopRange = false;
    double loopStartPpq = 0.0;
    double loopEndPpq = 0.0;
};

enum class Division : std::uint8_t
{
    sixtyFourthTriplet,
    sixtyFourth,
    sixtyFourthDotted,
    thirtySecondTriplet,
    thirtySecond,
    thirtySecondDotted,
    sixteenthTriplet,
    sixteenth,
    sixteenthDotted,
    eighthTriplet,
    eighth,
    eighthDotted,
    quarterTriplet,
    quarter,
    quarterDotted,
    halfTriplet,
    half,
    halfDotted,
    whole,
    doubleWhole,
    oneBar,
    twoBars,
    fourBars,
    eightBars,
    count
};

inline constexpr std::array<Division, static_cast<std::size_t> (Division::count)> allDivisions {
    Division::sixtyFourthTriplet,
    Division::sixtyFourth,
    Division::sixtyFourthDotted,
    Division::thirtySecondTriplet,
    Division::thirtySecond,
    Division::thirtySecondDotted,
    Division::sixteenthTriplet,
    Division::sixteenth,
    Division::sixteenthDotted,
    Division::eighthTriplet,
    Division::eighth,
    Division::eighthDotted,
    Division::quarterTriplet,
    Division::quarter,
    Division::quarterDotted,
    Division::halfTriplet,
    Division::half,
    Division::halfDotted,
    Division::whole,
    Division::doubleWhole,
    Division::oneBar,
    Division::twoBars,
    Division::fourBars,
    Division::eightBars,
};

[[nodiscard]] std::string_view divisionLabel (Division division) noexcept;
[[nodiscard]] Division legacyDivision (int legacyChoice) noexcept;
[[nodiscard]] double quarterNotes (Division division, TimeSignature timeSignature) noexcept;
[[nodiscard]] std::int64_t durationSamples (Division division,
                                            double bpm,
                                            double sampleRate,
                                            TimeSignature timeSignature) noexcept;
[[nodiscard]] HostTiming sanitiseTiming (double bpm,
                                         int numerator,
                                         int denominator,
                                         double ppqPosition) noexcept;
}
