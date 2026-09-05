#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "GateEngine.h"

#include <optional>
#include <utility>
#include <vector>

namespace rv
{
// A factory preset is plain code, not a serialized file, so it always exists
// (even on a fresh install) and stays trivial to add to or tweak.
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> params; // parameter id -> raw (denormalised) value
    std::optional<GatePattern> gatePattern; // nullopt leaves the current gate pattern untouched
};

[[nodiscard]] const std::vector<FactoryPreset>& factoryPresets();

// User presets are saved as one XML file per preset under this directory.
[[nodiscard]] juce::File getUserPresetDirectory();

// Strips characters that aren't safe in a filename; collapses to "Preset" if empty.
[[nodiscard]] juce::String sanitisePresetFileName (const juce::String& name) noexcept;

[[nodiscard]] juce::StringArray listUserPresetNames (const juce::File& directory);
bool saveUserPreset (const juce::File& directory, const juce::String& name, const juce::ValueTree& state);
bool deleteUserPreset (const juce::File& directory, const juce::String& name);
[[nodiscard]] juce::ValueTree loadUserPreset (const juce::File& directory, const juce::String& name);
}
