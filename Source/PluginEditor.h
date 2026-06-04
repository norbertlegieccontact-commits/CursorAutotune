#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

class NorbyAutotuneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit NorbyAutotuneAudioProcessorEditor (NorbyAutotuneAudioProcessor& processor);
    ~NorbyAutotuneAudioProcessorEditor() override = default;

    void resized() override;

private:
    void timerCallback() override;
    void dispatchPitchData();

    NorbyAutotuneAudioProcessor& audioProcessor;
    juce::WebBrowserComponent webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorbyAutotuneAudioProcessorEditor)
};

