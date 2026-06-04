#include "PluginProcessor.h"
#include "PluginEditor.h"

NorbyAutotuneAudioProcessor::NorbyAutotuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void NorbyAutotuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    engine.prepare (spec);
    setLatencySamples (engine.getLatencySamples());
}

void NorbyAutotuneAudioProcessor::releaseResources()
{
    engine.reset();
}

bool NorbyAutotuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != mainOutput)
        return false;

    return mainOutput == juce::AudioChannelSet::mono()
        || mainOutput == juce::AudioChannelSet::stereo();
}

void NorbyAutotuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    const auto frameData = engine.processBlock (buffer,
                                                midiMessages,
                                                readParameters(),
                                                getPlayheadSeconds(),
                                                graphicalMode);

    latestDetectedHz.store (frameData.detectedHz, std::memory_order_relaxed);
    latestCorrectedHz.store (frameData.correctedHz, std::memory_order_relaxed);
    latestConfidence.store (frameData.confidence, std::memory_order_relaxed);
    latestMidiNote.store (frameData.note.isNotEmpty()
                              ? static_cast<int> (std::round (ScaleQuantizer::hzToMidiNote (frameData.correctedHz)))
                              : -1,
                          std::memory_order_relaxed);
}

juce::AudioProcessorEditor* NorbyAutotuneAudioProcessor::createEditor()
{
    return new NorbyAutotuneAudioProcessorEditor (*this);
}

const juce::String NorbyAutotuneAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NorbyAutotuneAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NorbyAutotuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto root = std::make_unique<juce::XmlElement> ("NORBY_AUTOTUNE_STATE");

    if (auto stateXml = apvts.copyState().createXml())
        root->addChildElement (stateXml.release());

    root->addChildElement (graphicalMode.createXml().release());
    copyXmlToBinary (*root, destData);
}

void NorbyAutotuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto root = getXmlFromBinary (data, sizeInBytes))
    {
        if (root->hasTagName ("NORBY_AUTOTUNE_STATE"))
        {
            for (const auto* child : root->getChildIterator())
            {
                if (child->hasTagName ("GRAPHICAL_MODE"))
                    graphicalMode.restoreFromXml (child);
                else
                    apvts.replaceState (juce::ValueTree::fromXml (*child));
            }
        }
        else
        {
            apvts.replaceState (juce::ValueTree::fromXml (*root));
        }
    }
}

AutotuneFrameData NorbyAutotuneAudioProcessor::getLatestFrameData() const
{
    AutotuneFrameData frameData;
    frameData.detectedHz = latestDetectedHz.load (std::memory_order_relaxed);
    frameData.correctedHz = latestCorrectedHz.load (std::memory_order_relaxed);
    frameData.confidence = latestConfidence.load (std::memory_order_relaxed);

    const auto midiNote = latestMidiNote.load (std::memory_order_relaxed);
    frameData.note = midiNote >= 0 ? ScaleQuantizer::midiNoteName (midiNote) : juce::String();
    return frameData;
}

juce::AudioProcessorValueTreeState::ParameterLayout
NorbyAutotuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "retune_speed", 1 }, "Retune Speed",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 20.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "flex_tune", 1 }, "Flex-Tune",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "humanize", 1 }, "Humanize",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "natural_vibrato", 1 }, "Natural Vibrato",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.01f }, 0.0f));

    parameters.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "key", 1 }, "Key", 0, 12, 0));
    parameters.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "scale", 1 }, "Scale", 0, 12, 0));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "formant_shift", 1 }, "Formant Shift",
        juce::NormalisableRange<float> { -2.0f, 2.0f, 0.001f }, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "throat_length", 1 }, "Throat Length",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.01f }, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "pitch_offset", 1 }, "Pitch Offset",
        juce::NormalisableRange<float> { -2.0f, 2.0f, 0.001f }, 0.0f));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "low_latency_mode", 1 }, "Low Latency Mode", false));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "midi_control", 1 }, "MIDI Control", false));
    parameters.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "target_note_midi", 1 }, "Target Note MIDI", -1, 127, -1));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "input_gain", 1 }, "Input Gain",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output_gain", 1 }, "Output Gain",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));
    parameters.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "mode", 1 }, "Mode", 0, 1, 0));

    return { parameters.begin(), parameters.end() };
}

AutotuneParameters NorbyAutotuneAudioProcessor::readParameters() const noexcept
{
    const auto get = [this] (const char* id) noexcept
    {
        return apvts.getRawParameterValue (id)->load (std::memory_order_relaxed);
    };

    AutotuneParameters parameters;
    parameters.retuneSpeed = get ("retune_speed");
    parameters.flexTune = get ("flex_tune");
    parameters.humanize = get ("humanize");
    parameters.naturalVibrato = get ("natural_vibrato");
    parameters.key = static_cast<Key> (juce::jlimit (0, 12, static_cast<int> (std::round (get ("key")))));
    parameters.scale = static_cast<Scale> (juce::jlimit (0, 12, static_cast<int> (std::round (get ("scale")))));
    parameters.formantShift = get ("formant_shift");
    parameters.throatLength = get ("throat_length");
    parameters.pitchOffset = get ("pitch_offset");
    parameters.lowLatencyMode = get ("low_latency_mode") > 0.5f;
    parameters.midiControl = get ("midi_control") > 0.5f;
    parameters.targetNoteMidi = juce::jlimit (-1, 127, static_cast<int> (std::round (get ("target_note_midi"))));
    parameters.inputGainDb = get ("input_gain");
    parameters.outputGainDb = get ("output_gain");
    parameters.bypass = get ("bypass") > 0.5f;
    parameters.mode = juce::jlimit (0, 1, static_cast<int> (std::round (get ("mode"))));
    return parameters;
}

double NorbyAutotuneAudioProcessor::getPlayheadSeconds() const
{
    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto timeInSeconds = position->getTimeInSeconds())
                return *timeInSeconds;
        }
    }

    return 0.0;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NorbyAutotuneAudioProcessor();
}

