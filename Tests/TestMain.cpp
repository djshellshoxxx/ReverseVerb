// ReverseVerb™ test runner. Copyright © 2026 Sheldon Davidson. All rights reserved.
#include <JuceHeader.h>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::printf ("\nReverseVerb tests: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
