#include "SampleLoadService.h"

namespace rv
{
SampleLoadResult loadSampleSnapshot (const juce::File& file, juce::AudioFormatManager& formatManager)
{
    SampleLoadResult result;
    result.file = file;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
    {
        result.error = "Unable to read saved sample: " + file.getFullPathName();
        return result;
    }

    const auto maximumSamples = (juce::int64) std::llround (reader->sampleRate * 10.0);
    const auto length = (int) juce::jmin (reader->lengthInSamples, maximumSamples);
    result.audio.setSize ((int) reader->numChannels, length);
    if (! reader->read (&result.audio, 0, length, 0, true, true))
    {
        result.audio.setSize (0, 0);
        result.error = "Unable to decode saved sample: " + file.getFullPathName();
        return result;
    }

    result.sampleRate = reader->sampleRate;
    result.folderFiles = file.getParentDirectory().findChildFiles (
        juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    result.folderFiles.sort();
    result.currentIndex = result.folderFiles.indexOf (file);
    return result;
}
}
