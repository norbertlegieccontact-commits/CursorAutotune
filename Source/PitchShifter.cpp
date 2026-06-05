#include <signalsmith-stretch/signalsmith-stretch.h>

#include "PitchShifter.h"

#include <cmath>

void SignalsmithStretchDeleter::operator() (signalsmith::stretch::SignalsmithStretch<float, void>* pointer) const noexcept
{
    delete pointer;
}

PitchShifter::~PitchShifter() = default;

void PitchShifter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = juce::jmax (1.0, spec.sampleRate);
    maximumBlockSize = static_cast<int> (spec.maximumBlockSize);

    stretch = std::make_unique<signalsmith::stretch::SignalsmithStretch<float, void>> (0);
    stretch->presetCheaper (1, static_cast<float> (sampleRate), true);
    stretch->setTransposeFactor (1.0f);
    stretch->setFormantFactor (1.0f);
    stretch->setFormantBase (200.0f / static_cast<float> (sampleRate));

    latencySamples = stretch->inputLatency() + stretch->outputLatency();
    inputBuffer.assign (static_cast<size_t> (juce::jmax (maximumBlockSize, 1)), 0.0f);
    outputBuffer.assign (static_cast<size_t> (juce::jmax (maximumBlockSize, 1)), 0.0f);
    inputPointers[0] = inputBuffer.data();
    outputPointers[0] = outputBuffer.data();

    reset();
}

void PitchShifter::reset() noexcept
{
    if (stretch != nullptr)
        stretch->reset();

    std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
    std::fill (outputBuffer.begin(), outputBuffer.end(), 0.0f);
    lastPitchRatio = 1.0f;
}

float PitchShifter::processSample (float input, float pitchRatio) noexcept
{
    float sample = input;
    processBlock (&sample, 1, pitchRatio, 0.0f);
    return sample;
}

void PitchShifter::processBlock (float* samples, int numSamples, float pitchRatio) noexcept
{
    processBlock (samples, numSamples, pitchRatio, 0.0f);
}

void PitchShifter::processBlock (float* samples, int numSamples, float pitchRatio, float detectedPitchHz) noexcept
{
    if (samples == nullptr || stretch == nullptr || numSamples <= 0)
        return;

    if (numSamples > static_cast<int> (inputBuffer.size()))
        return;

    pitchRatio = juce::jlimit (0.5f, 2.0f, pitchRatio);
    lastPitchRatio += (pitchRatio - lastPitchRatio) * 0.25f;

    stretch->setTransposeFactor (lastPitchRatio, 8000.0f / static_cast<float> (sampleRate));

    if (detectedPitchHz > 55.0f && detectedPitchHz < 1000.0f)
        stretch->setFormantBase (detectedPitchHz / static_cast<float> (sampleRate));

    std::copy (samples, samples + numSamples, inputBuffer.begin());
    std::fill (outputBuffer.begin(), outputBuffer.begin() + numSamples, 0.0f);

    inputPointers[0] = inputBuffer.data();
    outputPointers[0] = outputBuffer.data();
    stretch->process (inputPointers.data(), numSamples, outputPointers.data(), numSamples);
    std::copy (outputBuffer.begin(), outputBuffer.begin() + numSamples, samples);
}


