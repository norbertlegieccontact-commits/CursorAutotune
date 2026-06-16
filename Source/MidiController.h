#pragma once

#include <JuceHeader.h>

class MidiController
{
public:
    void reset() noexcept;
    void processMidi (const juce::MidiBuffer& midiMessages) noexcept;

    int getActiveNote() const noexcept { return activeNote; }

private:
    int activeNote = -1;
};

