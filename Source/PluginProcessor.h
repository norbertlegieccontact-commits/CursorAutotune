#pragma once

#include <JuceHeader.h>
#include <atomic>

#include "AutotuneEngine.h"
#include "GraphicalMode.h"

class NorbyAutotuneAudioProcessor final : public juce::AudioProcessor
{
public:
    NorbyAutotuneAudioProcessor();
    ~NorbyAutotuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.08; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override { juce::ignoreUnused (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    GraphicalMode& getGraphicalMode() noexcept { return graphicalMode; }

    AutotuneFrameData getLatestFrameData() const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    AutotuneParameters readParameters() const noexcept;
    double getPlayheadSeconds() const;

    juce::AudioProcessorValueTreeState apvts;
    AutotuneEngine engine;
    GraphicalMode graphicalMode;

    std::atomic<float> latestDetectedHz { 0.0f };
    std::atomic<float> latestCorrectedHz { 0.0f };
    std::atomic<float> latestConfidence { 0.0f };
    std::atomic<int> latestMidiNote { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorbyAutotuneAudioProcessor)
};

