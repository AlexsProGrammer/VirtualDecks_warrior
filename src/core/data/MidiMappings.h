#pragma once

#include <JuceHeader.h>
#include "../midi/MidiMapper.h"

namespace MidiMappings
{
    juce::File getMappingsFile();

    bool saveMappings(const std::vector<Midi::MidiMappingEntry>& entries,
                      const juce::String& deviceId);

    bool saveMappings(const std::vector<Midi::MidiMappingEntry>& entries,
                      const juce::String& deviceId,
                      const juce::File& targetFile);

    std::vector<Midi::MidiMappingEntry> loadMappings(juce::String& outDeviceId);
    std::vector<Midi::MidiMappingEntry> loadMappingsFromFile(const juce::File& file,
                                                             juce::String& outDeviceId);

    bool exportMappings(const std::vector<Midi::MidiMappingEntry>& entries,
                        const juce::String& deviceId,
                        const juce::File& targetFile);

    juce::String actionToString(Midi::MidiActionTarget action);
    Midi::MidiActionTarget stringToAction(const juce::String& actionString);
}
