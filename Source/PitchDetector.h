#pragma once

#include <JuceHeader.h>

struct PitchDetectionResult
{
    float detectedHz = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
};

class PitchDetector
{
public:
    void prepare (double newSampleRate, int newAnalysisSize = 2048, int newHopSize = 512);
    void reset() noexcept;

    PitchDetectionResult processBlock (const float* samples, int numSamples) noexcept;
    PitchDetectionResult getLastResult() const noexcept { return lastResult; }

private:
    float readFrameSample (int index) const noexcept;
    PitchDetectionResult analyseYin() noexcept;
    PitchDetectionResult analyseAutocorrelation() noexcept;
    float parabolicInterpolate (int tau) const noexcept;

    double sampleRate = 44100.0;
    int analysisSize = 2048;
    int hopSize = 512;
    int writeIndex = 0;
    int samplesWritten = 0;
    int samplesSinceAnalysis = 0;

    std::vector<float> ringBuffer;
    std::vector<float> yinBuffer;
    PitchDetectionResult lastResult;

    static constexpr float yinThreshold = 0.15f;
};

