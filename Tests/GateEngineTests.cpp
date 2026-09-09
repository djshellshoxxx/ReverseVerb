#include <juce_core/juce_core.h>

#include "../Source/GateEngine.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <new>

namespace
{
std::atomic<bool> trackAllocations { false };
std::atomic<int> trackedAllocations { 0 };
}

void* operator new (std::size_t size)
{
    if (trackAllocations.load (std::memory_order_relaxed))
        trackedAllocations.fetch_add (1, std::memory_order_relaxed);
    if (auto* memory = std::malloc (size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete (void* memory) noexcept { std::free (memory); }
void operator delete[] (void* memory) noexcept { std::free (memory); }
void operator delete (void* memory, std::size_t) noexcept { std::free (memory); }
void operator delete[] (void* memory, std::size_t) noexcept { std::free (memory); }

namespace
{
class AllocationScope
{
public:
    AllocationScope()
    {
        trackedAllocations = 0;
        trackAllocations = true;
    }

    ~AllocationScope() { trackAllocations = false; }
    [[nodiscard]] int count() const noexcept { return trackedAllocations.load(); }
};

class GateEngineTests final : public juce::UnitTest
{
public:
    GateEngineTests() : juce::UnitTest ("Tempo-synced gate", "DSP") {}

    void runTest() override
    {
        testStepBoundaries();
        testShapes();
        testDepth();
        testSwingAndPhase();
        testRetriggerModes();
        testSmoothingAndRealtimeSafety();
        testPatternValidation();
    }

private:
    static rv::HostTiming timing()
    {
        rv::HostTiming result;
        result.bpm = 60.0;
        result.sampleRate = 1000.0;
        result.timeSignature = { 4, 4 };
        return result;
    }

    void expectNear (float actual, float expected, float tolerance = 1.0e-5f)
    {
        expect (std::abs (actual - expected) <= tolerance,
                "Expected " + juce::String (expected, 6)
                    + ", got " + juce::String (actual, 6));
    }

    void testStepBoundaries()
    {
        beginTest ("Step lookup changes exactly at musical boundaries and wraps");
        rv::GatePattern pattern;
        rv::GateSettings settings;
        settings.rate = rv::Division::quarter;
        settings.smoothingMilliseconds = 0.0f;
        settings.shape = rv::GateShape::square;
        rv::GateEngine engine;

        expectNear (engine.gainAt (pattern, settings, timing(), 0, 0), 1.0f);
        expectNear (engine.gainAt (pattern, settings, timing(), 999, 0), 1.0f);
        expectNear (engine.gainAt (pattern, settings, timing(), 1000, 0), 0.0f);
        expectNear (engine.gainAt (pattern, settings, timing(), 16000, 0), 1.0f);
    }

    void testShapes()
    {
        beginTest ("Every gate shape remains finite and inside unity range");
        rv::GatePattern pattern;
        pattern.steps.fill (1.0f);
        rv::GateSettings settings;
        settings.rate = rv::Division::quarter;
        rv::GateEngine engine;

        constexpr std::array shapes { rv::GateShape::square, rv::GateShape::smoothSquare,
                                      rv::GateShape::rampUp, rv::GateShape::rampDown,
                                      rv::GateShape::triangle, rv::GateShape::sine,
                                      rv::GateShape::curved };
        for (const auto shape : shapes)
        {
            settings.shape = shape;
            for (int sample = 0; sample <= 999; sample += 37)
            {
                const auto gain = engine.gainAt (pattern, settings, timing(), sample, 0);
                expect (std::isfinite (gain) && gain >= 0.0f && gain <= 1.0f);
            }
        }

        settings.shape = rv::GateShape::rampUp;
        expectNear (engine.gainAt (pattern, settings, timing(), 250, 0), 0.25f);
        settings.shape = rv::GateShape::rampDown;
        expectNear (engine.gainAt (pattern, settings, timing(), 250, 0), 0.75f);
        settings.shape = rv::GateShape::triangle;
        expectNear (engine.gainAt (pattern, settings, timing(), 500, 0), 1.0f);
    }

    void testDepth()
    {
        beginTest ("Depth crossfades between ungated and pattern gain");
        rv::GatePattern pattern;
        pattern.steps.fill (0.0f);
        rv::GateSettings settings;
        settings.rate = rv::Division::quarter;
        settings.depth = 0.25f;
        rv::GateEngine engine;
        expectNear (engine.gainAt (pattern, settings, timing(), 0, 0), 0.75f);
        settings.depth = 0.0f;
        expectNear (engine.gainAt (pattern, settings, timing(), 0, 0), 1.0f);
    }

    void testSwingAndPhase()
    {
        beginTest ("Swing lengthens the first step and shortens the second");
        rv::GatePattern pattern;
        rv::GateSettings settings;
        settings.rate = rv::Division::quarter;
        settings.swing = 0.5f;
        rv::GateEngine engine;
        expectNear (engine.gainAt (pattern, settings, timing(), 1499, 0), 1.0f);
        expectNear (engine.gainAt (pattern, settings, timing(), 1500, 0), 0.0f);
        expectNear (engine.gainAt (pattern, settings, timing(), 1999, 0), 0.0f);
        expectNear (engine.gainAt (pattern, settings, timing(), 2000, 0), 1.0f);

        beginTest ("Phase offsets the pattern by a fraction of its cycle");
        settings.swing = 0.0f;
        settings.phase = 1.0f / 16.0f;
        expectNear (engine.gainAt (pattern, settings, timing(), 0, 0), 0.0f);
    }

    void testRetriggerModes()
    {
        beginTest ("Host retrigger follows PPQ while Note retrigger follows voice age");
        rv::GatePattern pattern;
        rv::GateSettings settings;
        settings.rate = rv::Division::quarter;
        auto hostTiming = timing();
        hostTiming.hasPpqPosition = true;
        hostTiming.ppqPosition = 1.0;
        rv::GateEngine engine;

        settings.retrigger = rv::GateRetrigger::note;
        expectNear (engine.gainAt (pattern, settings, hostTiming, 0, 0), 1.0f);
        settings.retrigger = rv::GateRetrigger::host;
        expectNear (engine.gainAt (pattern, settings, hostTiming, 0, 0), 0.0f);
        expectNear (engine.gainAt (pattern, settings, hostTiming, 0, 1000), 1.0f);

        beginTest ("Missing host PPQ falls back to deterministic Note retrigger");
        hostTiming.hasPpqPosition = false;
        expectNear (engine.gainAt (pattern, settings, hostTiming, 0, 0), 1.0f);
        expectNear (engine.gainAt (pattern, settings, hostTiming, 1000, 0), 0.0f);
    }

    void testSmoothingAndRealtimeSafety()
    {
        beginTest ("Smoothing approaches a step without overshoot or allocation");
        rv::GatePattern pattern;
        pattern.steps.fill (0.0f);
        rv::GateSettings settings;
        settings.rate = rv::Division::quarter;
        settings.smoothingMilliseconds = 10.0f;
        rv::GateEngine engine;
        engine.reset (1.0f);
        std::array<float, 64> output {};

        int allocations = -1;
        {
            AllocationScope scope;
            engine.processGains (output.data(), (int) output.size(), pattern, settings, timing(), 0, 0);
            allocations = scope.count();
        }
        expectEquals (allocations, 0);
        expect (output.front() < 1.0f && output.front() > 0.0f);
        expect (output.back() < output.front() && output.back() >= 0.0f);
        for (const auto gain : output)
            expect (std::isfinite (gain) && gain >= 0.0f && gain <= 1.0f);
    }

    void testPatternValidation()
    {
        beginTest ("Invalid patterns are clamped to a supported immutable snapshot");
        rv::GatePattern pattern;
        pattern.activeSteps = 21;
        pattern.steps[0] = -2.0f;
        pattern.steps[1] = 4.0f;
        pattern.steps[2] = std::numeric_limits<float>::quiet_NaN();
        const auto clean = rv::sanitiseGatePattern (pattern);
        expectEquals (clean.activeSteps, 32);
        expectNear (clean.steps[0], 0.0f);
        expectNear (clean.steps[1], 1.0f);
        expectNear (clean.steps[2], 0.0f);
        pattern.activeSteps = -1;
        expectEquals (rv::sanitiseGatePattern (pattern).activeSteps, 16);
    }
};

GateEngineTests gateEngineTests;
}
