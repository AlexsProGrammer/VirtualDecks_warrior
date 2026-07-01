#include "MidiMappings.h"

namespace MidiMappings
{
    static constexpr const char* kRootTag = "MidiMappings";
    static constexpr const char* kEntryTag = "Entry";
    static constexpr const char* kDeviceAttr = "device";
    static constexpr const char* kChannelAttr = "channel";
    static constexpr const char* kTypeAttr = "type";
    static constexpr const char* kNumberAttr = "number";
    static constexpr const char* kActionAttr = "action";

    juce::File getMappingsFile()
    {
        return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile(".otodecks/MidiMappings.xml");
    }

    juce::String actionToString(Midi::MidiActionTarget action)
    {
        switch (action)
        {
            case Midi::MidiActionTarget::None: return "None";
            case Midi::MidiActionTarget::Deck1_Play: return "Deck1_Play";
            case Midi::MidiActionTarget::Deck2_Play: return "Deck2_Play";
            case Midi::MidiActionTarget::Deck1_CueMon: return "Deck1_CueMon";
            case Midi::MidiActionTarget::Deck2_CueMon: return "Deck2_CueMon";
            case Midi::MidiActionTarget::Deck1_HotCueSet_1: return "Deck1_HotCueSet_1";
            case Midi::MidiActionTarget::Deck1_HotCueSet_2: return "Deck1_HotCueSet_2";
            case Midi::MidiActionTarget::Deck1_HotCueSet_3: return "Deck1_HotCueSet_3";
            case Midi::MidiActionTarget::Deck1_HotCueSet_4: return "Deck1_HotCueSet_4";
            case Midi::MidiActionTarget::Deck1_HotCueSet_5: return "Deck1_HotCueSet_5";
            case Midi::MidiActionTarget::Deck1_HotCueSet_6: return "Deck1_HotCueSet_6";
            case Midi::MidiActionTarget::Deck1_HotCueJump_1: return "Deck1_HotCueJump_1";
            case Midi::MidiActionTarget::Deck1_HotCueJump_2: return "Deck1_HotCueJump_2";
            case Midi::MidiActionTarget::Deck1_HotCueJump_3: return "Deck1_HotCueJump_3";
            case Midi::MidiActionTarget::Deck1_HotCueJump_4: return "Deck1_HotCueJump_4";
            case Midi::MidiActionTarget::Deck1_HotCueJump_5: return "Deck1_HotCueJump_5";
            case Midi::MidiActionTarget::Deck1_HotCueJump_6: return "Deck1_HotCueJump_6";
            case Midi::MidiActionTarget::Deck2_HotCueSet_1: return "Deck2_HotCueSet_1";
            case Midi::MidiActionTarget::Deck2_HotCueSet_2: return "Deck2_HotCueSet_2";
            case Midi::MidiActionTarget::Deck2_HotCueSet_3: return "Deck2_HotCueSet_3";
            case Midi::MidiActionTarget::Deck2_HotCueSet_4: return "Deck2_HotCueSet_4";
            case Midi::MidiActionTarget::Deck2_HotCueSet_5: return "Deck2_HotCueSet_5";
            case Midi::MidiActionTarget::Deck2_HotCueSet_6: return "Deck2_HotCueSet_6";
            case Midi::MidiActionTarget::Deck2_HotCueJump_1: return "Deck2_HotCueJump_1";
            case Midi::MidiActionTarget::Deck2_HotCueJump_2: return "Deck2_HotCueJump_2";
            case Midi::MidiActionTarget::Deck2_HotCueJump_3: return "Deck2_HotCueJump_3";
            case Midi::MidiActionTarget::Deck2_HotCueJump_4: return "Deck2_HotCueJump_4";
            case Midi::MidiActionTarget::Deck2_HotCueJump_5: return "Deck2_HotCueJump_5";
            case Midi::MidiActionTarget::Deck2_HotCueJump_6: return "Deck2_HotCueJump_6";
            case Midi::MidiActionTarget::Deck1_Volume: return "Deck1_Volume";
            case Midi::MidiActionTarget::Deck2_Volume: return "Deck2_Volume";
            case Midi::MidiActionTarget::Deck1_Filter: return "Deck1_Filter";
            case Midi::MidiActionTarget::Deck2_Filter: return "Deck2_Filter";
            case Midi::MidiActionTarget::Deck1_SpeedRel: return "Deck1_SpeedRel";
            case Midi::MidiActionTarget::Deck2_SpeedRel: return "Deck2_SpeedRel";
            case Midi::MidiActionTarget::Crossfader: return "Crossfader";
            case Midi::MidiActionTarget::Deck1_EqLow: return "Deck1_EqLow";
            case Midi::MidiActionTarget::Deck2_EqLow: return "Deck2_EqLow";
            case Midi::MidiActionTarget::Deck1_EqMid: return "Deck1_EqMid";
            case Midi::MidiActionTarget::Deck2_EqMid: return "Deck2_EqMid";
            case Midi::MidiActionTarget::Deck1_EqHigh: return "Deck1_EqHigh";
            case Midi::MidiActionTarget::Deck2_EqHigh: return "Deck2_EqHigh";
            case Midi::MidiActionTarget::Deck1_JogWheel: return "Deck1_JogWheel";
            case Midi::MidiActionTarget::Deck2_JogWheel: return "Deck2_JogWheel";
            case Midi::MidiActionTarget::Deck1_BeatFxOn: return "Deck1_BeatFxOn";
            case Midi::MidiActionTarget::Deck2_BeatFxOn: return "Deck2_BeatFxOn";
            case Midi::MidiActionTarget::Deck1_BeatFxWet: return "Deck1_BeatFxWet";
            case Midi::MidiActionTarget::Deck2_BeatFxWet: return "Deck2_BeatFxWet";
            case Midi::MidiActionTarget::Deck1_LoopIn: return "Deck1_LoopIn";
            case Midi::MidiActionTarget::Deck2_LoopIn: return "Deck2_LoopIn";
            case Midi::MidiActionTarget::Deck1_LoopOut: return "Deck1_LoopOut";
            case Midi::MidiActionTarget::Deck2_LoopOut: return "Deck2_LoopOut";
            case Midi::MidiActionTarget::Deck1_Reloop: return "Deck1_Reloop";
            case Midi::MidiActionTarget::Deck2_Reloop: return "Deck2_Reloop";
            case Midi::MidiActionTarget::Deck1_Sync: return "Deck1_Sync";
            case Midi::MidiActionTarget::Deck2_Sync: return "Deck2_Sync";
            case Midi::MidiActionTarget::Deck1_Master: return "Deck1_Master";
            case Midi::MidiActionTarget::Deck2_Master: return "Deck2_Master";
        }
        return "None";
    }

