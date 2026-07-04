#include "MidiMapper.h"

namespace Midi {

MidiMapper::MidiMapper() = default;
MidiMapper::~MidiMapper() = default;

void MidiMapper::setActionCallback(std::function<void(MidiActionTarget, int, bool)> cb)
{
    actionCallback = std::move(cb);
}

std::function<void(MidiActionTarget, int, bool)> MidiMapper::getActionCallback() const noexcept
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

bool MidiMapper::openOutputDevice(const juce::String& identifier)
{
    closeOutputDevice();
    if (identifier.isEmpty())
        return false;

    auto output = juce::MidiOutput::openDevice(identifier);
    if (output != nullptr)
    {
        midiOutput = std::move(output);
        return true;
    }

    return false;
}

void MidiMapper::closeOutputDevice()
{
    midiOutput.reset();
}

juce::String MidiMapper::getActiveOutputDeviceIdentifier() const noexcept
{
    return midiOutput != nullptr ? midiOutput->getIdentifier() : juce::String();
}

void MidiMapper::setOutputMappings(std::vector<MidiOutputEntry> newMappings)
{
    outputMappings = std::move(newMappings);
}

const std::vector<MidiOutputEntry>& MidiMapper::getOutputMappings() const noexcept
{
    return outputMappings;
}

static void sendMidiMessageForEntry(juce::MidiOutput& output, const MidiOutputEntry& entry, int value)
{
    auto message = entry.messageType == 0
        ? juce::MidiMessage::noteOn(entry.channel, entry.number, (juce::uint8) value)
        : juce::MidiMessage::controllerEvent(entry.channel, entry.number, (juce::uint8) value);

    output.sendMessageNow(message);
}

void MidiMapper::sendBoolFeedback(MidiActionTarget target, bool active)
{
    if (midiOutput == nullptr)
        return;

    for (const auto& entry : outputMappings)
    {
        if (entry.target != target)
            continue;

        const int value = active ? entry.onValue : entry.offValue;
        sendMidiMessageForEntry(*midiOutput, entry, value);
    }
}

void MidiMapper::sendValueFeedback(MidiActionTarget target, int value)
{
    if (midiOutput == nullptr)
        return;

    for (const auto& entry : outputMappings)
    {
        if (entry.target != target)
            continue;

        sendMidiMessageForEntry(*midiOutput, entry, value);
    }
}

void MidiMapper::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
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

    // Note-based mappings (buttons/pads) can fire both a "note on" (press) and
    // a "note off" event (release). A real Note Off message is allowed to
    // carry a non-zero release velocity, so `value == 0` is NOT a reliable way
    // to detect a release - use JUCE's own isNoteOn()/isNoteOff() semantics
    // instead (isNoteOn() defaults to returning false for velocity-0 note-ons,
    // so it already treats those as releases too).
    const bool isNotePress = messageType == 0 && message.isNoteOn();
    const bool isNoteRelease = messageType == 0 && !isNotePress;

    if (learnCallback && !isNoteRelease)
    {
        const auto callback = learnCallback;
        juce::MessageManager::callAsync([callback, channel, messageType, number]() {
            callback(channel, messageType, number);
        });
    }

    if (actionCallback == nullptr)
        return;

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

        if (isNoteRelease)
        {
            if (!Midi::isMomentaryAction(mapping.target))
                continue;

            actionCallback(mapping.target, 0, false);
            continue;
        }

        // Defense-in-depth: if a button-type action ever ends up mapped to a
        // CC (e.g. a hand-edited/imported MidiMappings.xml), ignore the
        // "off" value (0) so it still behaves as a single press-to-toggle
        // rather than firing again when the controller reports release.
        if (Midi::isButtonAction(mapping.target) && value == 0)
            continue;

        actionCallback(mapping.target, value, true);
    }
}

} // namespace Midi
