// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "PresetManager.h"
#include "BinaryData.h"

namespace
{
    constexpr float kTolerance = 1.0e-4f;       // fraction of the parameter's range

    bool isReservedWindowsName (const juce::String& s)
    {
        static const char* reserved[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                           "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
        for (auto* r : reserved) if (s.equalsIgnoreCase (r)) return true;
        return false;
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& a, const juce::File& folder) : apvts (a), userFolder (folder)
{
    refresh();
}

juce::File PresetManager::defaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("CircuitDriftLabs").getChildFile ("ReverseVerb").getChildFile ("Presets");
}

bool PresetManager::sanitizeName (const juce::String& in, juce::String& out, juce::String& error)
{
    juce::String s;
    for (auto c : in.trim())
        if (c >= 32 && juce::String ("\\/:*?\"<>|").indexOfChar (c) < 0) s += c;
    s = s.trim();
    while (s.endsWithChar ('.') || s.endsWithChar (' ')) s = s.dropLastCharacters (1);
    if (s.isEmpty())        { error = "Please enter a name."; return false; }
    if (s.length() > 64)    { error = "Name is too long (maximum 64 characters)."; return false; }
    if (isReservedWindowsName (s) || s == "." || s == "..") { error = "That name is reserved by the operating system."; return false; }
    out = s;
    return true;
}

bool PresetManager::isExcluded (const juce::String& id, bool includeTrim)
{
    return ! includeTrim && (id == "trimStart" || id == "trimEnd");
}

juce::File PresetManager::folderFor (const juce::String& category) const
{
    juce::String cat, err;
    if (category.isNotEmpty() && sanitizeName (category, cat, err)) return userFolder.getChildFile (cat);
    return userFolder;
}

std::unique_ptr<juce::XmlElement> PresetManager::readFactory (int index) const
{
    int size = 0;
    if (index < 0 || index >= BinaryData::namedResourceListSize) return nullptr;
    auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[index], size);
    if (data == nullptr) return nullptr;
    return juce::XmlDocument::parse (juce::String::fromUTF8 (data, size));
}

void PresetManager::refresh()
{
    presets.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (auto xml = readFactory (i))
            if (xml->hasTagName ("ReverseVerbPreset"))
            {
                PresetRef r; r.factory = true; r.factoryIndex = i;
                r.name = xml->getStringAttribute ("name"); r.category = xml->getStringAttribute ("category");
                if (r.name.isNotEmpty()) presets.push_back (r);
            }
    std::stable_sort (presets.begin(), presets.end(), [] (const PresetRef& a, const PresetRef& b)
                      { return a.category == b.category ? a.name.compareNatural (b.name) < 0 : a.category.compareNatural (b.category) < 0; });

    std::vector<PresetRef> user;
    if (userFolder.isDirectory())
        for (auto& f : userFolder.findChildFiles (juce::File::findFiles, true, "*.rvpreset"))
        {
            PresetRef r; r.file = f; r.name = f.getFileNameWithoutExtension();
            if (f.getParentDirectory() != userFolder) r.category = f.getParentDirectory().getFileName();
            user.push_back (r);
        }
    std::stable_sort (user.begin(), user.end(), [] (const PresetRef& a, const PresetRef& b)
                      { return a.category == b.category ? a.name.compareNatural (b.name) < 0 : a.category.compareNatural (b.category) < 0; });
    presets.insert (presets.end(), user.begin(), user.end());
}

int PresetManager::indexOf (const juce::String& name, bool factory) const
{
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].factory == factory && presets[(size_t) i].name == name) return i;
    return -1;
}

PresetManager::Values PresetManager::captureCurrent() const
{
    Values v;
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isExcluded (rp->paramID, includeTrim))
                v[rp->paramID] = rp->convertFrom0to1 (rp->getValue());
    return v;
}

std::unique_ptr<juce::XmlElement> PresetManager::toXml (const juce::String& name, const juce::String& category) const
{
    auto xml = std::make_unique<juce::XmlElement> ("ReverseVerbPreset");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("name", name);
    xml->setAttribute ("category", category);
    for (auto& kv : captureCurrent())
    {
        auto* e = xml->createNewChildElement ("P");
        e->setAttribute ("id", kv.first);
        e->setAttribute ("v", (double) kv.second);
    }
    return xml;
}

bool PresetManager::parse (const juce::XmlElement& xml, Values& out, juce::String& name, juce::String& category, juce::String& error) const
{
    if (! xml.hasTagName ("ReverseVerbPreset")) { error = "Not a ReverseVerb preset."; return false; }
    if (xml.getIntAttribute ("version", 1) > 1)  { error = "This preset was made by a newer version of ReverseVerb."; return false; }
    name = xml.getStringAttribute ("name"); category = xml.getStringAttribute ("category");
    for (auto* e : xml.getChildWithTagNameIterator ("P"))
    {
        const auto id = e->getStringAttribute ("id");
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
        if (rp == nullptr || isExcluded (id, includeTrim)) continue;                    // unknown / excluded ids are ignored
        const double v = e->getDoubleAttribute ("v", (double) rp->convertFrom0to1 (rp->getDefaultValue()));
        if (! std::isfinite (v)) continue;
        const auto& range = rp->getNormalisableRange();
        out[id] = (float) juce::jlimit ((double) range.start, (double) range.end, v);   // clamp into range
    }
    return true;
}

