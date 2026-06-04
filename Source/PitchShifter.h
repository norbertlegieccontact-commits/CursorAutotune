#pragma once

#include <JuceHeader.h>

class PitchShifter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    float processSample (float input, float pitchRatio) noexcept;
    void processBlock (float* samples, int numSamples, float pitchRatio) noexcept;
    int getLatencySamples() const noexcept { return fifoLatency; }

private:
    void processFrame (float pitchRatio) noexcept;
    void clearSpectrumBuffers() noexcept;

    double sampleRate = 44100.0;
    int fftOrder = 11;
    int fftSize = 2048;
    int hopSize = 512;
    int oversampling = 4;
    int fifoLatency = 1536;
    int rover = 0;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> inputFifo;
    std::vector<float> outputFifo;
    std::vector<float> outputAccum;
    std::vector<float> fftData;
    std::vector<float> lastPhase;
    std::vector<float> sumPhase;
    std::vector<float> analysisMagnitude;
    std::vector<float> analysisFrequency;
    std::vector<float> synthesisMagnitude;
    std::vector<float> synthesisFrequency;
    std::vector<float> window;
};