    Midi::MidiActionTarget stringToAction(const juce::String& actionString)
    {
        if (actionString == "Deck1_Play") return Midi::MidiActionTarget::Deck1_Play;
        if (actionString == "Deck2_Play") return Midi::MidiActionTarget::Deck2_Play;
        if (actionString == "Deck1_CueMon") return Midi::MidiActionTarget::Deck1_CueMon;
        if (actionString == "Deck2_CueMon") return Midi::MidiActionTarget::Deck2_CueMon;
        if (actionString == "Deck1_HotCueSet_1") return Midi::MidiActionTarget::Deck1_HotCueSet_1;
        if (actionString == "Deck1_HotCueSet_2") return Midi::MidiActionTarget::Deck1_HotCueSet_2;
        if (actionString == "Deck1_HotCueSet_3") return Midi::MidiActionTarget::Deck1_HotCueSet_3;
        if (actionString == "Deck1_HotCueSet_4") return Midi::MidiActionTarget::Deck1_HotCueSet_4;
        if (actionString == "Deck1_HotCueSet_5") return Midi::MidiActionTarget::Deck1_HotCueSet_5;
        if (actionString == "Deck1_HotCueSet_6") return Midi::MidiActionTarget::Deck1_HotCueSet_6;
        if (actionString == "Deck1_HotCueJump_1") return Midi::MidiActionTarget::Deck1_HotCueJump_1;
        if (actionString == "Deck1_HotCueJump_2") return Midi::MidiActionTarget::Deck1_HotCueJump_2;
        if (actionString == "Deck1_HotCueJump_3") return Midi::MidiActionTarget::Deck1_HotCueJump_3;
        if (actionString == "Deck1_HotCueJump_4") return Midi::MidiActionTarget::Deck1_HotCueJump_4;
        if (actionString == "Deck1_HotCueJump_5") return Midi::MidiActionTarget::Deck1_HotCueJump_5;
        if (actionString == "Deck1_HotCueJump_6") return Midi::MidiActionTarget::Deck1_HotCueJump_6;
        if (actionString == "Deck2_HotCueSet_1") return Midi::MidiActionTarget::Deck2_HotCueSet_1;
        if (actionString == "Deck2_HotCueSet_2") return Midi::MidiActionTarget::Deck2_HotCueSet_2;
        if (actionString == "Deck2_HotCueSet_3") return Midi::MidiActionTarget::Deck2_HotCueSet_3;
        if (actionString == "Deck2_HotCueSet_4") return Midi::MidiActionTarget::Deck2_HotCueSet_4;
        if (actionString == "Deck2_HotCueSet_5") return Midi::MidiActionTarget::Deck2_HotCueSet_5;
        if (actionString == "Deck2_HotCueSet_6") return Midi::MidiActionTarget::Deck2_HotCueSet_6;
        if (actionString == "Deck2_HotCueJump_1") return Midi::MidiActionTarget::Deck2_HotCueJump_1;
        if (actionString == "Deck2_HotCueJump_2") return Midi::MidiActionTarget::Deck2_HotCueJump_2;
        if (actionString == "Deck2_HotCueJump_3") return Midi::MidiActionTarget::Deck2_HotCueJump_3;
        if (actionString == "Deck2_HotCueJump_4") return Midi::MidiActionTarget::Deck2_HotCueJump_4;
        if (actionString == "Deck2_HotCueJump_5") return Midi::MidiActionTarget::Deck2_HotCueJump_5;
        if (actionString == "Deck2_HotCueJump_6") return Midi::MidiActionTarget::Deck2_HotCueJump_6;
        if (actionString == "Deck1_Volume") return Midi::MidiActionTarget::Deck1_Volume;
        if (actionString == "Deck2_Volume") return Midi::MidiActionTarget::Deck2_Volume;
        if (actionString == "Deck1_Filter") return Midi::MidiActionTarget::Deck1_Filter;
        if (actionString == "Deck2_Filter") return Midi::MidiActionTarget::Deck2_Filter;
        if (actionString == "Deck1_SpeedRel") return Midi::MidiActionTarget::Deck1_SpeedRel;
        if (actionString == "Deck2_SpeedRel") return Midi::MidiActionTarget::Deck2_SpeedRel;
        if (actionString == "Crossfader") return Midi::MidiActionTarget::Crossfader;
        if (actionString == "Deck1_EqLow") return Midi::MidiActionTarget::Deck1_EqLow;
        if (actionString == "Deck2_EqLow") return Midi::MidiActionTarget::Deck2_EqLow;
        if (actionString == "Deck1_EqMid") return Midi::MidiActionTarget::Deck1_EqMid;
        if (actionString == "Deck2_EqMid") return Midi::MidiActionTarget::Deck2_EqMid;
        if (actionString == "Deck1_EqHigh") return Midi::MidiActionTarget::Deck1_EqHigh;
        if (actionString == "Deck2_EqHigh") return Midi::MidiActionTarget::Deck2_EqHigh;
        if (actionString == "Deck1_JogWheel") return Midi::MidiActionTarget::Deck1_JogWheel;
        if (actionString == "Deck2_JogWheel") return Midi::MidiActionTarget::Deck2_JogWheel;
        if (actionString == "Deck1_BeatFxOn") return Midi::MidiActionTarget::Deck1_BeatFxOn;
        if (actionString == "Deck2_BeatFxOn") return Midi::MidiActionTarget::Deck2_BeatFxOn;
        if (actionString == "Deck1_BeatFxWet") return Midi::MidiActionTarget::Deck1_BeatFxWet;
        if (actionString == "Deck2_BeatFxWet") return Midi::MidiActionTarget::Deck2_BeatFxWet;
        if (actionString == "Deck1_LoopIn") return Midi::MidiActionTarget::Deck1_LoopIn;
        if (actionString == "Deck2_LoopIn") return Midi::MidiActionTarget::Deck2_LoopIn;
        if (actionString == "Deck1_LoopOut") return Midi::MidiActionTarget::Deck1_LoopOut;
        if (actionString == "Deck2_LoopOut") return Midi::MidiActionTarget::Deck2_LoopOut;
        if (actionString == "Deck1_Reloop") return Midi::MidiActionTarget::Deck1_Reloop;
        if (actionString == "Deck2_Reloop") return Midi::MidiActionTarget::Deck2_Reloop;
        if (actionString == "Deck1_Sync") return Midi::MidiActionTarget::Deck1_Sync;
        if (actionString == "Deck2_Sync") return Midi::MidiActionTarget::Deck2_Sync;
        if (actionString == "Deck1_Master") return Midi::MidiActionTarget::Deck1_Master;
        if (actionString == "Deck2_Master") return Midi::MidiActionTarget::Deck2_Master;
        return Midi::MidiActionTarget::None;
    }

