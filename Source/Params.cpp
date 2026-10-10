// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "Params.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    using A = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&] (const juce::String& id, const juce::String& name, R range, float def, const juce::String& label = {})
    {
        p.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name, range, def, A().withLabel (label)));
    };
    add (IDs::dry,   "Hit",    R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::wet,   "Swell",  R { 0.0f, 1.0f, 0.001f }, 0.8f);
    add (IDs::size,  "Size",   R { 0.0f, 1.0f, 0.001f }, 0.7f);
    add (IDs::decay, "Decay",  R { 0.0f, 1.0f, 0.001f }, 0.8f);
    add (IDs::damp,  "Damp",   R { 0.0f, 1.0f, 0.001f }, 0.4f);
    add (IDs::diff,  "Diffusion", R { 0.0f, 1.0f, 0.001f }, 0.6f);
    add (IDs::er,    "Early Ref", R { 0.0f, 1.0f, 0.001f }, 0.3f);
    add (IDs::sep,   "Separation", R { 0.0f, 1.0f, 0.001f }, 0.4f);
    add (IDs::width, "Width",  R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::gap,   "Delay",  R { 0.0f, 500.0f, 1.0f }, 0.0f, "ms");
    add (IDs::tail,  "Length", R { 0.1f, 8.0f, 0.01f, 0.5f }, 1.2f, "s");
    add (IDs::shape, "Shape",  R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::tone,  "Color",  R { 500.0f, 20000.0f, 1.0f, 0.3f }, 20000.0f, "Hz");
    add (IDs::basscut, "Bass Cut", R { 20.0f, 2000.0f, 1.0f, 0.3f }, 20.0f, "Hz");
    add (IDs::trimStart, "Trim Start", R { 0.0f, 1.0f, 0.0001f }, 0.0f);
    add (IDs::trimEnd,   "Trim End",   R { 0.0f, 1.0f, 0.0001f }, 1.0f);
    add (IDs::pitch,        "Pitch",         R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::pitchTension, "Pitch Tension", R { -1.0f, 1.0f, 0.001f }, 0.0f);
    add (IDs::volStart,     "Vol Start",     R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::volEnd,       "Vol End",       R { 0.0f, 1.0f, 0.001f }, 1.0f);
    add (IDs::volTension,   "Vol Tension",   R { -1.0f, 1.0f, 0.001f }, 0.0f);
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::align, 1 }, "Hit on note (PDC)", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { IDs::sync, 1 }, "Sync to BPM", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::syncLen, 1 }, "Sync Length",
                     juce::StringArray { "1 beat", "2 beats", "4 beats", "8 beats", "1 bar", "2 bars", "4 bars" }, 2));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { IDs::pitchRange, 1 }, "Pitch Range",
                     juce::StringArray { "1 oct", "2 oct", "4 oct" }, 0));
    return { p.begin(), p.end() };
}
