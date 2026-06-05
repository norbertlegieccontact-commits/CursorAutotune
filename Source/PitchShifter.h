#pragma once

#include <JuceHeader.h>
#include <array>

namespace signalsmith::stretch
{
template <typename Sample, typename RandomEngine>
struct SignalsmithStretch;
}

struct SignalsmithStretchDeleter
{
    void operator() (signalsmith::stretch::SignalsmithStretch<float, void>* pointer) const noexcept;
};

class PitchShifter
{
public:
    ~PitchShifter();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    float processSample (float input, float pitchRatio) noexcept;
    void processBlock (float* samples, int numSamples, float pitchRatio) noexcept;
    void processBlock (float* samples, int numSamples, float pitchRatio, float detectedPitchHz) noexcept;
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    double sampleRate = 44100.0;
    int latencySamples = 0;
    int maximumBlockSize = 0;
    float lastPitchRatio = 1.0f;

    std::unique_ptr<signalsmith::stretch::SignalsmithStretch<float, void>, SignalsmithStretchDeleter> stretch;
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    std::array<const float*, 1> inputPointers {};
    std::array<float*, 1> outputPointers {};
};

