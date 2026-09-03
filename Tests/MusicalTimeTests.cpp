#include <juce_core/juce_core.h>

#include "../Source/MusicalTime.h"

#include <array>
#include <cmath>

namespace
{
class MusicalTimeTests final : public juce::UnitTest
{
public:
    MusicalTimeTests() : juce::UnitTest ("Musical time", "DSP") {}

    void runTest() override
    {
        testDivisionRatios();
        testBarTimeSignatures();
        testSampleRates();
        testTimingSanitisation();
    }

private:
    void expectNear (double actual, double expected, double tolerance = 1.0e-12)
    {
        expect (std::abs (actual - expected) <= tolerance,
                "Expected " + juce::String (expected, 12)
                    + ", got " + juce::String (actual, 12));
    }

    void testDivisionRatios()
    {
        beginTest ("Every note division has the expected quarter-note ratio");

        using D = rv::Division;
        struct Case { D division; double quarterNotes; };
        constexpr std::array cases {
            Case { D::sixtyFourthTriplet,  1.0 / 24.0 },
            Case { D::sixtyFourth,         1.0 / 16.0 },
            Case { D::sixtyFourthDotted,   3.0 / 32.0 },
            Case { D::thirtySecondTriplet, 1.0 / 12.0 },
            Case { D::thirtySecond,        1.0 / 8.0 },
            Case { D::thirtySecondDotted,  3.0 / 16.0 },
            Case { D::sixteenthTriplet,    1.0 / 6.0 },
            Case { D::sixteenth,           1.0 / 4.0 },
            Case { D::sixteenthDotted,     3.0 / 8.0 },
            Case { D::eighthTriplet,       1.0 / 3.0 },
            Case { D::eighth,              1.0 / 2.0 },
            Case { D::eighthDotted,        3.0 / 4.0 },
            Case { D::quarterTriplet,      2.0 / 3.0 },
            Case { D::quarter,             1.0 },
            Case { D::quarterDotted,       3.0 / 2.0 },
            Case { D::halfTriplet,         4.0 / 3.0 },
            Case { D::half,                2.0 },
            Case { D::halfDotted,          3.0 },
            Case { D::whole,               4.0 },
            Case { D::doubleWhole,         8.0 },
        };

        for (const auto& c : cases)
            expectNear (rv::quarterNotes (c.division, { 4, 4 }), c.quarterNotes);
    }

    void testBarTimeSignatures()
    {
        beginTest ("Bar divisions follow the host time signature");

        struct SignatureCase { rv::TimeSignature signature; double quarterNotesPerBar; };
        constexpr std::array signatures {
            SignatureCase { { 3, 4 }, 3.0 },
            SignatureCase { { 4, 4 }, 4.0 },
            SignatureCase { { 5, 4 }, 5.0 },
            SignatureCase { { 6, 8 }, 3.0 },
            SignatureCase { { 7, 8 }, 3.5 },
            SignatureCase { { 12, 8 }, 6.0 },
        };

        for (const auto& c : signatures)
        {
            expectNear (rv::quarterNotes (rv::Division::oneBar, c.signature), c.quarterNotesPerBar);
            expectNear (rv::quarterNotes (rv::Division::twoBars, c.signature), c.quarterNotesPerBar * 2.0);
            expectNear (rv::quarterNotes (rv::Division::fourBars, c.signature), c.quarterNotesPerBar * 4.0);
            expectNear (rv::quarterNotes (rv::Division::eightBars, c.signature), c.quarterNotesPerBar * 8.0);
        }
    }

    void testSampleRates()
    {
        beginTest ("Sample conversion is rounded and stable at supported rates");

        constexpr std::array sampleRates { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
        for (const auto sampleRate : sampleRates)
        {
            expectEquals (rv::durationSamples (rv::Division::quarter, 120.0, sampleRate, { 4, 4 }),
                          static_cast<int64_t> (std::llround (sampleRate * 0.5)));
            expectEquals (rv::durationSamples (rv::Division::oneBar, 120.0, sampleRate, { 7, 8 }),
                          static_cast<int64_t> (std::llround (sampleRate * 1.75)));
        }
    }

    void testTimingSanitisation()
    {
        beginTest ("Invalid host timing uses deterministic fallbacks");

        const auto missing = rv::sanitiseTiming (NAN, 0, 3, INFINITY);
        expectNear (missing.bpm, 120.0);
        expectEquals (missing.timeSignature.numerator, 4);
        expectEquals (missing.timeSignature.denominator, 4);
        expect (! missing.hasPpqPosition);
        expectNear (missing.ppqPosition, 0.0);

        const auto stoppedClock = rv::sanitiseTiming (0.0, 4, 4, 0.0);
        expectNear (stoppedClock.bpm, 120.0);

        const auto clampedLow = rv::sanitiseTiming (1.0, 7, 8, 12.5);
        expectNear (clampedLow.bpm, rv::minimumBpm);
        expectEquals (clampedLow.timeSignature.numerator, 7);
        expectEquals (clampedLow.timeSignature.denominator, 8);
        expect (clampedLow.hasPpqPosition);
        expectNear (clampedLow.ppqPosition, 12.5);

        const auto clampedHigh = rv::sanitiseTiming (5000.0, 12, 8, -3.25);
        expectNear (clampedHigh.bpm, rv::maximumBpm);
        expect (clampedHigh.hasPpqPosition);
        expectNear (clampedHigh.ppqPosition, -3.25);
    }
};

MusicalTimeTests musicalTimeTests;
}
