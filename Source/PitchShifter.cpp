#include "PitchShifter.h"

#include <cmath>

void PitchShifter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = juce::jmax (1.0, spec.sampleRate);
    latencySamples = static_cast<int> (juce::jlimit (1024, 4096, static_cast<int> (sampleRate * 0.045)));
    maxGrainRadius = static_cast<int> (juce::jlimit (512, 2048, static_cast<int> (sampleRate * 0.025)));
    ringSize = juce::nextPowerOfTwo (latencySamples + maxGrainRadius * 4 + static_cast<int> (spec.maximumBlockSize) * 4 + 8);

    inputRing.assign (static_cast<size_t> (ringSize), 0.0f);
    dryRing.assign (static_cast<size_t> (ringSize), 0.0f);
    outputRing.assign (static_cast<size_t> (ringSize), 0.0f);
    weightRing.assign (static_cast<size_t> (ringSize), 0.0f);

    reset();
}

void PitchShifter::reset() noexcept
{
    std::fill (inputRing.begin(), inputRing.end(), 0.0f);
    std::fill (dryRing.begin(), dryRing.end(), 0.0f);
    std::fill (outputRing.begin(), outputRing.end(), 0.0f);
    std::fill (weightRing.begin(), weightRing.end(), 0.0f);
    epochHistory.fill (0);
    epochWriteIndex = 0;
    epochCount = 0;
    currentSample = 0;
    nextAnalysisEpoch = 0;
    nextSynthesisEpoch = static_cast<double> (latencySamples);
    previousPeriodSamples = 0.0f;
    previousPitchRatio = 1.0f;
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
    if (samples == nullptr || inputRing.empty() || numSamples <= 0)
        return;

    const auto blockStart = currentSample;
    const auto blockEnd = blockStart + numSamples;
    const auto validPitch = detectedPitchHz >= 55.0f && detectedPitchHz <= 1000.0f;
    const auto periodSamples = validPitch ? static_cast<float> (sampleRate / detectedPitchHz) : 0.0f;
    const auto ratioCents = 1200.0f * std::log2 (juce::jmax (0.0001f, pitchRatio));
    const auto active = validPitch
        && periodSamples >= 44.0f
        && periodSamples <= 802.0f
        && std::abs (ratioCents) >= 5.0f
        && std::abs (ratioCents) <= 180.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto absoluteSample = blockStart + i;
        const auto ringIndex = static_cast<int> (absoluteSample & (ringSize - 1));
        inputRing[static_cast<size_t> (ringIndex)] = samples[i];
        dryRing[static_cast<size_t> (ringIndex)] = samples[i];

        const auto weight = weightRing[static_cast<size_t> (ringIndex)];
        const auto olaSample = weight > 0.0001f
            ? outputRing[static_cast<size_t> (ringIndex)] / weight
            : readDryAt (absoluteSample - latencySamples);

        samples[i] = active ? olaSample : readDryAt (absoluteSample - latencySamples);

        outputRing[static_cast<size_t> (ringIndex)] = 0.0f;
        weightRing[static_cast<size_t> (ringIndex)] = 0.0f;
    }

    currentSample = blockEnd;

    if (! active)
    {
        previousPeriodSamples = 0.0f;
        previousPitchRatio = 1.0f;
        nextSynthesisEpoch = static_cast<double> (blockEnd + latencySamples);
        nextAnalysisEpoch = blockEnd;
        return;
    }

    if (previousPeriodSamples <= 0.0f
        || std::abs (periodSamples - previousPeriodSamples) > previousPeriodSamples * 0.35f
        || std::abs (pitchRatio - previousPitchRatio) > 0.08f)
    {
        nextAnalysisEpoch = blockStart;
        nextSynthesisEpoch = static_cast<double> (blockStart + latencySamples);
    }

    previousPeriodSamples = periodSamples;
    previousPitchRatio = pitchRatio;

    detectEpochs (blockStart, blockEnd, periodSamples);
    scheduleSynthesisGrains (blockStart, blockEnd, pitchRatio, periodSamples);
}

