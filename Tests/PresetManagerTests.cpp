#include <juce_data_structures/juce_data_structures.h>

#include "../Source/PresetManager.h"

namespace
{
class PresetManagerTests final : public juce::UnitTest
{
public:
    PresetManagerTests() : juce::UnitTest ("Preset manager", "DSP") {}

    void runTest() override
    {
        testFactoryPresetsAreWellFormed();
        testFileNameSanitisation();
        testUserPresetRoundTrip();
        testMissingPresetReturnsInvalidTree();
        testDeletingAMissingPresetSucceeds();
    }

private:
    void testFactoryPresetsAreWellFormed()
    {
        beginTest ("Every factory preset has a unique name and at least one parameter");

        const auto& presets = rv::factoryPresets();
        expect (! presets.empty());

        juce::StringArray seenNames;
        for (const auto& preset : presets)
        {
            expect (preset.name.isNotEmpty());
            expect (! seenNames.contains (preset.name), "Duplicate factory preset name: " + preset.name);
            seenNames.add (preset.name);
            expect (! preset.params.empty(), "Preset with no parameters: " + preset.name);

            if (preset.gatePattern.has_value())
                for (const auto step : preset.gatePattern->steps)
                    expect (step >= 0.0f && step <= 1.0f, "Gate step out of range in " + preset.name);
        }
    }

    void testFileNameSanitisation()
    {
        beginTest ("Preset file names are sanitised and never empty");

        expectEquals (rv::sanitisePresetFileName ("My Riser"), juce::String ("My Riser"));
        expect (rv::sanitisePresetFileName ("").isNotEmpty());
        expect (rv::sanitisePresetFileName ("   ").isNotEmpty());
        expect (! rv::sanitisePresetFileName ("a/b\\c:d").containsAnyOf ("/\\:"));
    }

    juce::File makeScratchDirectory() const
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("ReverseVerbPresetTests")
                   .getChildFile (juce::String (juce::Random::getSystemRandom().nextInt64()));
    }

    void testUserPresetRoundTrip()
    {
        beginTest ("Saving, listing, loading, and deleting a user preset round-trips correctly");

        const auto dir = makeScratchDirectory();
        dir.deleteRecursively();

        juce::ValueTree state ("PARAMS");
        state.setProperty ("size", 0.42f, nullptr);
        state.setProperty ("decay", 0.77f, nullptr);

        expect (rv::saveUserPreset (dir, "My Test Preset", state));
        expect (rv::listUserPresetNames (dir).contains ("My Test Preset"));

        const auto loaded = rv::loadUserPreset (dir, "My Test Preset");
        expect (loaded.isValid());
        expectEquals ((float) loaded.getProperty ("size"), 0.42f);
        expectEquals ((float) loaded.getProperty ("decay"), 0.77f);

        expect (rv::deleteUserPreset (dir, "My Test Preset"));
        expect (! rv::listUserPresetNames (dir).contains ("My Test Preset"));

        dir.deleteRecursively();
    }

    void testMissingPresetReturnsInvalidTree()
    {
        beginTest ("Loading a preset that doesn't exist returns an invalid tree");

        const auto dir = makeScratchDirectory();
        dir.deleteRecursively();
        expect (! rv::loadUserPreset (dir, "Nothing Here").isValid());
        dir.deleteRecursively();
    }

    void testDeletingAMissingPresetSucceeds()
    {
        beginTest ("Deleting a preset that doesn't exist is a harmless no-op");

        const auto dir = makeScratchDirectory();
        dir.deleteRecursively();
        expect (rv::deleteUserPreset (dir, "Nothing Here"));
        dir.deleteRecursively();
    }
};

PresetManagerTests presetManagerTests;
}
