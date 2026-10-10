// ReverseVerb™ UI smoke tests (need a display: run under xvfb with RV_UI_TESTS=1).
// Copyright © 2026 Sheldon Davidson. All rights reserved.
#include "TestHelpers.h"

using namespace rvtest;

struct UiTest : public juce::UnitTest
{
    UiTest() : juce::UnitTest ("Editor", "ReverseVerb") {}

    void runTest() override
    {
        if (juce::SystemStats::getEnvironmentVariable ("RV_UI_TESTS", "").isEmpty())
        {
            logMessage ("skipped: set RV_UI_TESTS=1 (needs a display, e.g. xvfb-run)");
            return;
        }

        beginTest ("editor builds, resizes and renders at 75%, 100% and 150%");
        ReverseVerbProcessor p;
        p.prepareToPlay (44100.0, 256);
        p.loadSampleFile (makeBurst ("ui", 44100.0, 0.25, 2));
        p.renderBlocking();

        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        expect (ed != nullptr, "editor created");
        if (ed == nullptr) return;

        const juce::String outDir = juce::SystemStats::getEnvironmentVariable ("RV_SNAPSHOT_DIR", "");
        for (int w : { 795, 1060, 1590 })
        {
            ed->setSize (w, juce::roundToInt ((float) w * 720.0f / 1060.0f));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);      // let timers build the waveform
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            expect (img.isValid() && img.getWidth() == w, "snapshot " + juce::String (w));

            int distinct = 0; juce::uint32 first = img.getPixelAt (5, 5).getARGB();
            for (int y = 0; y < img.getHeight(); y += 13)
                for (int x = 0; x < img.getWidth(); x += 13)
                    if (img.getPixelAt (x, y).getARGB() != first) ++distinct;
            expect (distinct > 200, "not blank at " + juce::String (w));

            if (outDir.isNotEmpty())
            {
                juce::File f (outDir + "/editor_" + juce::String (w) + ".png");
                f.deleteFile();
                if (auto os = f.createOutputStream()) { juce::PNGImageFormat png; png.writeImageToStream (img, *os); }
            }
        }
        expectEquals (p.getUiWidth(), 1590);                                    // size is remembered
    }
};
static UiTest uiTest;
