#pragma once

#include <JuceHeader.h>

class FormantShifter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    void processBlock (juce::AudioBuffer<float>& buffer, float formantShiftOctaves, float throatLength) noexcept;

private:
    double sampleRate = 44100.0;
};

