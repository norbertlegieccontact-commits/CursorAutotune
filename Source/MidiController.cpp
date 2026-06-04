#include "MidiController.h"

void MidiController::reset() noexcept
{
    activeNote = -1;
}

void MidiController::processMidi (const juce::MidiBuffer& midiMessages) noexcept
{
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            activeNote = message.getNoteNumber();
        }
        else if (message.isNoteOff() && message.getNoteNumber() == activeNote)
        {
            activeNote = -1;
        }
    }
}

