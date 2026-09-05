#include "PresetManager.h"
#include "MusicalTime.h"

namespace rv
{
namespace
{
// Repeats a short on/off cycle across all 32 steps so presets stay readable
// instead of listing 32 raw numbers by hand.
GatePattern buildPattern (int activeSteps, std::initializer_list<float> cycle) noexcept
{
    GatePattern pattern;
    pattern.activeSteps = activeSteps;
    if (cycle.size() == 0)
        return pattern;

    size_t index = 0;
    for (auto& step : pattern.steps)
    {
        step = *(cycle.begin() + (index % cycle.size()));
        ++index;
    }
    return pattern;
}

constexpr float division (Division d) noexcept { return (float) static_cast<int> (d); }
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> list;

        list.push_back ({
            "Classic Rise",
            {
                { "direction", 0.0f }, { "sync", 1.0f }, { "syncDivisionV2", division (Division::oneBar) },
                { "align", 1.0f }, { "trimStart", 0.0f }, { "trimEnd", 1.0f },
                { "dry", 1.0f }, { "wet", 0.8f }, { "gap", 0.0f },
                { "size", 0.7f }, { "decay", 0.8f }, { "damp", 0.4f }, { "diff", 0.6f },
                { "er", 0.3f }, { "sep", 0.4f }, { "width", 1.0f },
                { "shape", 0.0f }, { "tone", 20000.0f }, { "basscut", 20.0f },
                { "pitch", 0.0f }, { "pitchRange", 0.0f }, { "pitchTension", 0.0f }, { "transpose", 0.0f }, { "stretch", 1.0f },
                { "panStart", 0.0f }, { "panEnd", 0.0f }, { "panTension", 0.0f },
                { "outputGain", 0.0f },
                { "lfoRate", 2.0f }, { "lfoDepth", 0.0f }, { "lfoShape", 0.0f }, { "lfoTarget", 0.0f },
                { "hitEnabled", 1.0f },
                { "fxEnabled", 0.0f }, { "fxTime", 20.0f }, { "fxFeedback", 0.3f },
                { "fxModRate", 1.5f }, { "fxModDepth", 0.0f }, { "fxMix", 0.0f }, { "fxOrder", 0.0f }, { "fxSync", 0.0f }, { "fxSyncDivision", 7.0f },
                { "volStart", 1.0f }, { "volEnd", 1.0f }, { "volTension", 0.0f },
                { "gateEnabled", 0.0f },
            },
            std::nullopt,
        });

        list.push_back ({
            "Classic Fall",
            {
                { "direction", 1.0f }, { "sync", 1.0f }, { "syncDivisionV2", division (Division::oneBar) },
                { "align", 0.0f }, { "trimStart", 0.0f }, { "trimEnd", 1.0f },
                { "dry", 1.0f }, { "wet", 0.8f }, { "gap", 0.0f },
                { "size", 0.6f }, { "decay", 0.75f }, { "damp", 0.45f }, { "diff", 0.55f },
                { "er", 0.3f }, { "sep", 0.4f }, { "width", 1.0f },
                { "shape", 0.0f }, { "tone", 18000.0f }, { "basscut", 20.0f },
                { "pitch", 0.0f }, { "pitchRange", 0.0f }, { "pitchTension", 0.0f }, { "transpose", 0.0f }, { "stretch", 1.0f },
                { "panStart", 0.0f }, { "panEnd", 0.0f }, { "panTension", 0.0f },
                { "outputGain", 0.0f },
                { "lfoRate", 2.0f }, { "lfoDepth", 0.0f }, { "lfoShape", 0.0f }, { "lfoTarget", 0.0f },
                { "hitEnabled", 1.0f },
                { "fxEnabled", 0.0f }, { "fxTime", 20.0f }, { "fxFeedback", 0.3f },
                { "fxModRate", 1.5f }, { "fxModDepth", 0.0f }, { "fxMix", 0.0f }, { "fxOrder", 0.0f }, { "fxSync", 0.0f }, { "fxSyncDivision", 7.0f },
                { "volStart", 1.0f }, { "volEnd", 1.0f }, { "volTension", 0.0f },
                { "gateEnabled", 0.0f },
            },
            std::nullopt,
        });

