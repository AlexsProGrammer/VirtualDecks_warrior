#include "MidiMapper.h"

namespace Midi {

MidiMapper::MidiMapper() = default;
MidiMapper::~MidiMapper() = default;

void MidiMapper::setActionCallback(std::function<void(MidiActionTarget, int)> cb)
{
    actionCallback = std::move(cb);
}

std::function<void(MidiActionTarget, int)> MidiMapper::getActionCallback() const noexcept
{
    return actionCallback;
}

void MidiMapper::setLearnCallback(std::function<void(int channel, int messageType, int number)> cb)
{
    learnCallback = std::move(cb);
}

void MidiMapper::clearLearnCallback() noexcept
{
    learnCallback = nullptr;
}

void MidiMapper::setMappings(std::vector<MidiMappingEntry> newMappings)
{
    mappings = std::move(newMappings);
}

const std::vector<MidiMappingEntry>& MidiMapper::getMappings() const noexcept
{
    return mappings;
}

bool MidiMapper::openDevice(const juce::String& identifier)
{
    closeDevice();
    if (identifier.isEmpty())
        return false;

    auto input = juce::MidiInput::openDevice(identifier, this);
    if (input != nullptr)
    {
        input->start();
        midiInput = std::move(input);
        return true;
    }

    return false;
}

void MidiMapper::closeDevice()
{
    if (midiInput != nullptr)
    {
        midiInput->stop();
        midiInput.reset();
    }
}

juce::String MidiMapper::getActiveDeviceIdentifier() const noexcept
{
    return midiInput != nullptr ? midiInput->getIdentifier() : juce::String();
}

void MidiMapper::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (actionCallback == nullptr)
        return;

    const int channel = message.getChannel();
    const int number = message.isNoteOn() || message.isNoteOff()
        ? message.getNoteNumber()
        : message.isController()
            ? message.getControllerNumber()
            : -1;

    if (number < 0)
        return;

    const int messageType = message.isNoteOn() || message.isNoteOff() ? 0
                          : message.isController() ? 1
                          : -1;

    if (messageType < 0)
        return;

    const int value = messageType == 0 ? message.getVelocity() : message.getControllerValue();

    if (learnCallback)
        learnCallback(channel, messageType, number);

    for (const auto& mapping : mappings)
    {
        if (mapping.target == MidiActionTarget::None)
            continue;

        if (mapping.messageType != messageType)
            continue;

        if (mapping.number != number)
            continue;

        if (mapping.channel != 0 && mapping.channel != channel)
            continue;

        actionCallback(mapping.target, value);
    }
}

} // namespace Midi
