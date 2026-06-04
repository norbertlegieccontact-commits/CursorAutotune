#include "PitchDetector.h"

#include <cmath>

void PitchDetector::prepare (double newSampleRate, int newAnalysisSize, int newHopSize)
{
    sampleRate = juce::jmax (1.0, newSampleRate);
    analysisSize = juce::jmax (512, newAnalysisSize);
    hopSize = juce::jmax (64, newHopSize);

    ringBuffer.assign (static_cast<size_t> (analysisSize), 0.0f);
    yinBuffer.assign (static_cast<size_t> (analysisSize / 2 + 1), 0.0f);
    reset();
}

void PitchDetector::reset() noexcept
{
    std::fill (ringBuffer.begin(), ringBuffer.end(), 0.0f);
    std::fill (yinBuffer.begin(), yinBuffer.end(), 0.0f);

    writeIndex = 0;
    samplesWritten = 0;
    samplesSinceAnalysis = 0;
    lastResult = {};
}

PitchDetectionResult PitchDetector::processBlock (const float* samples, int numSamples) noexcept
{
    if (samples == nullptr || ringBuffer.empty())
        return lastResult;

    for (int i = 0; i < numSamples; ++i)
    {
        ringBuffer[static_cast<size_t> (writeIndex)] = samples[i];
        writeIndex = (writeIndex + 1) % analysisSize;
        samplesWritten = juce::jmin (analysisSize, samplesWritten + 1);
        ++samplesSinceAnalysis;
    }

    if (samplesWritten >= analysisSize && samplesSinceAnalysis >= hopSize)
    {
        samplesSinceAnalysis = 0;
        lastResult = analyseYin();

        if (! lastResult.voiced)
            lastResult = analyseAutocorrelation();
    }

    return lastResult;
}

float PitchDetector::readFrameSample (int index) const noexcept
{
    const auto oldestIndex = writeIndex;
    const auto wrapped = (oldestIndex + index) % analysisSize;
    return ringBuffer[static_cast<size_t> (wrapped)];
}

PitchDetectionResult PitchDetector::analyseYin() noexcept
{
    const int minTau = juce::jlimit (2, analysisSize / 4, static_cast<int> (sampleRate / 1000.0));
    const int maxTau = juce::jlimit (minTau + 1, analysisSize / 2, static_cast<int> (sampleRate / 50.0));

    yinBuffer[0] = 1.0f;

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double difference = 0.0;
        const int limit = analysisSize - tau;

        for (int i = 0; i < limit; ++i)
        {
            const auto delta = static_cast<double> (readFrameSample (i) - readFrameSample (i + tau));
            difference += delta * delta;
        }

        yinBuffer[static_cast<size_t> (tau)] = static_cast<float> (difference);
    }

    double runningSum = 0.0;
    int bestTau = -1;

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        runningSum += yinBuffer[static_cast<size_t> (tau)];
        yinBuffer[static_cast<size_t> (tau)] = runningSum > 0.0
            ? static_cast<float> (yinBuffer[static_cast<size_t> (tau)] * tau / runningSum)
            : 1.0f;

        if (tau >= minTau && yinBuffer[static_cast<size_t> (tau)] < yinThreshold)
        {
            while (tau + 1 <= maxTau
                   && yinBuffer[static_cast<size_t> (tau + 1)] < yinBuffer[static_cast<size_t> (tau)])
                ++tau;

            bestTau = tau;
            break;
        }
    }

    if (bestTau < 0)
    {
        float lowest = 1.0f;

        for (int tau = minTau; tau <= maxTau; ++tau)
        {
            const auto value = yinBuffer[static_cast<size_t> (tau)];

            if (value < lowest)
            {
                lowest = value;
                bestTau = tau;
            }
        }
    }

    if (bestTau <= 0)
        return {};

    const auto refinedTau = juce::jmax (1.0f, parabolicInterpolate (bestTau));
    const auto confidence = juce::jlimit (0.0f, 1.0f, 1.0f - yinBuffer[static_cast<size_t> (bestTau)]);

    PitchDetectionResult result;
    result.detectedHz = static_cast<float> (sampleRate / refinedTau);
    result.confidence = confidence;
    result.voiced = yinBuffer[static_cast<size_t> (bestTau)] < yinThreshold;
    return result;
}

PitchDetectionResult PitchDetector::analyseAutocorrelation() noexcept
{
    const int minLag = juce::jlimit (2, analysisSize / 4, static_cast<int> (sampleRate / 1000.0));
    const int maxLag = juce::jlimit (minLag + 1, analysisSize / 2, static_cast<int> (sampleRate / 50.0));

    double zeroLag = 0.0;

    for (int i = 0; i < analysisSize; ++i)
    {
        const auto sample = static_cast<double> (readFrameSample (i));
        zeroLag += sample * sample;
    }

    if (zeroLag <= 1.0e-9)
        return {};

    int bestLag = -1;
    double bestCorrelation = 0.0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double correlation = 0.0;
        const int limit = analysisSize - lag;

        for (int i = 0; i < limit; ++i)
            correlation += static_cast<double> (readFrameSample (i)) * readFrameSample (i + lag);

        correlation /= zeroLag;

        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }

    PitchDetectionResult result;
    result.detectedHz = bestLag > 0 ? static_cast<float> (sampleRate / bestLag) : 0.0f;
    result.confidence = juce::jlimit (0.0f, 1.0f, static_cast<float> (bestCorrelation));
    result.voiced = result.confidence >= 0.75f;
    return result;
}

float PitchDetector::parabolicInterpolate (int tau) const noexcept
{
    if (tau <= 1 || tau + 1 >= static_cast<int> (yinBuffer.size()))
        return static_cast<float> (tau);

    const auto left = yinBuffer[static_cast<size_t> (tau - 1)];
    const auto centre = yinBuffer[static_cast<size_t> (tau)];
    const auto right = yinBuffer[static_cast<size_t> (tau + 1)];
    const auto divisor = left - 2.0f * centre + right;

    if (std::abs (divisor) < 1.0e-9f)
        return static_cast<float> (tau);

    return static_cast<float> (tau) + 0.5f * (left - right) / divisor;
}