        list.push_back ({
            "16-Bar Riser",
            {
                { "direction", 0.0f }, { "sync", 1.0f }, { "syncDivisionV2", division (Division::sixteenBars) },
                { "align", 1.0f }, { "trimStart", 0.0f }, { "trimEnd", 1.0f },
                { "dry", 0.9f }, { "wet", 1.0f }, { "gap", 0.0f },
                { "size", 0.85f }, { "decay", 0.95f }, { "damp", 0.3f }, { "diff", 0.7f },
                { "er", 0.25f }, { "sep", 0.5f }, { "width", 1.0f },
                { "shape", 0.4f }, { "tone", 12000.0f }, { "basscut", 30.0f },
                { "pitch", 0.0f }, { "pitchRange", 0.0f }, { "pitchTension", 0.0f }, { "transpose", 0.0f }, { "stretch", 1.0f },
                { "panStart", 0.0f }, { "panEnd", 0.0f }, { "panTension", 0.0f },
                { "outputGain", 0.0f },
                { "lfoRate", 2.0f }, { "lfoDepth", 0.0f }, { "lfoShape", 0.0f }, { "lfoTarget", 0.0f },
                { "hitEnabled", 1.0f },
                { "fxEnabled", 0.0f }, { "fxTime", 20.0f }, { "fxFeedback", 0.3f },
                { "fxModRate", 1.5f }, { "fxModDepth", 0.0f }, { "fxMix", 0.0f }, { "fxOrder", 0.0f }, { "fxSync", 0.0f }, { "fxSyncDivision", 7.0f },
                { "volStart", 0.15f }, { "volEnd", 1.0f }, { "volTension", -0.3f },
                { "gateEnabled", 0.0f },
            },
            std::nullopt,
        });

        list.push_back ({
            "Gated Riser",
            {
                { "direction", 0.0f }, { "sync", 1.0f }, { "syncDivisionV2", division (Division::fourBars) },
                { "align", 1.0f }, { "trimStart", 0.0f }, { "trimEnd", 1.0f },
                { "dry", 0.9f }, { "wet", 1.0f }, { "gap", 0.0f },
                { "size", 0.75f }, { "decay", 0.85f }, { "damp", 0.4f }, { "diff", 0.6f },
                { "er", 0.3f }, { "sep", 0.4f }, { "width", 1.0f },
                { "shape", 0.2f }, { "tone", 15000.0f }, { "basscut", 25.0f },
                { "pitch", 0.0f }, { "pitchRange", 0.0f }, { "pitchTension", 0.0f }, { "transpose", 0.0f }, { "stretch", 1.0f },
                { "panStart", 0.0f }, { "panEnd", 0.0f }, { "panTension", 0.0f },
                { "outputGain", 0.0f },
                { "lfoRate", 2.0f }, { "lfoDepth", 0.0f }, { "lfoShape", 0.0f }, { "lfoTarget", 0.0f },
                { "hitEnabled", 1.0f },
                { "fxEnabled", 0.0f }, { "fxTime", 20.0f }, { "fxFeedback", 0.3f },
                { "fxModRate", 1.5f }, { "fxModDepth", 0.0f }, { "fxMix", 0.0f }, { "fxOrder", 0.0f }, { "fxSync", 0.0f }, { "fxSyncDivision", 7.0f },
                { "volStart", 1.0f }, { "volEnd", 1.0f }, { "volTension", 0.0f },
                { "gateEnabled", 1.0f }, { "gateSteps", 1.0f }, { "gateRate", 7.0f },
                { "gateDepth", 1.0f }, { "gateSmooth", 4.0f }, { "gateSwing", 0.15f }, { "gatePhase", 0.0f },
                { "gateRetrigger", 0.0f }, { "gateTarget", 2.0f }, { "gateShape", 1.0f },
            },
            buildPattern (32, { 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f }),
        });

