#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Midi {

/**
 * Actions that can be triggered by a MIDI mapping.
 */
enum class MidiActionTarget
{
    None,
    Deck1_Play,
    Deck2_Play,
    Deck1_CueMon,
    Deck2_CueMon,
    Deck1_HotCueSet_1,
    Deck1_HotCueSet_2,
    Deck1_HotCueSet_3,
    Deck1_HotCueSet_4,
    Deck1_HotCueSet_5,
    Deck1_HotCueSet_6,
    Deck1_HotCueJump_1,
    Deck1_HotCueJump_2,
    Deck1_HotCueJump_3,
    Deck1_HotCueJump_4,
    Deck1_HotCueJump_5,
    Deck1_HotCueJump_6,
    Deck2_HotCueSet_1,
    Deck2_HotCueSet_2,
    Deck2_HotCueSet_3,
    Deck2_HotCueSet_4,
    Deck2_HotCueSet_5,
    Deck2_HotCueSet_6,
    Deck2_HotCueJump_1,
    Deck2_HotCueJump_2,
    Deck2_HotCueJump_3,
    Deck2_HotCueJump_4,
    Deck2_HotCueJump_5,
    Deck2_HotCueJump_6,
    Deck1_Volume,
    Deck2_Volume,
    Deck1_Filter,
    Deck2_Filter,
    Deck1_SpeedRel,
    Deck2_SpeedRel,
    Crossfader,
    Deck1_EqLow,
    Deck2_EqLow,
    Deck1_EqMid,
    Deck2_EqMid,
    Deck1_EqHigh,
    Deck2_EqHigh,
    Deck1_JogWheel,
    Deck2_JogWheel,
    Deck1_BeatFxOn,
    Deck2_BeatFxOn,
    Deck1_BeatFxWet,
    Deck2_BeatFxWet,
    Deck1_LoopIn,
    Deck2_LoopIn,
    Deck1_LoopOut,
    Deck2_LoopOut,
    Deck1_Reloop,
    Deck2_Reloop,
    Deck1_Sync,
    Deck2_Sync,
    Deck1_Master,
    Deck2_Master,
    Deck1_VuMeter,
    Deck2_VuMeter
};

/**
 * Single MIDI mapping entry.
 */
struct MidiMappingEntry
{
    int channel = 0;                ///< 0 means any channel
    int messageType = 0;            ///< 0 = Note, 1 = CC
    int number = 0;                 ///< Note number or controller number
    MidiActionTarget target = MidiActionTarget::None;
};

/**
 * Single MIDI output mapping entry for feedback.
 */
struct MidiOutputEntry
{
    MidiActionTarget target = MidiActionTarget::None;
    int channel = 1;                ///< 1-16; use 1 by default
    int messageType = 0;            ///< 0 = Note, 1 = CC
    int number = 0;                 ///< Note number or controller number
    int onValue = 127;              ///< value to send when active
    int offValue = 0;               ///< value to send when inactive
};

/**
 * MIDI input and output manager.
 */
class MidiMapper : public juce::MidiInputCallback
{
public:
    MidiMapper();
    ~MidiMapper() override;

    void setActionCallback(std::function<void(MidiActionTarget, int)> cb);
    std::function<void(MidiActionTarget, int)> getActionCallback() const noexcept;
    void setLearnCallback(std::function<void(int channel, int messageType, int number)> cb);
    void clearLearnCallback() noexcept;
    void setMappings(std::vector<MidiMappingEntry> newMappings);
    const std::vector<MidiMappingEntry>& getMappings() const noexcept;

    bool openDevice(const juce::String& identifier);
    void closeDevice();
    juce::String getActiveDeviceIdentifier() const noexcept;

    bool openOutputDevice(const juce::String& identifier);
    void closeOutputDevice();
    juce::String getActiveOutputDeviceIdentifier() const noexcept;

    void setOutputMappings(std::vector<MidiOutputEntry> newMappings);
    const std::vector<MidiOutputEntry>& getOutputMappings() const noexcept;

    void sendBoolFeedback(MidiActionTarget target, bool active);
    void sendValueFeedback(MidiActionTarget target, int value);

