#include <JuceHeader.h>
#include "../Source/SampleLoadService.h"

namespace
{
class SampleLoadServiceTests final : public juce::UnitTest
{
public:
    SampleLoadServiceTests() : UnitTest ("Sample load service") {}

    void runTest() override
    {
        beginTest ("A WAV is decoded with its sorted sibling list");
        juce::TemporaryFile first (juce::File::createTempFile ("rv-b.wav"));
        juce::TemporaryFile second (first.getFile().getSiblingFile ("rv-a.wav"));
        writeWav (first.getFile(), 48000.0, 128);
        writeWav (second.getFile(), 48000.0, 64);

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const auto result = rv::loadSampleSnapshot (first.getFile(), formats);
        expect (result.succeeded());
        expectEquals (result.audio.getNumSamples(), 128);
        expectWithinAbsoluteError (result.sampleRate, 48000.0, 0.01);
        expect (result.currentIndex >= 0);

        beginTest ("A missing file produces a non-blocking error result");
        const auto missing = rv::loadSampleSnapshot (
            first.getFile().getSiblingFile ("rv-missing.wav"), formats);
        expect (! missing.succeeded());
        expect (missing.error.isNotEmpty());
    }

private:
    static void writeWav (const juce::File& file, double sampleRate, int samples)
    {
        file.deleteFile();
        auto stream = file.createOutputStream();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.release(), sampleRate, 1, 16, {}, 0));
        jassert (writer != nullptr);
        juce::AudioBuffer<float> audio (1, samples);
        audio.clear();
        audio.setSample (0, 0, 0.5f);
        writer->writeFromAudioSampleBuffer (audio, 0, samples);
    }
};

SampleLoadServiceTests sampleLoadServiceTests;
}