        list.push_back ({
            "Glitch Stutter",
            {
                { "direction", 1.0f }, { "sync", 1.0f }, { "syncDivisionV2", division (Division::twoBars) },
                { "align", 0.0f }, { "trimStart", 0.0f }, { "trimEnd", 1.0f },
                { "dry", 0.9f }, { "wet", 0.9f }, { "gap", 0.0f },
                { "size", 0.5f }, { "decay", 0.6f }, { "damp", 0.5f }, { "diff", 0.8f },
                { "er", 0.4f }, { "sep", 0.6f }, { "width", 0.8f },
                { "shape", 0.0f }, { "tone", 9000.0f }, { "basscut", 60.0f },
                { "pitch", 0.0f }, { "pitchRange", 0.0f }, { "pitchTension", 0.0f }, { "transpose", 0.0f }, { "stretch", 1.0f },
                { "panStart", 0.0f }, { "panEnd", 0.0f }, { "panTension", 0.0f },
                { "outputGain", 0.0f },
                { "lfoRate", 2.0f }, { "lfoDepth", 0.0f }, { "lfoShape", 0.0f }, { "lfoTarget", 0.0f },
                { "hitEnabled", 1.0f },
                { "fxEnabled", 0.0f }, { "fxTime", 20.0f }, { "fxFeedback", 0.3f },
                { "fxModRate", 1.5f }, { "fxModDepth", 0.0f }, { "fxMix", 0.0f }, { "fxOrder", 0.0f }, { "fxSync", 0.0f }, { "fxSyncDivision", 7.0f },
                { "volStart", 1.0f }, { "volEnd", 1.0f }, { "volTension", 0.0f },
                { "gateEnabled", 1.0f }, { "gateSteps", 1.0f }, { "gateRate", 6.0f },
                { "gateDepth", 1.0f }, { "gateSmooth", 0.5f }, { "gateSwing", 0.0f }, { "gatePhase", 0.0f },
                { "gateRetrigger", 1.0f }, { "gateTarget", 2.0f }, { "gateShape", 0.0f },
            },
            buildPattern (32, { 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f }),
        });

        list.push_back ({
            "Ambient Wash",
            {
                { "direction", 1.0f }, { "sync", 0.0f }, { "tail", 28.0f },
                { "align", 0.0f }, { "trimStart", 0.0f }, { "trimEnd", 1.0f },
                { "dry", 0.3f }, { "wet", 1.0f }, { "gap", 120.0f },
                { "size", 1.0f }, { "decay", 1.0f }, { "damp", 0.6f }, { "diff", 0.9f },
                { "er", 0.15f }, { "sep", 0.3f }, { "width", 1.0f },
                { "shape", -0.5f }, { "tone", 6000.0f }, { "basscut", 120.0f },
                { "pitch", 0.0f }, { "pitchRange", 0.0f }, { "pitchTension", 0.0f }, { "transpose", 0.0f }, { "stretch", 1.0f },
                { "panStart", 0.0f }, { "panEnd", 0.0f }, { "panTension", 0.0f },
                { "outputGain", 0.0f },
                { "lfoRate", 2.0f }, { "lfoDepth", 0.0f }, { "lfoShape", 0.0f }, { "lfoTarget", 0.0f },
                { "hitEnabled", 1.0f },
                { "fxEnabled", 0.0f }, { "fxTime", 20.0f }, { "fxFeedback", 0.3f },
                { "fxModRate", 1.5f }, { "fxModDepth", 0.0f }, { "fxMix", 0.0f }, { "fxOrder", 0.0f }, { "fxSync", 0.0f }, { "fxSyncDivision", 7.0f },
                { "volStart", 0.0f }, { "volEnd", 1.0f }, { "volTension", 0.4f },
                { "gateEnabled", 0.0f },
            },
            std::nullopt,
        });

        list.push_back ({
            "Pitch Dive Riser",
            {
                { "direction", 0.0f }, { "sync", 1.0f }, { "syncDivisionV2", division (Division::fourBars) },
                { "align", 1.0f }, { "trimStart", 0.0f }, { "trimEnd", 1.0f },
                { "dry", 0.9f }, { "wet", 1.0f }, { "gap", 0.0f },
                { "size", 0.7f }, { "decay", 0.75f }, { "damp", 0.35f }, { "diff", 0.55f },
                { "er", 0.3f }, { "sep", 0.4f }, { "width", 1.0f },
                { "shape", 0.3f }, { "tone", 14000.0f }, { "basscut", 25.0f },
                { "pitch", -1.0f }, { "pitchRange", 2.0f }, { "pitchTension", 0.6f }, { "transpose", 0.0f }, { "stretch", 1.0f },
                { "panStart", 0.0f }, { "panEnd", 0.0f }, { "panTension", 0.0f },
                { "outputGain", 0.0f },
                { "lfoRate", 2.0f }, { "lfoDepth", 0.0f }, { "lfoShape", 0.0f }, { "lfoTarget", 0.0f },
                { "hitEnabled", 1.0f },
                { "fxEnabled", 0.0f }, { "fxTime", 20.0f }, { "fxFeedback", 0.3f },
                { "fxModRate", 1.5f }, { "fxModDepth", 0.0f }, { "fxMix", 0.0f }, { "fxOrder", 0.0f }, { "fxSync", 0.0f }, { "fxSyncDivision", 7.0f },
                { "volStart", 1.0f }, { "volEnd", 1.0f }, { "volTension", 0.0f },
                { "gateEnabled", 0.0f },
            },
            std::nullopt,
        });

