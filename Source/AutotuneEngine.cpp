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
    alignedDryBuffer.assign (scratchSize, 0.0f);
    dryDelaySamples = pitchShifter.getLatencySamples();
    dryDelayBuffer.assign (static_cast<size_t> (dryDelaySamples + maximumBlockSize + 8), 0.0f);

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
    inputLevelEnvelope = 0.0f;
    previousOutputSample = 0.0f;
    std::fill (dryDelayBuffer.begin(), dryDelayBuffer.end(), 0.0f);
    dryDelayWriteIndex = 0;
    lastTargetMidiNote = -1;
    activeTargetMidiNote = -1;
    candidateTargetMidiNote = -1;
    candidateTargetSamples = 0;
    attackProtectionSamplesRemaining = 0;
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

    double sumSquares = 0.0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mono = 0.0f;

        for (int channel = 0; channel < numInputChannels; ++channel)
            mono += buffer.getSample (channel, sample);

        monoBuffer[static_cast<size_t> (sample)] = mono / static_cast<float> (numInputChannels);
        sumSquares += static_cast<double> (monoBuffer[static_cast<size_t> (sample)])
            * monoBuffer[static_cast<size_t> (sample)];
    }

    updateOnsetState (static_cast<float> (std::sqrt (sumSquares / juce::jmax (1, numSamples))), numSamples);

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

    sumSquares = 0.0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mono = 0.0f;

        for (int channel = 0; channel < numInputChannels; ++channel)
            mono += buffer.getSample (channel, sample);

        monoBuffer[static_cast<size_t> (sample)] = mono / static_cast<float> (numInputChannels);
        correctedMonoBuffer[static_cast<size_t> (sample)] = monoBuffer[static_cast<size_t> (sample)];
        alignedDryBuffer[static_cast<size_t> (sample)] = processAlignedDrySample (monoBuffer[static_cast<size_t> (sample)]);
        sumSquares += static_cast<double> (monoBuffer[static_cast<size_t> (sample)])
            * monoBuffer[static_cast<size_t> (sample)];
    }

    updateOnsetState (static_cast<float> (std::sqrt (sumSquares / juce::jmax (1, numSamples))), numSamples);

    const auto detection = stabiliseDetection (detector.processBlock (monoBuffer.data(), numSamples), numSamples);
    auto targetRatio = calculateTargetRatio (detection, parameters, graphicalMode, playheadSeconds, numSamples, frameData);

    const auto pitchOffsetRatio = std::pow (2.0f, parameters.pitchOffset / 12.0f);
    targetRatio *= pitchOffsetRatio;

    const auto ratio = smoothRatio (targetRatio,
                                    parameters.retuneSpeed,
                                    parameters.humanize,
                                    frameData.note.isNotEmpty()
                                        ? static_cast<int> (std::round (ScaleQuantizer::hzToMidiNote (frameData.correctedHz)))
                                        : -1,
                                    numSamples);

    const auto ratioCents = 1200.0f * std::log2 (juce::jmax (0.0001f, ratio));
    const auto safeRatio = std::pow (2.0f, juce::jlimit (-120.0f, 120.0f, ratioCents) / 1200.0f);

    // Safety release: keep analysis/UI active but do not run the destructive temporary shifter.
    // The production shifter needs a full epoch-based TD-PSOLA implementation before touching audio.
    buffer.applyGain (outputGain);

    attackProtectionSamplesRemaining = juce::jmax (0, attackProtectionSamplesRemaining - numSamples);

    if (detection.detectedHz > 0.0f)
        frameData.correctedHz = detection.detectedHz * safeRatio;

    frameData.detectedHz = detection.detectedHz;
    frameData.confidence = detection.confidence;
    return frameData;
}

void AutotuneEngine::updateOnsetState (float blockRms, int numSamples) noexcept
{
    const auto attackCoefficient = 1.0f - std::exp (-static_cast<float> (numSamples)
                                                    / (static_cast<float> (sampleRate) * 0.006f));
    const auto releaseCoefficient = 1.0f - std::exp (-static_cast<float> (numSamples)
                                                     / (static_cast<float> (sampleRate) * 0.08f));
    const auto coefficient = blockRms > inputLevelEnvelope ? attackCoefficient : releaseCoefficient;
    const auto previousEnvelope = inputLevelEnvelope;
    inputLevelEnvelope += (blockRms - inputLevelEnvelope) * coefficient;

    const auto onsetThreshold = previousEnvelope * 1.75f + 0.018f;

    if (blockRms > onsetThreshold)
        attackProtectionSamplesRemaining = static_cast<int> (sampleRate * 0.09);
}

