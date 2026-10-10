// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#pragma once
#include <JuceHeader.h>

struct PresetRef
{
    juce::String name, category;
    bool factory = false;
    juce::File file;                    // user presets
    int factoryIndex = -1;              // factory presets (index into BinaryData)
};

// Saves / loads / browses presets. Message-thread only (except currentName(), which is locked).
// File format (*.rvpreset, UTF-8 XML) stores REAL parameter values, so it survives range changes:
//   <ReverseVerbPreset version="1" name="..." category="..."><P id="size" v="0.7"/>...</ReverseVerbPreset>
// Presets never contain the sample path. Trim points are excluded unless includeTrim is set.
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState&, const juce::File& userFolder);

    static juce::File defaultUserFolder();
    static bool sanitizeName (const juce::String& in, juce::String& out, juce::String& error);   // 1-64 chars, filesystem-safe

    void refresh();                                         // rescan factory + user presets
    const std::vector<PresetRef>& list() const { return presets; }
    int indexOf (const juce::String& name, bool factory) const;

    bool load (int index, juce::String* error = nullptr);
    bool save (const juce::String& name, const juce::String& category, bool overwrite, juce::String* error = nullptr);
    bool remove (int index, juce::String* error = nullptr);
    juce::File folderFor (const juce::String& category) const;

    juce::String currentName() const;                       // thread-safe
    int currentIndex() const;                               // index into list(), or -1
    void restoreName (const juce::String& name);            // after project load: re-attach snapshot for dirty detection
    bool isDirty() const;
    bool includeTrim = false;

    // A/B compare (in-memory)
    void toggleAB();
    bool isB() const { return abActive == 1; }

    // XML helpers (static so they can be tested without a processor)
    static bool isExcluded (const juce::String& id, bool includeTrim);

private:
    using Values = std::map<juce::String, float>;
    Values captureCurrent() const;
    bool applyValues (const Values&);
    std::unique_ptr<juce::XmlElement> toXml (const juce::String& name, const juce::String& category) const;
    bool parse (const juce::XmlElement&, Values& out, juce::String& name, juce::String& category, juce::String& error) const;
    std::unique_ptr<juce::XmlElement> readFactory (int index) const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userFolder;
    std::vector<PresetRef> presets;

    mutable juce::CriticalSection nameLock;
    juce::String loadedName;
    Values loadedValues;
    bool haveLoaded = false, loadedFactory = false;

    std::array<Values, 2> slots;
    std::array<bool, 2> slotUsed { false, false };
    int abActive = 0;
};
