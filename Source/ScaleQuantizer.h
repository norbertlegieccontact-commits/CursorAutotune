#pragma once

#include <JuceHeader.h>
#include <array>

enum class Key
{
    C = 0, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B, Chromatic
};

enum class Scale
{
    Major = 0,
    Minor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    HarmonicMinor,
    MelodicMinor,
    PentatonicMajor,
    PentatonicMinor,
    Blues,
    Chromatic
};

struct QuantizedPitch
{
    float targetHz = 0.0f;
    int targetMidiNote = -1;
    float centsOffset = 0.0f;
    juce::String noteName;
};

class ScaleQuantizer
{
public:
    QuantizedPitch quantize (float detectedHz, Key key, Scale scale) const noexcept;

    static float midiNoteToHz (float midiNote) noexcept;
    static float hzToMidiNote (float hz) noexcept;
    static juce::String midiNoteName (int midiNote);

private:
    static bool isPitchClassAllowed (int pitchClass, Key key, Scale scale) noexcept;
    static bool scaleContainsInterval (int interval, Scale scale) noexcept;
};