    bool saveMappings(const std::vector<Midi::MidiMappingEntry>& entries,
                      const juce::String& deviceId)
    {
        return saveMappings(entries, deviceId, getMappingsFile());
    }

    bool saveMappings(const std::vector<Midi::MidiMappingEntry>& entries,
                      const juce::String& deviceId,
                      const juce::File& targetFile)
    {
        targetFile.getParentDirectory().createDirectory();
        auto root = std::make_unique<juce::XmlElement>(kRootTag);
        root->setAttribute(kDeviceAttr, deviceId);

        for (const auto& entry : entries)
        {
            auto* child = new juce::XmlElement(kEntryTag);
            child->setAttribute(kChannelAttr, entry.channel);
            child->setAttribute(kTypeAttr, entry.messageType);
            child->setAttribute(kNumberAttr, entry.number);
            child->setAttribute(kActionAttr, actionToString(entry.target));
            root->addChildElement(child);
        }

        return root->writeTo(targetFile, juce::XmlElement::TextFormat{});
    }

    static     std::vector<Midi::MidiMappingEntry> getDefaultMappings()
    {
        std::vector<Midi::MidiMappingEntry> defaults;
        for (int i = 0; i < 6; ++i)
            defaults.push_back({ 0, 0, 36 + i, static_cast<Midi::MidiActionTarget>(static_cast<int>(Midi::MidiActionTarget::Deck1_HotCueSet_1) + i) });
        for (int i = 0; i < 6; ++i)
            defaults.push_back({ 0, 0, 48 + i, static_cast<Midi::MidiActionTarget>(static_cast<int>(Midi::MidiActionTarget::Deck2_HotCueSet_1) + i) });
        defaults.push_back({ 0, 1, 7, Midi::MidiActionTarget::Deck1_Volume });
        defaults.push_back({ 0, 1, 11, Midi::MidiActionTarget::Deck2_Volume });
        defaults.push_back({ 0, 1, 1, Midi::MidiActionTarget::Crossfader });
        return defaults;
    }