int AutotuneEngine::stabiliseTargetMidiNote (int proposedMidiNote, int numSamples) noexcept
{
    if (proposedMidiNote < 0)
        return proposedMidiNote;

    if (activeTargetMidiNote < 0)
    {
        activeTargetMidiNote = proposedMidiNote;
        candidateTargetMidiNote = proposedMidiNote;
        candidateTargetSamples = 0;
        return activeTargetMidiNote;
    }

    if (proposedMidiNote == activeTargetMidiNote)
    {
        candidateTargetMidiNote = proposedMidiNote;
        candidateTargetSamples = 0;
        return activeTargetMidiNote;
    }

    if (proposedMidiNote != candidateTargetMidiNote)
    {
        candidateTargetMidiNote = proposedMidiNote;
        candidateTargetSamples = 0;
        return activeTargetMidiNote;
    }

    candidateTargetSamples += numSamples;

    if (candidateTargetSamples >= static_cast<int> (sampleRate * 0.075))
    {
        activeTargetMidiNote = candidateTargetMidiNote;
        candidateTargetSamples = 0;
        attackProtectionSamplesRemaining = juce::jmax (attackProtectionSamplesRemaining,
                                                       static_cast<int> (sampleRate * 0.07));
    }

    return activeTargetMidiNote;
}

float AutotuneEngine::processAlignedDrySample (float sample) noexcept
{
    if (dryDelayBuffer.empty())
        return sample;

    const auto readIndex = (dryDelayWriteIndex + static_cast<int> (dryDelayBuffer.size()) - dryDelaySamples)
        % static_cast<int> (dryDelayBuffer.size());
    const auto delayed = dryDelayBuffer[static_cast<size_t> (readIndex)];
    dryDelayBuffer[static_cast<size_t> (dryDelayWriteIndex)] = sample;
    dryDelayWriteIndex = (dryDelayWriteIndex + 1) % static_cast<int> (dryDelayBuffer.size());
    return delayed;
}

float AutotuneEngine::deClickAndLimit (float sample) noexcept
{
    const auto maxStep = 0.10f;
    const auto delta = juce::jlimit (-maxStep, maxStep, sample - previousOutputSample);
    const auto deClicked = previousOutputSample + delta;
    const auto limited = softLimit (deClicked);
    previousOutputSample = limited;
    return limited;
}

float AutotuneEngine::softLimit (float sample) noexcept
{
    const auto threshold = 0.78f;
    const auto absSample = std::abs (sample);

    if (absSample <= threshold)
        return sample;

    const auto sign = sample >= 0.0f ? 1.0f : -1.0f;
    const auto over = absSample - threshold;
    return sign * (threshold + (1.0f - threshold) * (1.0f - std::exp (-over / (1.0f - threshold))));
}

PitchDetectionResult AutotuneEngine::stabiliseDetection (PitchDetectionResult detection, int numSamples) noexcept
{
    const auto confidenceAttack = 1.0f - std::exp (-static_cast<float> (numSamples)
                                                   / (static_cast<float> (sampleRate) * 0.015f));
    const auto confidenceRelease = 1.0f - std::exp (-static_cast<float> (numSamples)
                                                    / (static_cast<float> (sampleRate) * 0.25f));
    const auto pitchSmoothing = 1.0f - std::exp (-static_cast<float> (numSamples)
                                                 / (static_cast<float> (sampleRate) * 0.025f));
    auto detectedHz = detection.detectedHz;

    if (smoothedDetectedHz > 0.0f && detectedHz > 0.0f)
    {
        while (detectedHz / smoothedDetectedHz > 1.45f)
            detectedHz *= 0.5f;

        while (detectedHz / smoothedDetectedHz < 0.69f)
            detectedHz *= 2.0f;
    }

    auto isReliable = detection.voiced
        && detection.detectedHz >= 50.0f
        && detection.detectedHz <= 1200.0f
        && detection.confidence >= 0.45f;

    if (isReliable && smoothedDetectedHz > 0.0f && detectedHz > 0.0f)
    {
        const auto jumpCents = 1200.0f * std::log2 (juce::jmax (0.0001f, detectedHz / smoothedDetectedHz));

        if (std::abs (jumpCents) > 220.0f)
            isReliable = false;
    }

    if (isReliable)
    {
        if (smoothedDetectedHz <= 0.0f)
            smoothedDetectedHz = detectedHz;
        else
            smoothedDetectedHz += (detectedHz - smoothedDetectedHz) * pitchSmoothing;

        smoothedConfidence += (detection.confidence - smoothedConfidence) * confidenceAttack;
        pitchHoldSamplesRemaining = static_cast<int> (sampleRate * 0.12);
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
        && stable.confidence >= 0.18f
        && (isReliable || pitchHoldSamplesRemaining > 0);

    if (! stable.voiced && pitchHoldSamplesRemaining <= 0)
        stable.detectedHz = 0.0f;

    return stable;
}

float AutotuneEngine::calculateTargetRatio (const PitchDetectionResult& detection,
                                            const AutotuneParameters& parameters,
                                            const GraphicalMode& graphicalMode,
                                            double playheadSeconds,
                                            int numSamples,
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

    if (targetMidi >= 0)
    {
        targetMidi = stabiliseTargetMidiNote (targetMidi, numSamples);
        targetHz = ScaleQuantizer::midiNoteToHz (static_cast<float> (targetMidi));
        noteName = ScaleQuantizer::midiNoteName (targetMidi);
    }

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
    const auto targetCents = 1200.0f * std::log2 (juce::jmax (0.0001f, targetRatio));
    targetRatio = std::pow (2.0f, juce::jlimit (-120.0f, 120.0f, targetCents) / 1200.0f);

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

