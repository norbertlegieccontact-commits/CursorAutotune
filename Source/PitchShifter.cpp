#include "PitchShifter.h"

#include <cmath>

void PitchShifter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = juce::jmax (1.0, spec.sampleRate);
    juce::ignoreUnused (spec);

    fftOrder = 11;
    fftSize = 1 << fftOrder;
    oversampling = 4;
    hopSize = fftSize / oversampling;
    fifoLatency = fftSize - hopSize;
    rover = fifoLatency;

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    const auto fftSizeWithComplexStorage = static_cast<size_t> (fftSize * 2);
    const auto halfFft = static_cast<size_t> (fftSize / 2 + 1);

    inputFifo.assign (static_cast<size_t> (fftSize), 0.0f);
    outputFifo.assign (static_cast<size_t> (fftSize), 0.0f);
    outputAccum.assign (fftSizeWithComplexStorage, 0.0f);
    fftData.assign (fftSizeWithComplexStorage, 0.0f);
    lastPhase.assign (halfFft, 0.0f);
    sumPhase.assign (halfFft, 0.0f);
    analysisMagnitude.assign (halfFft, 0.0f);
    analysisFrequency.assign (halfFft, 0.0f);
    synthesisMagnitude.assign (halfFft, 0.0f);
    synthesisFrequency.assign (halfFft, 0.0f);
    window.assign (static_cast<size_t> (fftSize), 0.0f);

    for (int i = 0; i < fftSize; ++i)
        window[static_cast<size_t> (i)] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                                  * static_cast<float> (i)
                                                                  / static_cast<float> (fftSize));

    reset();
}

void PitchShifter::reset() noexcept
{
    std::fill (inputFifo.begin(), inputFifo.end(), 0.0f);
    std::fill (outputFifo.begin(), outputFifo.end(), 0.0f);
    std::fill (outputAccum.begin(), outputAccum.end(), 0.0f);
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::fill (lastPhase.begin(), lastPhase.end(), 0.0f);
    std::fill (sumPhase.begin(), sumPhase.end(), 0.0f);
    clearSpectrumBuffers();
    rover = fifoLatency;
}

float PitchShifter::processSample (float input, float pitchRatio) noexcept
{
    if (fft == nullptr || inputFifo.empty())
        return input;

    pitchRatio = juce::jlimit (0.5f, 2.0f, pitchRatio);

    const auto output = outputFifo[static_cast<size_t> (rover - fifoLatency)];
    inputFifo[static_cast<size_t> (rover)] = input;
    outputFifo[static_cast<size_t> (rover)] = 0.0f;
    ++rover;

    if (rover >= fftSize)
    {
        rover = fifoLatency;
        processFrame (pitchRatio);
    }

    return output;
}

void PitchShifter::processBlock (float* samples, int numSamples, float pitchRatio) noexcept
{
    if (samples == nullptr)
        return;

    for (int i = 0; i < numSamples; ++i)
        samples[i] = processSample (samples[i], pitchRatio);
}

