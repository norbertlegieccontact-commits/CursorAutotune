#pragma once

#include <JuceHeader.h>

class PitchShifter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    float processSample (float input, float pitchRatio) noexcept;
    void processBlock (float* samples, int numSamples, float pitchRatio) noexcept;
    void processBlock (float* samples, int numSamples, float pitchRatio, float detectedPitchHz) noexcept;
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    void updatePitchSynchronousWindow (float detectedPitchHz) noexcept;
    float readDelay (float delaySamples) const noexcept;
    static float grainWindow (float phase) noexcept;

    double sampleRate = 44100.0;
    int delaySize = 1;
    int writeIndex = 0;
    int latencySamples = 0;
    float grainLength = 2048.0f;
    float baseDelay = 2048.0f;
    float phase = 0.0f;
    float targetGrainLength = 768.0f;

    std::vector<float> delayBuffer;
};

