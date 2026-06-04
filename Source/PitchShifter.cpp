#include "PitchShifter.h"

#include <cmath>

void PitchShifter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = juce::jmax (1.0, spec.sampleRate);
    grainLength = static_cast<float> (juce::jlimit (512, 1536, static_cast<int> (sampleRate * 0.015)));
    baseDelay = grainLength;
    latencySamples = static_cast<int> (baseDelay);
    delaySize = static_cast<int> (grainLength * 4.0f) + static_cast<int> (spec.maximumBlockSize) + 8;
    delayBuffer.assign (static_cast<size_t> (delaySize), 0.0f);

    reset();
}

void PitchShifter::reset() noexcept
{
    std::fill (delayBuffer.begin(), delayBuffer.end(), 0.0f);
    writeIndex = 0;
    phase = 0.0f;
}

float PitchShifter::processSample (float input, float pitchRatio) noexcept
{
    if (delayBuffer.empty())
        return input;

    pitchRatio = juce::jlimit (0.5f, 2.0f, pitchRatio);

    delayBuffer[static_cast<size_t> (writeIndex)] = input;

    if (std::abs (pitchRatio - 1.0f) < 0.0005f)
    {
        const auto output = readDelay (baseDelay);
        writeIndex = (writeIndex + 1) % delaySize;
        return output;
    }

    const auto phaseIncrement = (1.0f - pitchRatio) / grainLength;
    phase += phaseIncrement;

    while (phase < 0.0f)
        phase += 1.0f;

    while (phase >= 1.0f)
        phase -= 1.0f;

    auto phaseA = phase;
    auto phaseB = phase + 0.5f;

    if (phaseB >= 1.0f)
        phaseB -= 1.0f;

    const auto delayA = baseDelay + phaseA * grainLength;
    const auto delayB = baseDelay + phaseB * grainLength;
    const auto weightA = grainWindow (phaseA);
    const auto weightB = grainWindow (phaseB);
    const auto normaliser = juce::jmax (0.0001f, weightA + weightB);

    const auto output = (readDelay (delayA) * weightA + readDelay (delayB) * weightB) / normaliser;

    writeIndex = (writeIndex + 1) % delaySize;
    return output;
}

void PitchShifter::processBlock (float* samples, int numSamples, float pitchRatio) noexcept
{
    if (samples == nullptr)
        return;

    for (int i = 0; i < numSamples; ++i)
        samples[i] = processSample (samples[i], pitchRatio);
}

float PitchShifter::readDelay (float delaySamples) const noexcept
{
    const auto readPosition = static_cast<float> (writeIndex) - delaySamples;
    auto wrappedPosition = std::fmod (readPosition, static_cast<float> (delaySize));

    if (wrappedPosition < 0.0f)
        wrappedPosition += static_cast<float> (delaySize);

    const auto index0 = static_cast<int> (wrappedPosition);
    const auto index1 = (index0 + 1) % delaySize;
    const auto fraction = wrappedPosition - static_cast<float> (index0);

    return delayBuffer[static_cast<size_t> (index0)]
        + (delayBuffer[static_cast<size_t> (index1)] - delayBuffer[static_cast<size_t> (index0)]) * fraction;
}

float PitchShifter::grainWindow (float phase) noexcept
{
    return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
}