bool PresetManager::applyValues (const Values& values)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (isExcluded (rp->paramID, includeTrim)) continue;
            auto it = values.find (rp->paramID);
            const float norm = it != values.end() ? rp->convertTo0to1 (it->second) : rp->getDefaultValue();   // missing -> default
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
            rp->endChangeGesture();
        }
    return true;
}

bool PresetManager::load (int index, juce::String* error)
{
    auto fail = [&] (const juce::String& m) { if (error != nullptr) *error = m; return false; };
    if (index < 0 || index >= (int) presets.size()) return fail ("Preset not found.");
    const auto& ref = presets[(size_t) index];

    std::unique_ptr<juce::XmlElement> xml = ref.factory ? readFactory (ref.factoryIndex) : juce::XmlDocument::parse (ref.file);
    if (xml == nullptr) return fail ("The preset file could not be read.");

    Values values; juce::String name, cat, err;
    if (! parse (*xml, values, name, cat, err)) return fail (err);
    applyValues (values);

    const juce::ScopedLock sl (nameLock);
    loadedName = ref.name; loadedFactory = ref.factory; loadedValues = captureCurrent(); haveLoaded = true;
    return true;
}

bool PresetManager::save (const juce::String& rawName, const juce::String& rawCategory, bool overwrite, juce::String* error)
{
    auto fail = [&] (const juce::String& m) { if (error != nullptr) *error = m; return false; };
    juce::String name, cat, err;
    if (! sanitizeName (rawName, name, err)) return fail (err);
    if (rawCategory.trim().isNotEmpty() && ! sanitizeName (rawCategory, cat, err)) return fail ("Category: " + err);

    auto dir = folderFor (cat);
    if (! dir.createDirectory().wasOk()) return fail ("Could not create the preset folder.");
    auto file = dir.getChildFile (name + ".rvpreset");
    if (file.existsAsFile() && ! overwrite) return fail ("A preset with that name already exists.");

    auto xml = toXml (name, cat);
    juce::TemporaryFile tmp (file);                              // write atomically: never leave a half-written preset
    if (! xml->writeTo (tmp.getFile()) || ! tmp.overwriteTargetFileWithTemporary()) return fail ("Could not write the preset file.");

    refresh();
    {
        const juce::ScopedLock sl (nameLock);
        loadedName = name; loadedFactory = false; loadedValues = captureCurrent(); haveLoaded = true;
    }
    return true;
}

bool PresetManager::remove (int index, juce::String* error)
{
    if (index < 0 || index >= (int) presets.size() || presets[(size_t) index].factory)
    {
        if (error != nullptr) *error = "Factory presets cannot be deleted.";
        return false;
    }
    const auto file = presets[(size_t) index].file;
    if (! file.deleteFile()) { if (error != nullptr) *error = "Could not delete the preset file."; return false; }
    if (file.getParentDirectory() != userFolder && file.getParentDirectory().getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
        file.getParentDirectory().deleteFile();                  // tidy empty category folders
    refresh();
    return true;
}

juce::String PresetManager::currentName() const
{
    const juce::ScopedLock sl (nameLock);
    return haveLoaded ? loadedName : juce::String();
}

int PresetManager::currentIndex() const
{
    const juce::ScopedLock sl (nameLock);
    return haveLoaded ? indexOf (loadedName, loadedFactory) : -1;
}

void PresetManager::restoreName (const juce::String& name)
{
    // Called after a project load: find the preset by name so the dirty marker can compare against it.
    for (bool factory : { false, true })
    {
        const int i = indexOf (name, factory);
        if (i < 0) continue;
        const auto& ref = presets[(size_t) i];
        std::unique_ptr<juce::XmlElement> xml = ref.factory ? readFactory (ref.factoryIndex) : juce::XmlDocument::parse (ref.file);
        Values values; juce::String n, c, e;
        if (xml != nullptr && parse (*xml, values, n, c, e))
        {
            const juce::ScopedLock sl (nameLock);
            loadedName = name; loadedFactory = factory; haveLoaded = true;
            loadedValues = captureCurrent();
            for (auto& kv : values) loadedValues[kv.first] = kv.second;
            return;
        }
    }
    const juce::ScopedLock sl (nameLock);
    haveLoaded = false; loadedName = {};
}

bool PresetManager::isDirty() const
{
    const juce::ScopedLock sl (nameLock);
    if (! haveLoaded) return false;
    for (auto& kv : captureCurrent())
    {
        auto it = loadedValues.find (kv.first);
        if (it == loadedValues.end()) continue;
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (kv.first));
        const float span = rp != nullptr ? rp->getNormalisableRange().end - rp->getNormalisableRange().start : 1.0f;
        if (std::abs (kv.second - it->second) > kTolerance * juce::jmax (span, 1.0e-6f)) return true;
    }
    return false;
}

void PresetManager::toggleAB()
{
    slots[(size_t) abActive] = captureCurrent(); slotUsed[(size_t) abActive] = true;
    abActive = 1 - abActive;
    if (slotUsed[(size_t) abActive]) applyValues (slots[(size_t) abActive]);
    else { slots[(size_t) abActive] = captureCurrent(); slotUsed[(size_t) abActive] = true; }     // first use: B starts as a copy of A
}
