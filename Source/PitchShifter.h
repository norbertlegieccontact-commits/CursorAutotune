#pragma once

#include <JuceHeader.h>
#include <array>

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
    void detectEpochs (int64 blockStart, int64 blockEnd, float periodSamples) noexcept;
    void scheduleSynthesisGrains (int64 blockStart, int64 blockEnd, float pitchRatio, float periodSamples) noexcept;
    void addGrain (int64 analysisEpoch, int64 synthesisEpoch, int grainRadius) noexcept;
    int64 findNearestEpoch (int64 target, float maxDistance) const noexcept;
    float readInputAt (int64 absoluteSample) const noexcept;
    float readDryAt (int64 absoluteSample) const noexcept;
    static float hannWindow (int offset, int radius) noexcept;

    double sampleRate = 44100.0;
    int latencySamples = 2048;
    int ringSize = 16384;
    int maxGrainRadius = 1024;
    int64 currentSample = 0;
    int64 nextAnalysisEpoch = 0;
    double nextSynthesisEpoch = 0.0;
    float previousPeriodSamples = 0.0f;
    float previousPitchRatio = 1.0f;

    std::vector<float> inputRing;
    std::vector<float> dryRing;
    std::vector<float> outputRing;
    std::vector<float> weightRing;

    std::array<int64, 256> epochHistory {};
    int epochWriteIndex = 0;
    int epochCount = 0;
};

