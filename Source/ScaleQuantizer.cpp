#include "ScaleQuantizer.h"

#include <cmath>

QuantizedPitch ScaleQuantizer::quantize (float detectedHz, Key key, Scale scale) const noexcept
{
    QuantizedPitch result;

    if (detectedHz <= 0.0f || ! std::isfinite (detectedHz))
        return result;

    const auto detectedMidi = hzToMidiNote (detectedHz);
    const auto roundedMidi = static_cast<int> (std::round (detectedMidi));

    int bestMidi = roundedMidi;
    auto bestDistance = std::numeric_limits<float>::max();

    for (int candidate = roundedMidi - 12; candidate <= roundedMidi + 12; ++candidate)
    {
        const auto pitchClass = (candidate % 12 + 12) % 12;

        if (! isPitchClassAllowed (pitchClass, key, scale))
            continue;

        const auto distance = std::abs (static_cast<float> (candidate) - detectedMidi);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestMidi = candidate;
        }
    }

    result.targetMidiNote = bestMidi;
    result.targetHz = midiNoteToHz (static_cast<float> (bestMidi));
    result.centsOffset = (static_cast<float> (bestMidi) - detectedMidi) * 100.0f;
    result.noteName = midiNoteName (bestMidi);
    return result;
}

float ScaleQuantizer::midiNoteToHz (float midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, (midiNote - 69.0f) / 12.0f);
}

float ScaleQuantizer::hzToMidiNote (float hz) noexcept
{
    return hz > 0.0f ? 69.0f + 12.0f * std::log2 (hz / 440.0f) : 0.0f;
}

juce::String ScaleQuantizer::midiNoteName (int midiNote)
{
    static constexpr const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                             "F#", "G", "G#", "A", "A#", "B" };

    if (midiNote < 0)
        return {};

    const auto pitchClass = (midiNote % 12 + 12) % 12;
    const auto octave = midiNote / 12 - 1;
    return juce::String (names[pitchClass]) + juce::String (octave);
}

bool ScaleQuantizer::isPitchClassAllowed (int pitchClass, Key key, Scale scale) noexcept
{
    if (key == Key::Chromatic || scale == Scale::Chromatic)
        return true;

    const auto root = static_cast<int> (key);
    const auto interval = (pitchClass - root + 12) % 12;
    return scaleContainsInterval (interval, scale);
}

bool ScaleQuantizer::scaleContainsInterval (int interval, Scale scale) noexcept
{
    switch (scale)
    {
        case Scale::Major:
        {
            static constexpr std::array<int, 7> notes { 0, 2, 4, 5, 7, 9, 11 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::Minor:
        {
            static constexpr std::array<int, 7> notes { 0, 2, 3, 5, 7, 8, 10 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::Dorian:
        {
            static constexpr std::array<int, 7> notes { 0, 2, 3, 5, 7, 9, 10 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::Phrygian:
        {
            static constexpr std::array<int, 7> notes { 0, 1, 3, 5, 7, 8, 10 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::Lydian:
        {
            static constexpr std::array<int, 7> notes { 0, 2, 4, 6, 7, 9, 11 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::Mixolydian:
        {
            static constexpr std::array<int, 7> notes { 0, 2, 4, 5, 7, 9, 10 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::Locrian:
        {
            static constexpr std::array<int, 7> notes { 0, 1, 3, 5, 6, 8, 10 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::HarmonicMinor:
        {
            static constexpr std::array<int, 7> notes { 0, 2, 3, 5, 7, 8, 11 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::MelodicMinor:
        {
            static constexpr std::array<int, 7> notes { 0, 2, 3, 5, 7, 9, 11 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::PentatonicMajor:
        {
            static constexpr std::array<int, 5> notes { 0, 2, 4, 7, 9 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::PentatonicMinor:
        {
            static constexpr std::array<int, 5> notes { 0, 3, 5, 7, 10 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::Blues:
        {
            static constexpr std::array<int, 6> notes { 0, 3, 5, 6, 7, 10 };
            return std::find (notes.begin(), notes.end(), interval) != notes.end();
        }

        case Scale::Chromatic:
            return true;
    }

    return true;
}