    static bool isButtonAction(MidiActionTarget action);
    static bool isContinuousAction(MidiActionTarget action);

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    std::vector<MidiMappingEntry> mappings;
    std::vector<MidiOutputEntry> outputMappings;
    std::function<void(MidiActionTarget, int)> actionCallback;
    std::function<void(int channel, int messageType, int number)> learnCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMapper)
};

inline bool isButtonAction(MidiActionTarget action)
{
    switch (action) {
        case MidiActionTarget::Deck1_Play:
        case MidiActionTarget::Deck2_Play:
        case MidiActionTarget::Deck1_CueMon:
        case MidiActionTarget::Deck2_CueMon:
        case MidiActionTarget::Deck1_HotCueSet_1:
        case MidiActionTarget::Deck1_HotCueSet_2:
        case MidiActionTarget::Deck1_HotCueSet_3:
        case MidiActionTarget::Deck1_HotCueSet_4:
        case MidiActionTarget::Deck1_HotCueSet_5:
        case MidiActionTarget::Deck1_HotCueSet_6:
        case MidiActionTarget::Deck1_HotCueJump_1:
        case MidiActionTarget::Deck1_HotCueJump_2:
        case MidiActionTarget::Deck1_HotCueJump_3:
        case MidiActionTarget::Deck1_HotCueJump_4:
        case MidiActionTarget::Deck1_HotCueJump_5:
        case MidiActionTarget::Deck1_HotCueJump_6:
        case MidiActionTarget::Deck2_HotCueSet_1:
        case MidiActionTarget::Deck2_HotCueSet_2:
        case MidiActionTarget::Deck2_HotCueSet_3:
        case MidiActionTarget::Deck2_HotCueSet_4:
        case MidiActionTarget::Deck2_HotCueSet_5:
        case MidiActionTarget::Deck2_HotCueSet_6:
        case MidiActionTarget::Deck2_HotCueJump_1:
        case MidiActionTarget::Deck2_HotCueJump_2:
        case MidiActionTarget::Deck2_HotCueJump_3:
        case MidiActionTarget::Deck2_HotCueJump_4:
        case MidiActionTarget::Deck2_HotCueJump_5:
        case MidiActionTarget::Deck2_HotCueJump_6:
        case MidiActionTarget::Deck1_BeatFxOn:
        case MidiActionTarget::Deck2_BeatFxOn:
        case MidiActionTarget::Deck1_LoopIn:
        case MidiActionTarget::Deck2_LoopIn:
        case MidiActionTarget::Deck1_LoopOut:
        case MidiActionTarget::Deck2_LoopOut:
        case MidiActionTarget::Deck1_Reloop:
        case MidiActionTarget::Deck2_Reloop:
        case MidiActionTarget::Deck1_Sync:
        case MidiActionTarget::Deck2_Sync:
        case MidiActionTarget::Deck1_Master:
        case MidiActionTarget::Deck2_Master:
            return true;
        default:
            return false;
    }
}

inline bool isContinuousAction(MidiActionTarget action)
{
    switch (action) {
        case MidiActionTarget::Deck1_Volume:
        case MidiActionTarget::Deck2_Volume:
        case MidiActionTarget::Deck1_Filter:
        case MidiActionTarget::Deck2_Filter:
        case MidiActionTarget::Deck1_SpeedRel:
        case MidiActionTarget::Deck2_SpeedRel:
        case MidiActionTarget::Crossfader:
        case MidiActionTarget::Deck1_EqLow:
        case MidiActionTarget::Deck2_EqLow:
        case MidiActionTarget::Deck1_EqMid:
        case MidiActionTarget::Deck2_EqMid:
        case MidiActionTarget::Deck1_EqHigh:
        case MidiActionTarget::Deck2_EqHigh:
        case MidiActionTarget::Deck1_JogWheel:
        case MidiActionTarget::Deck2_JogWheel:
        case MidiActionTarget::Deck1_BeatFxWet:
        case MidiActionTarget::Deck2_BeatFxWet:
            return true;
        default:
            return false;
    }
}

} // namespace Midi
