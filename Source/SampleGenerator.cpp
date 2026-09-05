#include "SampleGenerator.h"

#include <array>
#include <cmath>

namespace rv
{
namespace
{
// One-pole lowpass, also used as a highpass via (input - lowpass(input)).
struct OnePole
{
    float a = 0.0f, z = 0.0f;

    void setCutoff (float hz, float sampleRate) noexcept
    {
        const auto clamped = juce::jlimit (10.0f, sampleRate * 0.49f, hz);
        a = std::exp (-juce::MathConstants<float>::twoPi * clamped / sampleRate);
    }

    float lowpass (float in) noexcept
    {
        z = in * (1.0f - a) + z * a;
        return z;
    }

    float highpass (float in) noexcept { return in - lowpass (in); }
};

float noiseSample (juce::Random& rng) noexcept { return rng.nextFloat() * 2.0f - 1.0f; }

// Fast-attack, exponential-ish decay envelope. `curve` steepens the falloff.
float envelope (float t, float attack, float decay, float curve) noexcept
{
    if (t < 0.0f) return 0.0f;
    if (t < attack) return attack > 0.0f ? t / attack : 1.0f;
    const auto d = t - attack;
    return std::exp (-d / juce::jmax (0.0005f, decay) * curve);
}

void normalise (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto n = buffer.getNumSamples();
    if (n <= 0) return;
    const auto mag = buffer.getMagnitude (0, n);
    if (mag > 0.0f) buffer.applyGain (0.9f / mag);
}

juce::AudioBuffer<float> renderSnare (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.24f + rng.nextFloat() * 0.10f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    OnePole noiseTone;  noiseTone.setCutoff (2400.0f + rng.nextFloat() * 900.0f, sr);
    OnePole noiseFloor; noiseFloor.setCutoff (700.0f + rng.nextFloat() * 250.0f, sr);
    const float bodyFreq1 = 175.0f + rng.nextFloat() * 45.0f;
    const float bodyFreq2 = bodyFreq1 * (1.55f + rng.nextFloat() * 0.2f);
    float phase1 = 0.0f, phase2 = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float noiseEnv = envelope (t, 0.0004f, 0.14f + rng.nextFloat() * 0.03f, 4.5f);
        const float bodyEnv  = envelope (t, 0.0002f, 0.045f, 7.0f);

        float noise = noiseSample (rng);
        noise = noise - noiseFloor.lowpass (noise);   // crude highpass, cuts rumble
        noise = noiseTone.lowpass (noise * 1.5f);

        const float body = std::sin (phase1) * 0.6f + std::sin (phase2) * 0.4f;
        phase1 += juce::MathConstants<float>::twoPi * bodyFreq1 / sr;
        phase2 += juce::MathConstants<float>::twoPi * bodyFreq2 / sr;

        const float sample = noise * noiseEnv * 0.8f + body * bodyEnv * 0.5f;
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderHat (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.07f + rng.nextFloat() * 0.06f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    // Classic analog hi-hat trick: several inharmonic square oscillators, highpassed.
    static constexpr std::array<float, 6> ratios { 1.0f, 1.342f, 1.478f, 1.79f, 2.11f, 2.47f };
    const float fundamental = 280.0f + rng.nextFloat() * 70.0f;
    std::array<float, 6> phases {};
    OnePole hp; hp.setCutoff (6500.0f + rng.nextFloat() * 2000.0f, sr);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float env = envelope (t, 0.0002f, 0.04f + rng.nextFloat() * 0.02f, 6.0f);

        float mix = 0.0f;
        for (size_t k = 0; k < ratios.size(); ++k)
        {
            const auto cyclePos = std::fmod (phases[k] / juce::MathConstants<float>::twoPi, 1.0f);
            mix += cyclePos < 0.5f ? 1.0f : -1.0f;
            phases[k] += juce::MathConstants<float>::twoPi * fundamental * ratios[k] / sr;
        }
        mix /= (float) ratios.size();

        const float filtered = hp.highpass (mix);
        const float sample = filtered * env * 0.9f;
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderClap (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.22f + rng.nextFloat() * 0.10f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    OnePole tone;  tone.setCutoff (2600.0f + rng.nextFloat() * 500.0f, sr);
    OnePole floor_; floor_.setCutoff (650.0f + rng.nextFloat() * 200.0f, sr);

    constexpr int burstCount = 4;
    const float burstSpacingSamples = (0.008f + rng.nextFloat() * 0.005f) * sr;

    for (int i = 0; i < n; ++i)
    {
        float env = 0.0f;
        for (int b = 0; b < burstCount; ++b)
        {
            const float onset = (float) b * burstSpacingSamples;
            const float tSec = ((float) i - onset) / sr;
            const bool finalBurst = b == burstCount - 1;
            const float burstEnv = envelope (tSec, 0.0003f,
                                             finalBurst ? 0.14f : 0.018f,
                                             finalBurst ? 4.0f : 9.0f);
            env = juce::jmax (env, burstEnv);
        }

        float noise = noiseSample (rng);
        noise = tone.lowpass (noise);
        noise = noise - floor_.lowpass (noise);

        const float sample = noise * env * 0.85f;
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderBassDrum (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.35f + rng.nextFloat() * 0.15f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    const float startFreq = 130.0f + rng.nextFloat() * 40.0f;
    const float endFreq = 42.0f + rng.nextFloat() * 10.0f;
    const float pitchDecay = 0.045f + rng.nextFloat() * 0.015f;
    float phase = 0.0f;
    OnePole clickTone; clickTone.setCutoff (2200.0f, sr);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float bodyEnv = envelope (t, 0.0005f, 0.32f, 3.0f);
        const float clickEnv = envelope (t, 0.0002f, 0.006f, 8.0f);
        const float freq = endFreq + (startFreq - endFreq) * std::exp (-t / pitchDecay);
        phase += juce::MathConstants<float>::twoPi * freq / sr;

        const float click = clickTone.lowpass (noiseSample (rng)) * clickEnv * 0.5f;
        const float sample = std::sin (phase) * bodyEnv * 0.95f + click;
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderHorn (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.4f + rng.nextFloat() * 0.15f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    const float fundamental = 155.0f + rng.nextFloat() * 35.0f;
    static constexpr std::array<float, 5> harmonicRatio { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    static constexpr std::array<float, 5> harmonicLevel { 1.0f, 0.55f, 0.7f, 0.3f, 0.35f };
    std::array<float, 5> phases {};
    OnePole tone; tone.setCutoff (3200.0f, sr);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float env = envelope (t, 0.015f, 0.28f, 2.2f);
        float mix = 0.0f;
        for (size_t k = 0; k < harmonicRatio.size(); ++k)
        {
            mix += std::sin (phases[k]) * harmonicLevel[k];
            phases[k] += juce::MathConstants<float>::twoPi * fundamental * harmonicRatio[k] / sr;
        }
        const float sample = tone.lowpass (mix) * env * 0.35f;
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderString (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.5f + rng.nextFloat() * 0.2f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    const float fundamental = 200.0f + rng.nextFloat() * 80.0f;
    static constexpr std::array<float, 6> harmonicRatio { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    std::array<float, 6> phases {};

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        float mix = 0.0f;
        for (size_t k = 0; k < harmonicRatio.size(); ++k)
        {
            // Higher harmonics decay faster - the classic plucked/bowed-string cue.
            const float decay = 0.35f / (float) (k + 1);
            const float level = envelope (t, 0.003f, decay, 1.6f) / (float) (k + 1);
            mix += std::sin (phases[k]) * level;
            phases[k] += juce::MathConstants<float>::twoPi * fundamental * harmonicRatio[k] / sr;
        }
        buffer.setSample (0, i, mix);
        buffer.setSample (1, i, mix);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderPluck (double sampleRate, juce::uint32 seed)
{
    // Karplus-Strong: a short noise burst circulates through a delay line sized
    // to the target pitch, low-passed each pass so it decays into a plucked tone.
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float freq = 220.0f + rng.nextFloat() * 220.0f;
    const float durationSec = 0.6f + rng.nextFloat() * 0.2f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    const int delayLen = juce::jmax (2, (int) std::lround (sr / freq));
    std::vector<float> line ((size_t) delayLen);
    for (auto& s : line) s = noiseSample (rng);

    juce::AudioBuffer<float> buffer (2, n);
    int idx = 0;
    float prev = 0.0f;
    const float damping = 0.996f - rng.nextFloat() * 0.003f;
    for (int i = 0; i < n; ++i)
    {
        const float current = line[(size_t) idx];
        const float next = (current + prev) * 0.5f * damping;
        line[(size_t) idx] = next;
        prev = next;
        idx = (idx + 1) % delayLen;
        buffer.setSample (0, i, current);
        buffer.setSample (1, i, current);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderRimshot (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.09f + rng.nextFloat() * 0.04f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    const float tockFreq = 900.0f + rng.nextFloat() * 300.0f;
    float phase = 0.0f;
    OnePole tone; tone.setCutoff (3500.0f + rng.nextFloat() * 800.0f, sr);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float clickEnv = envelope (t, 0.0001f, 0.006f, 10.0f);
        const float tockEnv = envelope (t, 0.0001f, 0.03f, 6.0f);
        phase += juce::MathConstants<float>::twoPi * tockFreq / sr;

        const float click = tone.lowpass (noiseSample (rng)) * clickEnv * 0.9f;
        const float tock = std::sin (phase) * tockEnv * 0.6f;
        const float sample = click + tock;
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderTriangle (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.9f + rng.nextFloat() * 0.3f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    // The orchestral triangle: a bright fundamental plus a few inharmonic
    // partials (non-integer ratios), ringing out slowly.
    const float fundamental = 2600.0f + rng.nextFloat() * 600.0f;
    static constexpr std::array<float, 4> ratio { 1.0f, 2.76f, 4.18f, 5.63f };
    static constexpr std::array<float, 4> level { 1.0f, 0.5f, 0.32f, 0.2f };
    std::array<float, 4> phases {};

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float env = envelope (t, 0.0004f, 0.65f, 1.1f);
        float mix = 0.0f;
        for (size_t k = 0; k < ratio.size(); ++k)
        {
            mix += std::sin (phases[k]) * level[k];
            phases[k] += juce::MathConstants<float>::twoPi * fundamental * ratio[k] / sr;
        }
        const float sample = mix * env * 0.3f;
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    normalise (buffer);
    return buffer;
}

juce::AudioBuffer<float> renderTuba (double sampleRate, juce::uint32 seed)
{
    juce::Random rng (seed);
    const auto sr = (float) sampleRate;
    const float durationSec = 0.55f + rng.nextFloat() * 0.2f;
    const int n = juce::jmax (1, (int) (durationSec * sr));
    juce::AudioBuffer<float> buffer (2, n);

    const float fundamental = 55.0f + rng.nextFloat() * 20.0f;
    static constexpr std::array<float, 4> harmonicRatio { 1.0f, 2.0f, 3.0f, 4.0f };
    static constexpr std::array<float, 4> harmonicLevel { 1.0f, 0.45f, 0.25f, 0.12f };
    std::array<float, 4> phases {};
    OnePole tone; tone.setCutoff (900.0f, sr);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / sr;
        const float env = envelope (t, 0.02f, 0.35f, 2.0f);
        float mix = 0.0f;
        for (size_t k = 0; k < harmonicRatio.size(); ++k)
        {
            mix += std::sin (phases[k]) * harmonicLevel[k];
            phases[k] += juce::MathConstants<float>::twoPi * fundamental * harmonicRatio[k] / sr;
        }
        const float sample = tone.lowpass (mix) * env * 0.5f;
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    normalise (buffer);
    return buffer;
}
}

juce::AudioBuffer<float> generateSample (GeneratedSampleType type, double sampleRate, juce::uint32 seed) noexcept
{
    switch (type)
    {
        case GeneratedSampleType::snare:    return renderSnare    (sampleRate, seed);
        case GeneratedSampleType::hat:      return renderHat      (sampleRate, seed);
        case GeneratedSampleType::clap:     return renderClap     (sampleRate, seed);
        case GeneratedSampleType::bassDrum: return renderBassDrum (sampleRate, seed);
        case GeneratedSampleType::horn:     return renderHorn     (sampleRate, seed);
        case GeneratedSampleType::string:   return renderString   (sampleRate, seed);
        case GeneratedSampleType::pluck:    return renderPluck    (sampleRate, seed);
        case GeneratedSampleType::rimshot:  return renderRimshot  (sampleRate, seed);
        case GeneratedSampleType::triangle: return renderTriangle (sampleRate, seed);
        case GeneratedSampleType::tuba:     return renderTuba     (sampleRate, seed);
    }
    return renderSnare (sampleRate, seed);
}

const char* generatedSampleName (GeneratedSampleType type) noexcept
{
    switch (type)
    {
        case GeneratedSampleType::snare:    return "Generated Snare";
        case GeneratedSampleType::hat:      return "Generated Hat";
        case GeneratedSampleType::clap:     return "Generated Clap";
        case GeneratedSampleType::bassDrum: return "Generated Bass Drum";
        case GeneratedSampleType::horn:     return "Generated Horn";
        case GeneratedSampleType::string:   return "Generated String";
        case GeneratedSampleType::pluck:    return "Generated Pluck";
        case GeneratedSampleType::rimshot:  return "Generated Rimshot";
        case GeneratedSampleType::triangle: return "Generated Triangle";
        case GeneratedSampleType::tuba:     return "Generated Tuba";
    }
    return "Generated Sample";
}
}
