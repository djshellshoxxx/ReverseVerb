#include <JuceHeader.h>
#include <iostream>

namespace
{
class ConsoleLogger final : public juce::Logger
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << '\n';
    }
};

class ConsoleTestRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        juce::Logger::writeToLog (message);
    }
};
}

int main()
{
    ConsoleLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    ConsoleTestRunner runner;
    runner.setAssertOnFailure (false);
    // JUCE itself registers a large platform-dependent test suite whenever
    // JUCE_UNIT_TESTS is enabled. Run only this project's tests so a framework
    // assertion cannot turn a plugin regression run into a SIGTRAP.
    runner.runTestsInCategory ("DSP", 0x52565632);

    int failureCount = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (const auto* result = runner.getResult (i))
            failureCount += result->failures;

    juce::Logger::setCurrentLogger (nullptr);
    return failureCount == 0 ? 0 : 1;
}
