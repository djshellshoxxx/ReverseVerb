// ReverseVerb™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "PresetBar.h"

PresetBar::PresetBar (ReverseVerbProcessor& p) : proc (p)
{
    for (auto* b : { &prevButton, &nextButton, &nameButton, &saveButton, &abButton }) addAndMakeVisible (b);
    prevButton.setTooltip ("Previous preset.");
    nextButton.setTooltip ("Next preset.");
    nameButton.setTooltip ("Browse presets, save, delete or open the preset folder. A * means the settings changed since the preset was loaded.");
    saveButton.setTooltip ("Save the current settings as a preset.");
    abButton.setTooltip ("A/B compare: switch between two sets of settings. Your first switch copies the current sound to the other slot.");
    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (+1); };
    nameButton.onClick = [this] { showMenu(); };
    saveButton.onClick = [this] { promptSave(); };
    abButton.onClick   = [this] { proc.getPresets().toggleAB(); abButton.setButtonText (proc.getPresets().isB() ? "B" : "A"); };
    startTimerHz (10);
    timerCallback();
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    prevButton.setBounds (r.removeFromLeft (24));   r.removeFromLeft (3);
    abButton.setBounds (r.removeFromRight (30));    r.removeFromRight (3);
    saveButton.setBounds (r.removeFromRight (50));  r.removeFromRight (3);
    nextButton.setBounds (r.removeFromRight (24));  r.removeFromRight (3);
    nameButton.setBounds (r);
}

void PresetBar::timerCallback()
{
    auto& pm = proc.getPresets();
    const auto name = pm.currentName();
    const juce::String text = name.isEmpty() ? juce::String ("- no preset -") : name + (pm.isDirty() ? " *" : "");
    if (text != shown) { shown = text; nameButton.setButtonText (text + "  v"); }
}

void PresetBar::step (int direction)
{
    auto& pm = proc.getPresets();
    const int n = (int) pm.list().size();
    if (n == 0) return;
    const int cur = pm.currentIndex();
    const int next = cur < 0 ? (direction > 0 ? 0 : n - 1) : (cur + direction + n) % n;
    juce::String err;
    if (! pm.load (next, &err)) showError (err);
}

void PresetBar::showError (const juce::String& message)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("Preset").withMessage (message).withButton ("OK").withAssociatedComponent (this), nullptr);
}

void PresetBar::showMenu()
{
    auto& pm = proc.getPresets();
    const auto& list = pm.list();
    const int cur = pm.currentIndex();

    juce::PopupMenu menu;
    for (bool factory : { true, false })
    {
        juce::PopupMenu group; juce::String lastCat; juce::PopupMenu sub; bool any = false;
        auto flush = [&] { if (lastCat.isNotEmpty() && sub.getNumItems() > 0) group.addSubMenu (lastCat, sub); sub = juce::PopupMenu(); };
        for (int i = 0; i < (int) list.size(); ++i)
        {
            const auto& r = list[(size_t) i];
            if (r.factory != factory) continue;
            any = true;
            if (r.category.isEmpty()) { group.addItem (1000 + i, r.name, true, i == cur); continue; }
            if (r.category != lastCat) { flush(); lastCat = r.category; }
            sub.addItem (1000 + i, r.name, true, i == cur);
        }
        flush();
        if (! any) group.addItem (-1, "(none yet)", false, false);
        menu.addSubMenu (factory ? "Factory" : "User", group);
    }
    menu.addSeparator();
    menu.addItem (1, "Save preset...");
    const bool canDelete = cur >= 0 && ! list[(size_t) cur].factory;
    menu.addItem (2, canDelete ? "Delete \"" + list[(size_t) cur].name + "\"" : juce::String ("Delete preset"), canDelete);
    menu.addItem (3, "Open preset folder");
    menu.addItem (4, "Refresh list");

    juce::Component::SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton), [safe] (int result)
    {
        if (safe == nullptr || result <= 0) return;
        auto& pm2 = safe->proc.getPresets();
        if (result >= 1000)      { juce::String err; if (! pm2.load (result - 1000, &err)) safe->showError (err); }
        else if (result == 1)    safe->promptSave();
        else if (result == 2)    { juce::String err; if (! pm2.remove (pm2.currentIndex(), &err)) safe->showError (err); }
        else if (result == 3)    { auto f = pm2.folderFor ({}); f.createDirectory(); f.revealToUser(); }
        else if (result == 4)    pm2.refresh();
    });
}

void PresetBar::promptSave()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset. Sample files and trim points are not stored in presets.",
                                     juce::MessageBoxIconType::NoIcon, getTopLevelComponent());
    w->addTextEditor ("name", proc.getPresets().currentName(), "Name");
    w->addTextEditor ("cat", {}, "Category (optional)");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r == 1 && safe != nullptr) safe->save (w->getTextEditorContents ("name"), w->getTextEditorContents ("cat"), false);
    }), true);
}

void PresetBar::save (const juce::String& name, const juce::String& category, bool overwrite)
{
    juce::String err;
    if (proc.getPresets().save (name, category, overwrite, &err)) return;
    if (err.contains ("already exists"))
    {
        juce::Component::SafePointer<PresetBar> safe (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("Overwrite preset?").withMessage ("A preset named \"" + name + "\" already exists.")
                                          .withButton ("Overwrite").withButton ("Cancel").withAssociatedComponent (this),
                                      [safe, name, category] (int r) { if (r == 1 && safe != nullptr) safe->save (name, category, true); });
        return;
    }
    showError (err);
}
