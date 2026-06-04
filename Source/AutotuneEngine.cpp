#include "AutotuneEngine.h"

#include <cmath>

void AutotuneEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = juce::jmax (1.0, spec.sampleRate);
    maximumBlockSize = static_cast<int> (spec.maximumBlockSize);

    detector.prepare (sampleRate, 2048, 512);
    formantShifter.prepare (spec);

    for (auto& shifter : pitchShifters)
        shifter.prepare (spec);

    reset();
}

void AutotuneEngine::reset() noexcept
{
    detector.reset();
    midiController.reset();
    formantShifter.reset();

    for (auto& shifter : pitchShifters)
        shifter.reset();

    smoothedRatio = 1.0f;
    lastTargetMidiNote = -1;
    currentNoteSeconds = 0.0;
}

AutotuneFrameData AutotuneEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages,
                                                const AutotuneParameters& parameters,
                                                double playheadSeconds,
                                                const GraphicalMode& graphicalMode) noexcept
{
    AutotuneFrameData frameData;

    midiController.processMidi (midiMessages);

    if (parameters.bypass)
    {
        const auto detection = detector.processBlock (buffer.getReadPointer (0), buffer.getNumSamples());
        frameData.detectedHz = detection.detectedHz;
        frameData.confidence = detection.confidence;
        frameData.correctedHz = detection.detectedHz;
        frameData.note = detection.detectedHz > 0.0f
            ? ScaleQuantizer::midiNoteName (static_cast<int> (std::round (ScaleQuantizer::hzToMidiNote (detection.detectedHz))))
            : juce::String();
        return frameData;
    }

    const auto inputGain = juce::Decibels::decibelsToGain (parameters.inputGainDb);
    const auto outputGain = juce::Decibels::decibelsToGain (parameters.outputGainDb);

    buffer.applyGain (inputGain);

    const auto detection = detector.processBlock (buffer.getReadPointer (0), buffer.getNumSamples());
    auto targetRatio = calculateTargetRatio (detection, parameters, graphicalMode, playheadSeconds, frameData);

    const auto pitchOffsetRatio = std::pow (2.0f, parameters.pitchOffset / 12.0f);
    targetRatio *= pitchOffsetRatio;

    const auto ratio = smoothRatio (targetRatio,
                                    parameters.retuneSpeed,
                                    parameters.humanize,
                                    frameData.note.isNotEmpty()
                                        ? static_cast<int> (std::round (ScaleQuantizer::hzToMidiNote (frameData.correctedHz)))
                                        : -1,
                                    buffer.getNumSamples());

    const auto channelsToProcess = juce::jmin (buffer.getNumChannels(), static_cast<int> (pitchShifters.size()));

    for (int channel = 0; channel < channelsToProcess; ++channel)
        pitchShifters[static_cast<size_t> (channel)].processBlock (buffer.getWritePointer (channel),
                                                                   buffer.getNumSamples(),
                                                                   ratio);

    formantShifter.processBlock (buffer, parameters.formantShift, parameters.throatLength);
    buffer.applyGain (outputGain);

    if (detection.detectedHz > 0.0f)
        frameData.correctedHz = detection.detectedHz * ratio;

    frameData.detectedHz = detection.detectedHz;
    frameData.confidence = detection.confidence;
    return frameData;
}

float AutotuneEngine::calculateTargetRatio (const PitchDetectionResult& detection,
                                            const AutotuneParameters& parameters,
                                            const GraphicalMode& graphicalMode,
                                            double playheadSeconds,
                                            AutotuneFrameData& frameData) noexcept
{
    frameData.detectedHz = detection.detectedHz;
    frameData.correctedHz = detection.detectedHz;
    frameData.confidence = detection.confidence;

    if (! detection.voiced || detection.detectedHz <= 0.0f || detection.confidence < 0.15f)
        return 1.0f;

    float targetHz = 0.0f;
    int targetMidi = -1;
    juce::String noteName;

    if (parameters.mode == 1)
    {
        targetHz = graphicalMode.getInterpolatedHz (playheadSeconds);

        if (targetHz > 0.0f)
        {
            targetMidi = static_cast<int> (std::round (ScaleQuantizer::hzToMidiNote (targetHz)));
            noteName = ScaleQuantizer::midiNoteName (targetMidi);
        }
    }

    if (targetHz <= 0.0f && parameters.midiControl)
    {
        const auto activeMidi = midiController.getActiveNote();
        targetMidi = activeMidi >= 0 ? activeMidi : parameters.targetNoteMidi;

        if (targetMidi >= 0)
        {
            targetHz = ScaleQuantizer::midiNoteToHz (static_cast<float> (targetMidi));
            noteName = ScaleQuantizer::midiNoteName (targetMidi);
        }
    }

    if (targetHz <= 0.0f)
    {
        const auto quantized = quantizer.quantize (detection.detectedHz, parameters.key, parameters.scale);
        targetHz = quantized.targetHz;
        targetMidi = quantized.targetMidiNote;
        noteName = quantized.noteName;
    }

    if (targetHz <= 0.0f)
        return 1.0f;

    const auto fullCorrectionRatio = targetHz / detection.detectedHz;
    const auto flexDepth = juce::jlimit (0.0f, 1.0f, 1.0f - parameters.flexTune / 100.0f);
    const auto vibratoPreserve = 1.0f - 0.25f * juce::jlimit (0.0f, 1.0f, parameters.naturalVibrato / 100.0f);
    const auto correctionDepth = flexDepth * vibratoPreserve;
    const auto ratio = std::pow (fullCorrectionRatio, correctionDepth);

    frameData.correctedHz = detection.detectedHz * ratio;
    frameData.note = noteName;
    juce::ignoreUnused (targetMidi);
    return ratio;
}

float AutotuneEngine::smoothRatio (float targetRatio,
                                   float retuneSpeed,
                                   float humanize,
                                   int targetMidiNote,
                                   int numSamples) noexcept
{
    targetRatio = juce::jlimit (0.5f, 2.0f, targetRatio);

    if (targetMidiNote != lastTargetMidiNote)
    {
        lastTargetMidiNote = targetMidiNote;
        currentNoteSeconds = 0.0;
    }

    currentNoteSeconds += static_cast<double> (numSamples) / sampleRate;

    const auto humanizeAmount = juce::jlimit (0.0f, 1.0f, humanize / 100.0f);
    const auto sustainedNoteFactor = static_cast<float> (juce::jlimit (0.0, 1.0, currentNoteSeconds * 2.0));
    const auto effectiveRetune = juce::jlimit (0.0f, 100.0f, retuneSpeed + humanizeAmount * sustainedNoteFactor * 40.0f);

    if (effectiveRetune <= 0.01f)
    {
        smoothedRatio = targetRatio;
        return smoothedRatio;
    }

    const auto timeSeconds = 0.002f + std::pow (effectiveRetune / 100.0f, 2.0f) * 0.75f;
    const auto coefficient = std::exp (-static_cast<float> (numSamples) / (static_cast<float> (sampleRate) * timeSeconds));
    smoothedRatio = smoothedRatio * coefficient + targetRatio * (1.0f - coefficient);
    return smoothedRatio;
}

