#include "FormantShifter.h"

void FormantShifter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = juce::jmax (1.0, spec.sampleRate);
    juce::ignoreUnused (sampleRate);
}

void FormantShifter::reset() noexcept
{
}

void FormantShifter::processBlock (juce::AudioBuffer<float>& buffer,
                                   float formantShiftOctaves,
                                   float throatLength) noexcept
{
    juce::ignoreUnused (buffer, formantShiftOctaves, throatLength);
}