void PitchShifter::processFrame (float pitchRatio) noexcept
{
    clearSpectrumBuffers();
    std::fill (fftData.begin(), fftData.end(), 0.0f);

    for (int i = 0; i < fftSize; ++i)
        fftData[static_cast<size_t> (i)] = inputFifo[static_cast<size_t> (i)] * window[static_cast<size_t> (i)];

    fft->performRealOnlyForwardTransform (fftData.data());

    const auto expectedPhaseAdvance = juce::MathConstants<float>::twoPi
        * static_cast<float> (hopSize)
        / static_cast<float> (fftSize);
    const auto frequencyPerBin = static_cast<float> (sampleRate) / static_cast<float> (fftSize);

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const auto real = fftData[static_cast<size_t> (2 * bin)];
        const auto imag = fftData[static_cast<size_t> (2 * bin + 1)];
        const auto magnitude = 2.0f * std::sqrt (real * real + imag * imag);
        const auto phase = std::atan2 (imag, real);

        auto phaseDeviation = phase - lastPhase[static_cast<size_t> (bin)];
        lastPhase[static_cast<size_t> (bin)] = phase;

        phaseDeviation -= static_cast<float> (bin) * expectedPhaseAdvance;

        auto wrappedDeviation = static_cast<int> (phaseDeviation / juce::MathConstants<float>::pi);

        if (wrappedDeviation >= 0)
            wrappedDeviation += wrappedDeviation & 1;
        else
            wrappedDeviation -= wrappedDeviation & 1;

        phaseDeviation -= juce::MathConstants<float>::pi * static_cast<float> (wrappedDeviation);
        phaseDeviation = static_cast<float> (oversampling) * phaseDeviation / juce::MathConstants<float>::twoPi;

        const auto trueFrequency = (static_cast<float> (bin) + phaseDeviation) * frequencyPerBin;
        analysisMagnitude[static_cast<size_t> (bin)] = magnitude;
        analysisFrequency[static_cast<size_t> (bin)] = trueFrequency;
    }

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const auto targetBin = static_cast<int> (static_cast<float> (bin) * pitchRatio);

        if (targetBin <= fftSize / 2)
        {
            synthesisMagnitude[static_cast<size_t> (targetBin)] += analysisMagnitude[static_cast<size_t> (bin)];
            synthesisFrequency[static_cast<size_t> (targetBin)] = analysisFrequency[static_cast<size_t> (bin)] * pitchRatio;
        }
    }

    std::fill (fftData.begin(), fftData.end(), 0.0f);

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const auto magnitude = synthesisMagnitude[static_cast<size_t> (bin)];
        auto phaseDeviation = synthesisFrequency[static_cast<size_t> (bin)] - static_cast<float> (bin) * frequencyPerBin;
        phaseDeviation /= frequencyPerBin;
        phaseDeviation = juce::MathConstants<float>::twoPi * phaseDeviation / static_cast<float> (oversampling);
        phaseDeviation += static_cast<float> (bin) * expectedPhaseAdvance;

        sumPhase[static_cast<size_t> (bin)] += phaseDeviation;

        const auto phase = sumPhase[static_cast<size_t> (bin)];
        fftData[static_cast<size_t> (2 * bin)] = magnitude * std::cos (phase);
        fftData[static_cast<size_t> (2 * bin + 1)] = magnitude * std::sin (phase);
    }

    fft->performRealOnlyInverseTransform (fftData.data());

    const auto gain = 1.0f / (static_cast<float> (fftSize) * static_cast<float> (oversampling));

    for (int i = 0; i < fftSize; ++i)
        outputAccum[static_cast<size_t> (i)] += 2.0f
            * window[static_cast<size_t> (i)]
            * fftData[static_cast<size_t> (i)]
            * gain;

    for (int i = 0; i < hopSize; ++i)
        outputFifo[static_cast<size_t> (i)] = outputAccum[static_cast<size_t> (i)];

    std::move (outputAccum.begin() + hopSize,
               outputAccum.begin() + fftSize,
               outputAccum.begin());
    std::fill (outputAccum.begin() + (fftSize - hopSize),
               outputAccum.begin() + fftSize,
               0.0f);

    std::move (inputFifo.begin() + hopSize,
               inputFifo.begin() + fftSize,
               inputFifo.begin());
    std::fill (inputFifo.begin() + (fftSize - hopSize),
               inputFifo.begin() + fftSize,
               0.0f);
}

void PitchShifter::clearSpectrumBuffers() noexcept
{
    std::fill (analysisMagnitude.begin(), analysisMagnitude.end(), 0.0f);
    std::fill (analysisFrequency.begin(), analysisFrequency.end(), 0.0f);
    std::fill (synthesisMagnitude.begin(), synthesisMagnitude.end(), 0.0f);
    std::fill (synthesisFrequency.begin(), synthesisFrequency.end(), 0.0f);
}

