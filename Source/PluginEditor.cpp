#include "PluginEditor.h"

NorbyAutotuneAudioProcessorEditor::NorbyAutotuneAudioProcessorEditor (NorbyAutotuneAudioProcessor& processor)
    : AudioProcessorEditor (&processor),
      audioProcessor (processor)
{
    setSize (900, 600);
    addAndMakeVisible (webView);
    webView.goToURL ("about:blank");
    startTimerHz (30);
}

void NorbyAutotuneAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void NorbyAutotuneAudioProcessorEditor::timerCallback()
{
    dispatchPitchData();
}

void NorbyAutotuneAudioProcessorEditor::dispatchPitchData()
{
    auto pitchData = std::make_unique<juce::DynamicObject>();
    const auto frameData = audioProcessor.getLatestFrameData();

    pitchData->setProperty ("detectedHz", frameData.detectedHz);
    pitchData->setProperty ("correctedHz", frameData.correctedHz);
    pitchData->setProperty ("confidence", frameData.confidence);
    pitchData->setProperty ("note", frameData.note);

    const auto payload = juce::JSON::toString (juce::var (pitchData.release()));
    const auto script = "window.dispatchEvent(new CustomEvent('pitch_data', { detail: "
        + payload
        + " }));";

    webView.evaluateJavascript (script);
}