        list.push_back ({
            "Snare Riser Builder",
            {
                { "direction", 0.0f }, { "sync", 1.0f }, { "syncDivisionV2", division (Division::eightBars) },
                { "align", 1.0f }, { "trimStart", 0.0f }, { "trimEnd", 1.0f },
                { "dry", 0.85f }, { "wet", 1.0f }, { "gap", 0.0f },
                { "size", 0.8f }, { "decay", 0.9f }, { "damp", 0.35f }, { "diff", 0.65f },
                { "er", 0.3f }, { "sep", 0.4f }, { "width", 1.0f },
                { "shape", 0.15f }, { "tone", 16000.0f }, { "basscut", 25.0f },
                { "pitch", 0.0f }, { "pitchRange", 0.0f }, { "pitchTension", 0.0f }, { "transpose", 0.0f }, { "stretch", 1.0f },
                { "panStart", 0.0f }, { "panEnd", 0.0f }, { "panTension", 0.0f },
                { "outputGain", 0.0f },
                { "lfoRate", 2.0f }, { "lfoDepth", 0.0f }, { "lfoShape", 0.0f }, { "lfoTarget", 0.0f },
                { "hitEnabled", 1.0f },
                { "fxEnabled", 0.0f }, { "fxTime", 20.0f }, { "fxFeedback", 0.3f },
                { "fxModRate", 1.5f }, { "fxModDepth", 0.0f }, { "fxMix", 0.0f }, { "fxOrder", 0.0f }, { "fxSync", 0.0f }, { "fxSyncDivision", 7.0f },
                { "volStart", 0.2f }, { "volEnd", 1.0f }, { "volTension", -0.2f },
                { "gateEnabled", 1.0f }, { "gateSteps", 1.0f }, { "gateRate", 10.0f },
                { "gateDepth", 0.9f }, { "gateSmooth", 8.0f }, { "gateSwing", 0.25f }, { "gatePhase", 0.0f },
                { "gateRetrigger", 0.0f }, { "gateTarget", 2.0f }, { "gateShape", 4.0f },
            },
            buildPattern (32, { 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f }),
        });

        return list;
    }();
    return presets;
}

juce::File getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("SheldonDavidson")
               .getChildFile ("ReverseVerb")
               .getChildFile ("Presets");
}

juce::String sanitisePresetFileName (const juce::String& name) noexcept
{
    auto cleaned = juce::File::createLegalFileName (name.trim());
    return cleaned.isNotEmpty() ? cleaned : juce::String ("Preset");
}

juce::StringArray listUserPresetNames (const juce::File& directory)
{
    juce::StringArray names;
    if (! directory.isDirectory())
        return names;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.xml", juce::File::findFiles))
        names.add (entry.getFile().getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

bool saveUserPreset (const juce::File& directory, const juce::String& name, const juce::ValueTree& state)
{
    if (name.trim().isEmpty() || ! state.isValid())
        return false;
    if (! directory.isDirectory() && ! directory.createDirectory().wasOk())
        return false;

    const auto file = directory.getChildFile (sanitisePresetFileName (name) + ".xml");
    if (auto xml = state.createXml())
        return xml->writeTo (file);
    return false;
}

bool deleteUserPreset (const juce::File& directory, const juce::String& name)
{
    const auto file = directory.getChildFile (sanitisePresetFileName (name) + ".xml");
    return ! file.existsAsFile() || file.deleteFile();
}

juce::ValueTree loadUserPreset (const juce::File& directory, const juce::String& name)
{
    const auto file = directory.getChildFile (sanitisePresetFileName (name) + ".xml");
    if (auto xml = juce::XmlDocument::parse (file))
        return juce::ValueTree::fromXml (*xml);
    return {};
}
}
