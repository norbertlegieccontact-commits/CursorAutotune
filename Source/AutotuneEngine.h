#pragma once

#include <JuceHeader.h>

#include "FormantShifter.h"
#include "GraphicalMode.h"
#include "MidiController.h"
#include "PitchDetector.h"
#include "PitchShifter.h"
#include "ScaleQuantizer.h"

struct AutotuneParameters
{
    float retuneSpeed = 20.0f;
    float flexTune = 0.0f;
    float humanize = 0.0f;
    float naturalVibrato = 0.0f;
    Key key = Key::C;
    Scale scale = Scale::Major;
    float formantShift = 0.0f;
    float throatLength = 0.0f;
    float pitchOffset = 0.0f;
    bool lowLatencyMode = false;
    bool midiControl = false;
    int targetNoteMidi = -1;
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    bool bypass = false;
    int mode = 0;
};

struct AutotuneFrameData
{
    float detectedHz = 0.0f;
    float correctedHz = 0.0f;
    float confidence = 0.0f;
    juce::String note;
};

class AutotuneEngine
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    AutotuneFrameData processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages,
                                    const AutotuneParameters& parameters,
                                    double playheadSeconds,
                                    const GraphicalMode& graphicalMode) noexcept;

    int getLatencySamples() const noexcept { return 0; }

private:
    PitchDetectionResult stabiliseDetection (PitchDetectionResult detection, int numSamples) noexcept;
    void updateOnsetState (float blockRms, int numSamples) noexcept;
    int stabiliseTargetMidiNote (int proposedMidiNote, int numSamples) noexcept;
    float processAlignedDrySample (float sample) noexcept;
    float deClickAndLimit (float sample) noexcept;
    static float softLimit (float sample) noexcept;

    float calculateTargetRatio (const PitchDetectionResult& detection,
                                const AutotuneParameters& parameters,
                                const GraphicalMode& graphicalMode,
                                double playheadSeconds,
                                int numSamples,
                                AutotuneFrameData& frameData) noexcept;

    float smoothRatio (float targetRatio,
                       float retuneSpeed,
                       float humanize,
                       int targetMidiNote,
                       int numSamples) noexcept;

    double sampleRate = 44100.0;
    int maximumBlockSize = 512;
    float smoothedRatio = 1.0f;
    float smoothedDetectedHz = 0.0f;
    float smoothedConfidence = 0.0f;
    float correctionBlend = 0.0f;
    float inputLevelEnvelope = 0.0f;
    float previousOutputSample = 0.0f;
    int lastTargetMidiNote = -1;
    int activeTargetMidiNote = -1;
    int candidateTargetMidiNote = -1;
    int candidateTargetSamples = 0;
    int attackProtectionSamplesRemaining = 0;
    int pitchHoldSamplesRemaining = 0;
    double currentNoteSeconds = 0.0;

    PitchDetector detector;
    ScaleQuantizer quantizer;
    MidiController midiController;
    FormantShifter formantShifter;
    PitchShifter pitchShifter;
    std::vector<float> monoBuffer;
    std::vector<float> correctedMonoBuffer;
    std::vector<float> alignedDryBuffer;
    std::vector<float> dryDelayBuffer;
    int dryDelayWriteIndex = 0;
    int dryDelaySamples = 0;
};

