#include <juce_core/juce_core.h>

#include "../Source/GatePatternClipboard.h"

#include <cmath>

namespace
{
class GatePatternClipboardTests final : public juce::UnitTest
{
public:
    GatePatternClipboardTests() : juce::UnitTest ("Gate pattern clipboard", "DSP") {}

    void runTest() override
    {
        beginTest ("Clipboard pattern preserves all steps and metadata");
        rv::GatePattern source;
        source.activeSteps = 32;
        source.seed = 4294967295u;
        for (int step = 0; step < 32; ++step)
            source.steps[(size_t) step] = (float) step / 31.0f;
        const auto decoded = rv::decodeGatePattern (rv::encodeGatePattern (source));
        expect (decoded.has_value());
        if (decoded)
        {
            expectEquals (decoded->activeSteps, 32);
            expectEquals ((int64_t) decoded->seed, (int64_t) 4294967295u);
            for (int step = 0; step < 32; ++step)
                expect (std::abs (decoded->steps[(size_t) step] - source.steps[(size_t) step]) <= 1.0e-5f);
        }

        beginTest ("Clipboard parser rejects truncated, forged, and out-of-range data");
        expect (! rv::decodeGatePattern ("not-a-reverseverb-pattern").has_value());
        expect (! rv::decodeGatePattern ("RVGATE1|24|1|0,1").has_value());
        juce::String invalid = "RVGATE1|16|1|";
        for (int step = 0; step < 32; ++step)
            invalid += (step == 5 ? "1.5" : "0") + juce::String (step == 31 ? "" : ",");
        expect (! rv::decodeGatePattern (invalid).has_value());
    }
};

GatePatternClipboardTests gatePatternClipboardTests;
}
