#include "AutotuneEngine.h"

#include <cmath>

void AutotuneEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = juce::jmax (1.0, spec.sampleRate);
    maximumBlockSize = static_cast<int> (spec.maximumBlockSize);

    detector.prepare (sampleRate, 2048, 512);
    formantShifter.prepare (spec);
    pitchShifter.prepare (spec);

    const auto scratchSize = static_cast<size_t> (juce::jmax (maximumBlockSize, 8192));
    monoBuffer.assign (scratchSize, 0.0f);
    correctedMonoBuffer.assign (scratchSize, 0.0f);

    reset();
}

void AutotuneEngine::reset() noexcept
{
    detector.reset();
    midiController.reset();
    formantShifter.reset();
    pitchShifter.reset();

    smoothedRatio = 1.0f;
    smoothedDetectedHz = 0.0f;
    smoothedConfidence = 0.0f;
    correctionBlend = 0.0f;
    lastTargetMidiNote = -1;
    pitchHoldSamplesRemaining = 0;
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
    const auto numSamples = buffer.getNumSamples();

    if (numSamples > static_cast<int> (monoBuffer.size()))
    {
        buffer.applyGain (juce::Decibels::decibelsToGain (parameters.inputGainDb + parameters.outputGainDb));
        return frameData;
    }

    const auto numInputChannels = juce::jmax (1, buffer.getNumChannels());

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mono = 0.0f;

        for (int channel = 0; channel < numInputChannels; ++channel)
            mono += buffer.getSample (channel, sample);

        monoBuffer[static_cast<size_t> (sample)] = mono / static_cast<float> (numInputChannels);
    }

    if (parameters.bypass)
    {
        const auto detection = stabiliseDetection (detector.processBlock (monoBuffer.data(), numSamples), numSamples);
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

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mono = 0.0f;

        for (int channel = 0; channel < numInputChannels; ++channel)
            mono += buffer.getSample (channel, sample);

        monoBuffer[static_cast<size_t> (sample)] = mono / static_cast<float> (numInputChannels);
        correctedMonoBuffer[static_cast<size_t> (sample)] = monoBuffer[static_cast<size_t> (sample)];
    }

    const auto detection = stabiliseDetection (detector.processBlock (monoBuffer.data(), numSamples), numSamples);
    auto targetRatio = calculateTargetRatio (detection, parameters, graphicalMode, playheadSeconds, frameData);

    const auto pitchOffsetRatio = std::pow (2.0f, parameters.pitchOffset / 12.0f);
    targetRatio *= pitchOffsetRatio;

    const auto ratio = smoothRatio (targetRatio,
                                    parameters.retuneSpeed,
                                    parameters.humanize,
                                    frameData.note.isNotEmpty()
                                        ? static_cast<int> (std::round (ScaleQuantizer::hzToMidiNote (frameData.correctedHz)))
                                        : -1,
                                    numSamples);

    pitchShifter.processBlock (correctedMonoBuffer.data(), numSamples, ratio);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] = correctedMonoBuffer[static_cast<size_t> (sample)];
    }

    formantShifter.processBlock (buffer, parameters.formantShift, parameters.throatLength);
    buffer.applyGain (outputGain);

    if (detection.detectedHz > 0.0f)
        frameData.correctedHz = detection.detectedHz * ratio;

    frameData.detectedHz = detection.detectedHz;
    frameData.confidence = detection.confidence;
    return frameData;
}

PitchDetectionResult AutotuneEngine::stabiliseDetection (PitchDetectionResult detection, int numSamples) noexcept
{
    const auto confidenceAttack = 1.0f - std::exp (-static_cast<float> (numSamples)
                                                   / (static_cast<float> (sampleRate) * 0.015f));
    const auto confidenceRelease = 1.0f - std::exp (-static_cast<float> (numSamples)
                                                    / (static_cast<float> (sampleRate) * 0.12f));
    const auto pitchSmoothing = 1.0f - std::exp (-static_cast<float> (numSamples)
                                                 / (static_cast<float> (sampleRate) * 0.025f));
    const auto isReliable = detection.voiced
        && detection.detectedHz >= 50.0f
        && detection.detectedHz <= 1200.0f
        && detection.confidence >= 0.20f;

    if (isReliable)
    {
        if (smoothedDetectedHz <= 0.0f)
            smoothedDetectedHz = detection.detectedHz;
        else
            smoothedDetectedHz += (detection.detectedHz - smoothedDetectedHz) * pitchSmoothing;

        smoothedConfidence += (detection.confidence - smoothedConfidence) * confidenceAttack;
        pitchHoldSamplesRemaining = static_cast<int> (sampleRate * 0.08);
    }
    else
    {
        smoothedConfidence += (0.0f - smoothedConfidence) * confidenceRelease;
        pitchHoldSamplesRemaining = juce::jmax (0, pitchHoldSamplesRemaining - numSamples);
    }

    PitchDetectionResult stable;
    stable.detectedHz = smoothedDetectedHz;
    stable.confidence = juce::jlimit (0.0f, 1.0f, smoothedConfidence);
    stable.voiced = smoothedDetectedHz > 0.0f
        && stable.confidence >= 0.12f
        && (isReliable || pitchHoldSamplesRemaining > 0);

    if (! stable.voiced && pitchHoldSamplesRemaining <= 0)
        stable.detectedHz = 0.0f;

    return stable;
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

