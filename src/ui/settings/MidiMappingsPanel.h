#pragma once

#include <JuceHeader.h>
#include "../../core/midi/MidiMapper.h"
#include "../../core/data/MidiMappings.h"
#include "../../core/data/AppSettings.h"
#include "CustomLookAndFeel.h"

class MidiMappingsPanel : public juce::Component,
                         private juce::Button::Listener,
                         private juce::ComboBox::Listener,
                         private juce::Timer
{
public:
    explicit MidiMappingsPanel(Midi::MidiMapper& mapper);
    ~MidiMappingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setOnMappingsChanged(std::function<void()> cb) { onMappingsChanged = std::move(cb); }

private:
    struct EditableTextEditor : public juce::TextEditor
    {
        explicit EditableTextEditor(MidiMappingsPanel& owner_) : owner(owner_) {}
        void focusLost(juce::Component::FocusChangeType cause) override
        {
            juce::TextEditor::focusLost(cause);
            commitValue();
        }

        void commitValue()
        {
            if (row < 0 || row >= (int)owner.mappings.size())
                return;

            const int value = getText().getIntValue();
            if (columnId == 1)
                owner.mappings[row].channel = juce::jlimit(0, 16, value);
            else if (columnId == 3)
                owner.mappings[row].number = juce::jlimit(0, 127, value);

            owner.saveMappings();
            owner.refreshMappingsTable();
        }

        MidiMappingsPanel& owner;
        int row = -1;
        int columnId = -1;
    };

    struct TypeComboBox : public juce::ComboBox
    {
        explicit TypeComboBox(MidiMappingsPanel& owner_) : owner(owner_) {}
        void setRow(int newRow) { row = newRow; }
        MidiMappingsPanel& owner;
        int row = -1;
    };

    struct ActionComboBox : public juce::ComboBox
    {
        explicit ActionComboBox(MidiMappingsPanel& owner_) : owner(owner_) {}
        void setRow(int newRow) { row = newRow; }
        MidiMappingsPanel& owner;
        int row = -1;
    };

    struct MappingTableModel : public juce::TableListBoxModel
    {
        explicit MappingTableModel(MidiMappingsPanel& owner_) : owner(owner_) {}

        int getNumRows() override;
        void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
        void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
        juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponent) override;

        MidiMappingsPanel& owner;
    };

    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    void refreshDeviceList();
    void refreshMappingsTable();
    void timerCallback() override;
    void saveMappings();
    void loadSavedMappings();
    void updateStatusLabel();
    void setLearnMode(bool enable);
    void handleLearnMessage(int channel, int messageType, int number);
    juce::String getSelectedMidiDeviceId() const;
    juce::String getMappingDeviceId() const;
    void setActionForRow(int row, Midi::MidiActionTarget target);

    static juce::String getDeckLabel(Midi::MidiActionTarget target);
    static juce::String getActionLabel(Midi::MidiActionTarget target);
    static juce::String getTypeLabel(int messageType);
    static juce::String getStatusLabel(const Midi::MidiMappingEntry& entry);
    static std::vector<std::pair<Midi::MidiActionTarget, juce::String>> getActionChoices();

    Midi::MidiMapper& mapper;
    juce::ComboBox deviceSelector;
    juce::TextButton refreshBtn{ "Autodetect" };
    juce::TextButton importBtn{ "Import..." };
    juce::TextButton exportBtn{ "Export..." };
    struct ToolbarDivider : public juce::Component
    {
        void paint(juce::Graphics& g) override
        {
            g.fillAll(UI::borderSubtle);
        }
    };
    ToolbarDivider toolbarDivider;
    ToolbarDivider toolbarDivider2;
    juce::TextButton addRowBtn{ "Add" };
    juce::TextButton clearRowBtn{ "Remove" };
    juce::TextButton learnBtn{ "MIDI Learn" };
    juce::TableListBox mappingTable;
    juce::Label statusLabel{ "status", "Not connected" };
    juce::String overrideStatusMessage;
    bool learnModeActive = false;
    int learnTargetRow = -1;
    std::shared_ptr<juce::FileChooser> fileChooserRef;
    std::function<void()> onMappingsChanged;
    std::vector<Midi::MidiMappingEntry> mappings;
    std::vector<juce::String> deviceIds;
    MappingTableModel tableModel{ *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMappingsPanel)
};
