#include "MusicalTime.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rv
{
namespace
{
struct Ratio
{
    std::int64_t numerator;
    std::int64_t denominator;
};

bool isValidTimeSignature (int numerator, int denominator) noexcept
{
    const bool denominatorIsPowerOfTwo = denominator > 0
                                      && (denominator & (denominator - 1)) == 0;
    return numerator >= 1 && numerator <= 64
        && denominatorIsPowerOfTwo && denominator <= 64;
}

TimeSignature sanitiseTimeSignature (TimeSignature value) noexcept
{
    return isValidTimeSignature (value.numerator, value.denominator)
             ? value
             : TimeSignature {};
}

Ratio noteRatio (Division division) noexcept
{
    switch (division)
    {
        case Division::sixtyFourthTriplet:  return { 1, 24 };
        case Division::sixtyFourth:         return { 1, 16 };
        case Division::sixtyFourthDotted:   return { 3, 32 };
        case Division::thirtySecondTriplet: return { 1, 12 };
        case Division::thirtySecond:        return { 1, 8 };
        case Division::thirtySecondDotted:  return { 3, 16 };
        case Division::sixteenthTriplet:    return { 1, 6 };
        case Division::sixteenth:           return { 1, 4 };
        case Division::sixteenthDotted:     return { 3, 8 };
        case Division::eighthTriplet:       return { 1, 3 };
        case Division::eighth:              return { 1, 2 };
        case Division::eighthDotted:        return { 3, 4 };
        case Division::quarterTriplet:      return { 2, 3 };
        case Division::quarter:             return { 1, 1 };
        case Division::quarterDotted:       return { 3, 2 };
        case Division::halfTriplet:         return { 4, 3 };
        case Division::half:                return { 2, 1 };
        case Division::halfDotted:          return { 3, 1 };
        case Division::whole:               return { 4, 1 };
        case Division::doubleWhole:         return { 8, 1 };
        case Division::oneBar:
        case Division::twoBars:
        case Division::fourBars:
        case Division::eightBars:
        case Division::count:
            break;
    }

    return { 1, 1 };
}

int barCount (Division division) noexcept
{
    switch (division)
    {
        case Division::oneBar:   return 1;
        case Division::twoBars:  return 2;
        case Division::fourBars: return 4;
        case Division::eightBars:return 8;
        default:                 return 0;
    }
}
}

std::string_view divisionLabel (Division division) noexcept
{
    switch (division)
    {
        case Division::sixtyFourthTriplet:  return "1/64T";
        case Division::sixtyFourth:         return "1/64";
        case Division::sixtyFourthDotted:   return "1/64D";
        case Division::thirtySecondTriplet: return "1/32T";
        case Division::thirtySecond:        return "1/32";
        case Division::thirtySecondDotted:  return "1/32D";
        case Division::sixteenthTriplet:    return "1/16T";
        case Division::sixteenth:           return "1/16";
        case Division::sixteenthDotted:     return "1/16D";
        case Division::eighthTriplet:       return "1/8T";
        case Division::eighth:              return "1/8";
        case Division::eighthDotted:        return "1/8D";
        case Division::quarterTriplet:      return "1/4T";
        case Division::quarter:             return "1/4";
        case Division::quarterDotted:       return "1/4D";
        case Division::halfTriplet:         return "1/2T";
        case Division::half:                return "1/2";
        case Division::halfDotted:          return "1/2D";
        case Division::whole:               return "1/1";
        case Division::doubleWhole:         return "2/1";
        case Division::oneBar:              return "1 bar";
        case Division::twoBars:             return "2 bars";
        case Division::fourBars:            return "4 bars";
        case Division::eightBars:           return "8 bars";
        case Division::count:               break;
    }

    return "1/4";
}

Division legacyDivision (int legacyChoice) noexcept
{
    switch (legacyChoice)
    {
        case 0:  return Division::quarter;
        case 1:  return Division::half;
        case 2:  return Division::whole;
        case 3:  return Division::doubleWhole;
        case 4:  return Division::oneBar;
        case 5:  return Division::twoBars;
        case 6:  return Division::fourBars;
        default: return Division::whole;
    }
}

double quarterNotes (Division division, TimeSignature timeSignature) noexcept
{
    if (const auto bars = barCount (division); bars > 0)
    {
        const auto valid = sanitiseTimeSignature (timeSignature);
        return static_cast<double> (bars * valid.numerator * 4) / valid.denominator;
    }

    const auto ratio = noteRatio (division);
    return static_cast<double> (ratio.numerator) / static_cast<double> (ratio.denominator);
}

std::int64_t durationSamples (Division division,
                              double bpm,
                              double sampleRate,
                              TimeSignature timeSignature) noexcept
{
    if (! std::isfinite (sampleRate) || sampleRate <= 0.0)
        return 0;

    const auto timing = sanitiseTiming (bpm,
                                        timeSignature.numerator,
                                        timeSignature.denominator,
                                        0.0);
    const long double seconds = static_cast<long double> (quarterNotes (division, timing.timeSignature))
                              * 60.0L / static_cast<long double> (timing.bpm);
    const long double samples = seconds * static_cast<long double> (sampleRate);
    const long double maximum = static_cast<long double> (std::numeric_limits<std::int64_t>::max());

    if (! std::isfinite (samples) || samples >= maximum)
        return std::numeric_limits<std::int64_t>::max();

    return std::max<std::int64_t> (0, static_cast<std::int64_t> (std::llround (samples)));
}

HostTiming sanitiseTiming (double bpm,
                           int numerator,
                           int denominator,
                           double ppqPosition) noexcept
{
    HostTiming result;
    result.bpm = std::isfinite (bpm) && bpm > 0.0
                   ? std::clamp (bpm, minimumBpm, maximumBpm)
                   : defaultBpm;
    result.timeSignature = sanitiseTimeSignature ({ numerator, denominator });

    if (std::isfinite (ppqPosition))
    {
        result.ppqPosition = ppqPosition;
        result.hasPpqPosition = true;
    }

    return result;
}
}