void PitchShifter::detectEpochs (int64 blockStart, int64 blockEnd, float periodSamples) noexcept
{
    const auto searchRadius = static_cast<int64> (juce::jlimit (8, 128, static_cast<int> (periodSamples * 0.28f)));

    if (nextAnalysisEpoch <= 0 || nextAnalysisEpoch < blockStart - static_cast<int64> (periodSamples))
        nextAnalysisEpoch = blockStart + static_cast<int64> (periodSamples * 0.5f);

    while (nextAnalysisEpoch < blockEnd)
    {
        const auto start = juce::jmax (blockStart, nextAnalysisEpoch - searchRadius);
        const auto end = juce::jmin (blockEnd - 1, nextAnalysisEpoch + searchRadius);
        auto bestPosition = nextAnalysisEpoch;
        auto bestValue = 0.0f;

        for (auto sample = start; sample <= end; ++sample)
        {
            const auto previous = readInputAt (sample - 1);
            const auto current = readInputAt (sample);
            const auto next = readInputAt (sample + 1);
            const auto isPeak = current >= previous && current >= next;
            const auto value = isPeak ? std::abs (current) : std::abs (current) * 0.35f;

            if (value > bestValue)
            {
                bestValue = value;
                bestPosition = sample;
            }
        }

        if (bestValue > 0.004f)
        {
            epochHistory[static_cast<size_t> (epochWriteIndex)] = bestPosition;
            epochWriteIndex = (epochWriteIndex + 1) % static_cast<int> (epochHistory.size());
            epochCount = juce::jmin (epochCount + 1, static_cast<int> (epochHistory.size()));
        }

        nextAnalysisEpoch = bestPosition + static_cast<int64> (periodSamples);
    }
}

void PitchShifter::scheduleSynthesisGrains (int64 blockStart, int64 blockEnd, float pitchRatio, float periodSamples) noexcept
{
    const auto targetPeriod = periodSamples / juce::jlimit (0.8f, 1.25f, pitchRatio);
    const auto outputStart = static_cast<double> (blockStart + latencySamples);
    const auto outputEnd = static_cast<double> (blockEnd + latencySamples);
    const auto grainRadius = juce::jlimit (48, maxGrainRadius, static_cast<int> (periodSamples * 1.15f));

    if (nextSynthesisEpoch < outputStart - targetPeriod)
        nextSynthesisEpoch = outputStart;

    while (nextSynthesisEpoch < outputEnd)
    {
        const auto synthesisEpoch = static_cast<int64> (std::llround (nextSynthesisEpoch));
        const auto analysisTarget = synthesisEpoch - latencySamples;
        const auto analysisEpoch = findNearestEpoch (analysisTarget, periodSamples * 0.65f);

        if (analysisEpoch >= 0)
            addGrain (analysisEpoch, synthesisEpoch, grainRadius);

        nextSynthesisEpoch += targetPeriod;
    }
}

void PitchShifter::addGrain (int64 analysisEpoch, int64 synthesisEpoch, int grainRadius) noexcept
{
    for (int offset = -grainRadius; offset <= grainRadius; ++offset)
    {
        const auto outputSample = synthesisEpoch + offset;
        const auto outputIndex = static_cast<int> (outputSample & (ringSize - 1));
        const auto window = hannWindow (offset, grainRadius);
        const auto input = readInputAt (analysisEpoch + offset);

        outputRing[static_cast<size_t> (outputIndex)] += input * window;
        weightRing[static_cast<size_t> (outputIndex)] += window;
    }
}

int64 PitchShifter::findNearestEpoch (int64 target, float maxDistance) const noexcept
{
    auto bestEpoch = static_cast<int64> (-1);
    auto bestDistance = static_cast<int64> (maxDistance) + 1;

    for (int i = 0; i < epochCount; ++i)
    {
        const auto index = (epochWriteIndex - 1 - i + static_cast<int> (epochHistory.size()))
            % static_cast<int> (epochHistory.size());
        const auto epoch = epochHistory[static_cast<size_t> (index)];
        const auto distance = std::llabs (epoch - target);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestEpoch = epoch;
        }
    }

    return bestEpoch;
}

float PitchShifter::readInputAt (int64 absoluteSample) const noexcept
{
    return inputRing[static_cast<size_t> (absoluteSample & (ringSize - 1))];
}

float PitchShifter::readDryAt (int64 absoluteSample) const noexcept
{
    return dryRing[static_cast<size_t> (absoluteSample & (ringSize - 1))];
}

float PitchShifter::hannWindow (int offset, int radius) noexcept
{
    const auto phase = (static_cast<float> (offset + radius) / static_cast<float> (radius * 2));
    return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
}

