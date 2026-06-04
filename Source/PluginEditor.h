#pragma once

#include <JuceHeader.h>
#include <map>

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
    void dispatchChangedParameters();
    void dispatchParameterChanged (const juce::String& parameterId, float value);
    void dispatchVersionInfo();
    void handleJavascriptEvent (const juce::var& event);
    void handleSetParameter (const juce::var& payload);

    NorbyAutotuneAudioProcessor& audioProcessor;
    juce::WebBrowserComponent webView;
    std::map<juce::String, float> lastParameterValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorbyAutotuneAudioProcessorEditor)
};