    std::vector<Midi::MidiMappingEntry> loadMappings(juce::String& outDeviceId)
    {
        auto file = getMappingsFile();
        if (!file.existsAsFile())
            return getDefaultMappings();

        return loadMappingsFromFile(file, outDeviceId);
    }

    std::vector<Midi::MidiMappingEntry> loadMappingsFromFile(const juce::File& file,
                                                             juce::String& outDeviceId)
    {
        std::vector<Midi::MidiMappingEntry> result;
        outDeviceId.clear();

        if (!file.existsAsFile())
            return result;

        auto root = juce::XmlDocument::parse(file);
        if (root == nullptr || !root->hasTagName(kRootTag))
            return result;

        outDeviceId = root->getStringAttribute(kDeviceAttr, {});

        for (auto* child = root->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            if (!child->hasTagName(kEntryTag))
                continue;

            Midi::MidiMappingEntry entry;
            entry.channel = static_cast<int>(child->getDoubleAttribute(kChannelAttr, 0.0));
            entry.messageType = static_cast<int>(child->getDoubleAttribute(kTypeAttr, 0.0));
            entry.number = static_cast<int>(child->getDoubleAttribute(kNumberAttr, 0.0));
            entry.target = stringToAction(child->getStringAttribute(kActionAttr, "None"));
            result.push_back(entry);
        }

        return result;
    }

    bool exportMappings(const std::vector<Midi::MidiMappingEntry>& entries,
                        const juce::String& deviceId,
                        const juce::File& targetFile)
    {
        return saveMappings(entries, deviceId, targetFile);
    }

} // namespace MidiMappings
