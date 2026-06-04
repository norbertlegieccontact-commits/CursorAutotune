#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
constexpr const char* parameterIds[] =
{
    "retune_speed",
    "flex_tune",
    "humanize",
    "natural_vibrato",
    "key",
    "scale",
    "formant_shift",
    "throat_length",
    "pitch_offset",
    "low_latency_mode",
    "midi_control",
    "target_note_midi",
    "input_gain",
    "output_gain",
    "bypass",
    "mode"
};

juce::WebBrowserComponent::Resource makeIndexResource()
{
    const auto* begin = reinterpret_cast<const std::byte*> (BinaryData::index_html);
    const auto* end = begin + BinaryData::index_htmlSize;
    return { std::vector<std::byte> (begin, end), "text/html" };
}

juce::String makeCustomEventScript (const juce::String& eventName, const juce::var& payload)
{
    return "window.dispatchEvent(new CustomEvent('"
        + eventName
        + "', { detail: "
        + juce::JSON::toString (payload)
        + " }));";
}
}

NorbyAutotuneAudioProcessorEditor::NorbyAutotuneAudioProcessorEditor (NorbyAutotuneAudioProcessor& ownerProcessor)
    : AudioProcessorEditor (&ownerProcessor),
      audioProcessor (ownerProcessor),
      webView (juce::WebBrowserComponent::Options {}
                   .withNativeIntegrationEnabled()
                   .withKeepPageLoadedWhenBrowserIsHidden()
                   .withEventListener ("juce_event",
                                       [this] (const auto& event)
                                       {
                                           handleJavascriptEvent (event);
                                       })
#if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
                   .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
                                       {
                                           if (path == "/" || path == "/index.html")
                                               return makeIndexResource();

                                           return std::nullopt;
                                       })
#endif
      )
{
    setSize (760, 740);
    addAndMakeVisible (webView);

#if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
    webView.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
#else
    auto html = juce::String::fromUTF8 (BinaryData::index_html, BinaryData::index_htmlSize);
    webView.goToURL ("data:text/html;charset=utf-8," + juce::URL::addEscapeChars (html, true));
#endif

    startTimerHz (30);
}

void NorbyAutotuneAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void NorbyAutotuneAudioProcessorEditor::timerCallback()
{
    dispatchPitchData();
    dispatchChangedParameters();
}

void NorbyAutotuneAudioProcessorEditor::dispatchPitchData()
{
    auto pitchData = std::make_unique<juce::DynamicObject>();
    const auto frameData = audioProcessor.getLatestFrameData();

    pitchData->setProperty ("detectedHz", frameData.detectedHz);
    pitchData->setProperty ("correctedHz", frameData.correctedHz);
    pitchData->setProperty ("confidence", frameData.confidence);
    pitchData->setProperty ("note", frameData.note);

    webView.evaluateJavascript (makeCustomEventScript ("pitch_data", juce::var (pitchData.release())));
}

void NorbyAutotuneAudioProcessorEditor::dispatchChangedParameters()
{
    for (const auto* parameterId : parameterIds)
    {
        if (const auto* value = audioProcessor.getValueTreeState().getRawParameterValue (parameterId))
        {
            const auto currentValue = value->load (std::memory_order_relaxed);
            const auto it = lastParameterValues.find (parameterId);

            if (it == lastParameterValues.end() || ! juce::approximatelyEqual (it->second, currentValue))
            {
                lastParameterValues[parameterId] = currentValue;
                dispatchParameterChanged (parameterId, currentValue);
            }
        }
    }
}

void NorbyAutotuneAudioProcessorEditor::dispatchParameterChanged (const juce::String& parameterId, float value)
{
    auto payload = std::make_unique<juce::DynamicObject>();
    payload->setProperty ("name", parameterId);
    payload->setProperty ("value", value);
    webView.evaluateJavascript (makeCustomEventScript ("parameter_changed", juce::var (payload.release())));
}

void NorbyAutotuneAudioProcessorEditor::dispatchVersionInfo()
{
    auto payload = std::make_unique<juce::DynamicObject>();
    payload->setProperty ("version", JucePlugin_VersionString);
    payload->setProperty ("hasUpdate", false);
    webView.evaluateJavascript (makeCustomEventScript ("version_info", juce::var (payload.release())));
}

void NorbyAutotuneAudioProcessorEditor::handleJavascriptEvent (const juce::var& event)
{
    if (! event.isObject())
        return;

    const auto* object = event.getDynamicObject();

    if (object == nullptr)
        return;

    const auto eventId = object->getProperty ("eventId").toString();
    const auto payload = object->getProperty ("payload");

    if (eventId == "set_parameter")
    {
        handleSetParameter (payload);
    }
    else if (eventId == "request_state")
    {
        lastParameterValues.clear();
        dispatchChangedParameters();
        dispatchVersionInfo();
    }
    else if (eventId == "open_website")
    {
        const auto url = payload.getProperty ("url", {}).toString();

        if (url.startsWithIgnoreCase ("https://") || url.startsWithIgnoreCase ("http://"))
            juce::URL (url).launchInDefaultBrowser();
    }
    else if (eventId == "check_updates")
    {
        dispatchVersionInfo();
    }
}

void NorbyAutotuneAudioProcessorEditor::handleSetParameter (const juce::var& payload)
{
    if (! payload.isObject())
        return;

    const auto name = payload.getProperty ("name", {}).toString();
    const auto value = static_cast<float> (payload.getProperty ("value", 0.0));

    if (auto* parameter = audioProcessor.getValueTreeState().getParameter (name))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }
}

