#include "MidiMappingsPanel.h"

MidiMappingsPanel::MidiMappingsPanel(Midi::MidiMapper& mapper_) :
    mapper(mapper_)
{
    addAndMakeVisible(inputModeBtn);
    addAndMakeVisible(outputModeBtn);
    addAndMakeVisible(deviceSelector);
    addAndMakeVisible(refreshBtn);
    addAndMakeVisible(importBtn);
    addAndMakeVisible(exportBtn);
    addAndMakeVisible(toolbarDivider);
    addAndMakeVisible(toolbarDivider2);
    addAndMakeVisible(addRowBtn);
    addAndMakeVisible(clearRowBtn);
    addAndMakeVisible(learnBtn);
    addAndMakeVisible(mappingTable);
    addAndMakeVisible(outputMappingTable);
    addAndMakeVisible(statusLabel);

    mappingTable.setModel(&tableModel);
    outputMappingTable.setModel(&outputTableModel);
    inputModeBtn.setClickingTogglesState(true);
    outputModeBtn.setClickingTogglesState(true);
    inputModeBtn.setToggleState(true, juce::dontSendNotification);
    outputModeBtn.setToggleState(false, juce::dontSendNotification);
    inputModeBtn.setColour(juce::TextButton::buttonOnColourId, UI::accentPositive);
    outputModeBtn.setColour(juce::TextButton::buttonOnColourId, UI::accentPositive);

    mappingTable.getHeader().addColumn("Ch", 1, 40, 40, 40, juce::TableHeaderComponent::defaultFlags);
    mappingTable.getHeader().addColumn("Type", 2, 60, 60, 80, juce::TableHeaderComponent::defaultFlags);
    mappingTable.getHeader().addColumn("Number", 3, 60, 60, 80, juce::TableHeaderComponent::defaultFlags);
    mappingTable.getHeader().addColumn("Deck", 4, 120, 80, 200, juce::TableHeaderComponent::defaultFlags);
    mappingTable.getHeader().addColumn("Action", 5, 120, 80, 250, juce::TableHeaderComponent::defaultFlags);
    mappingTable.getHeader().addColumn("Status", 6, 120, 80, 180, juce::TableHeaderComponent::defaultFlags);

    outputMappingTable.getHeader().addColumn("State", 1, 160, 80, 240, juce::TableHeaderComponent::defaultFlags);
    outputMappingTable.getHeader().addColumn("Ch", 2, 40, 40, 40, juce::TableHeaderComponent::defaultFlags);
    outputMappingTable.getHeader().addColumn("Type", 3, 60, 60, 80, juce::TableHeaderComponent::defaultFlags);
    outputMappingTable.getHeader().addColumn("Number", 4, 60, 60, 80, juce::TableHeaderComponent::defaultFlags);
    outputMappingTable.getHeader().addColumn("On", 5, 50, 50, 70, juce::TableHeaderComponent::defaultFlags);
    outputMappingTable.getHeader().addColumn("Off", 6, 50, 50, 70, juce::TableHeaderComponent::defaultFlags);

    inputModeBtn.addListener(this);
    outputModeBtn.addListener(this);
    deviceSelector.addListener(this);
    refreshBtn.addListener(this);
    importBtn.addListener(this);
    exportBtn.addListener(this);
    addRowBtn.addListener(this);
    clearRowBtn.addListener(this);
    learnBtn.addListener(this);

    toolbarDivider.setInterceptsMouseClicks(false, false);
    toolbarDivider2.setInterceptsMouseClicks(false, false);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::backgroundColourId, UI::bgCard);
    statusLabel.setColour(juce::Label::outlineColourId, UI::borderSubtle);
    statusLabel.setBorderSize(juce::BorderSize<int>(1));
    statusLabel.setOpaque(true);

    loadSavedMappings();
    refreshDeviceList();
    startTimerHz(2);
    updateStatusLabel();
}

MidiMappingsPanel::~MidiMappingsPanel() = default;

void MidiMappingsPanel::paint(juce::Graphics& g)
{
    CustomLookAndFeel::paintPanelBackground(g, getLocalBounds().toFloat(), true, UI::kPanelRadius);
}

void MidiMappingsPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto top = area.removeFromTop(40);

    inputModeBtn.setBounds(top.removeFromLeft(80).reduced(2));
    outputModeBtn.setBounds(top.removeFromLeft(90).reduced(2));
    top.removeFromLeft(8);

    const int deviceWidth = juce::jmin(360, juce::jmax(260, top.getWidth() / 3));
    deviceSelector.setBounds(top.removeFromLeft(deviceWidth).reduced(0, 2));
    refreshBtn.setBounds(top.removeFromLeft(110).reduced(2));

    top.removeFromLeft(4);
    toolbarDivider.setBounds(top.removeFromLeft(2).withTrimmedTop(6).withHeight(top.getHeight() - 12));
    top.removeFromLeft(4);
    importBtn.setBounds(top.removeFromLeft(90).reduced(2));
    exportBtn.setBounds(top.removeFromLeft(90).reduced(2));

    top.removeFromLeft(4);
    toolbarDivider2.setBounds(top.removeFromLeft(2).withTrimmedTop(6).withHeight(top.getHeight() - 12));
    top.removeFromLeft(4);
    addRowBtn.setBounds(top.removeFromLeft(80).reduced(2));
    clearRowBtn.setBounds(top.removeFromLeft(90).reduced(2));
    learnBtn.setBounds(top.removeFromLeft(110).reduced(2));
    statusLabel.setBounds(top.reduced(2));

    mappingTable.setBounds(area.reduced(0, 8));
    outputMappingTable.setBounds(area.reduced(0, 8));
}

void MidiMappingsPanel::buttonClicked(juce::Button* button)
{
    if (button == &inputModeBtn) {
        setOutputMode(false);
    } else if (button == &outputModeBtn) {
        setOutputMode(true);
    } else if (button == &refreshBtn) {
        if (outputModeActive)
            refreshOutputDeviceList();
        else
            refreshDeviceList();
    } else if (button == &importBtn) {
        fileChooserRef = std::make_shared<juce::FileChooser>(outputModeActive ? "Import MIDI output mappings" : "Import MIDI mappings", juce::File(), "*.xml");
        fileChooserRef->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& chooser) {
                auto file = chooser.getResult();
                if (!file.existsAsFile())
                    return;

                if (outputModeActive) {
                    juce::String outputDeviceId;
                    auto loaded = MidiMappings::loadOutputMappingsFromFile(file, outputDeviceId);
                    if (!loaded.empty()) {
                        outputMappings = std::move(loaded);
                        refreshOutputMappingsTable();
                        saveOutputMappings();
                    }
                } else {
                    juce::String deviceId;
                    auto loaded = MidiMappings::loadMappingsFromFile(file, deviceId);
                    if (!loaded.empty()) {
                        mappings = std::move(loaded);
                        mapper.setMappings(mappings);
                        refreshMappingsTable();
                        saveMappings();
                    }
                }
            });
    } else if (button == &exportBtn) {
        fileChooserRef = std::make_shared<juce::FileChooser>(outputModeActive ? "Export MIDI output mappings" : "Export MIDI mappings", juce::File(), "*.xml");
        fileChooserRef->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& chooser) {
                auto file = chooser.getResult();
                if (!file.exists())
                    file = file.withFileExtension(".xml");

                if (outputModeActive)
                    MidiMappings::saveOutputMappings(outputMappings, getSelectedMidiOutputDeviceId(), file);
                else
                    MidiMappings::exportMappings(mappings, getSelectedMidiDeviceId(), file);
            });
    } else if (button == &addRowBtn) {
        if (outputModeActive) {
            outputMappings.push_back({Midi::MidiActionTarget::None, 1, 0, 0, 127, 0});
            refreshOutputMappingsTable();
            saveOutputMappings();
        } else {
            mappings.push_back({0, 0, 0, Midi::MidiActionTarget::None});
            refreshMappingsTable();
            saveMappings();
        }
    } else if (button == &clearRowBtn) {
        if (outputModeActive) {
            int row = outputMappingTable.getSelectedRow();
            if (row >= 0 && row < (int)outputMappings.size()) {
                outputMappings.erase(outputMappings.begin() + row);
                refreshOutputMappingsTable();
                saveOutputMappings();
            }
        } else {
            int row = mappingTable.getSelectedRow();
            if (row >= 0 && row < (int)mappings.size()) {
                mappings.erase(mappings.begin() + row);
                refreshMappingsTable();
                saveMappings();
            }
        }
    } else if (button == &learnBtn) {
        if (!outputModeActive)
            setLearnMode(!learnModeActive);
    }
}

void MidiMappingsPanel::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &deviceSelector) {
        const int idx = deviceSelector.getSelectedId() - 1;
        if (outputModeActive) {
            if (idx >= 0 && idx < (int)outputDeviceIds.size()) {
                bool opened = mapper.openOutputDevice(outputDeviceIds[idx]);
                if (opened) {
                    AppSettings::saveMidiOutputDeviceId(outputDeviceIds[idx]);
                    saveOutputMappings();
                } else {
                    overrideStatusMessage = "Failed to open output device — please select another";
                }
                updateStatusLabel();
            }
        } else {
            if (idx >= 0 && idx < (int)deviceIds.size()) {
                bool opened = mapper.openDevice(deviceIds[idx]);
                if (opened) {
                    AppSettings::saveMidiDeviceId(deviceIds[idx]);
                    saveMappings();
                } else {
                    overrideStatusMessage = "Failed to open device — please select another";
                }
                updateStatusLabel();
            }
        }
    }
}

void MidiMappingsPanel::refreshDeviceList()
{
    deviceSelector.clear(juce::dontSendNotification);
    deviceIds.clear();

    auto devices = juce::MidiInput::getAvailableDevices();
    for (auto& d : devices) {
        deviceIds.push_back(d.identifier);
        deviceSelector.addItem(d.name, deviceIds.size());
    }

    auto activeId = mapper.getActiveDeviceIdentifier();
    int selected = 0;
    for (int i = 0; i < (int)deviceIds.size(); ++i) {
        if (deviceIds[i] == activeId) {
            selected = i + 1;
            break;
        }
    }

    deviceSelector.setSelectedId(selected, juce::dontSendNotification);
    updateStatusLabel();
}

void MidiMappingsPanel::refreshOutputDeviceList()
{
    deviceSelector.clear(juce::dontSendNotification);
    outputDeviceIds.clear();

    auto devices = juce::MidiOutput::getAvailableDevices();
    for (auto& d : devices) {
        outputDeviceIds.push_back(d.identifier);
        deviceSelector.addItem(d.name, outputDeviceIds.size());
    }

    auto activeId = mapper.getActiveOutputDeviceIdentifier();
    int selected = 0;
    for (int i = 0; i < (int)outputDeviceIds.size(); ++i) {
        if (outputDeviceIds[i] == activeId) {
            selected = i + 1;
            break;
        }
    }

    deviceSelector.setSelectedId(selected, juce::dontSendNotification);
    updateStatusLabel();
}

void MidiMappingsPanel::refreshMappingsTable()
{
    mappingTable.updateContent();
    mappingTable.repaint();
}

void MidiMappingsPanel::saveMappings()
{
    mapper.setMappings(mappings);
    MidiMappings::saveMappings(mappings, getSelectedMidiDeviceId());
}

void MidiMappingsPanel::saveOutputMappings()
{
    mapper.setOutputMappings(outputMappings);
    MidiMappings::saveOutputMappings(outputMappings, getSelectedMidiOutputDeviceId());
}

void MidiMappingsPanel::loadSavedMappings()
{
    juce::String deviceId = AppSettings::loadMidiDeviceId();
    mappings = MidiMappings::loadMappings(deviceId);
    mapper.setMappings(mappings);
    refreshMappingsTable();

    if (!deviceId.isEmpty()) {
        bool opened = mapper.openDevice(deviceId);
        if (!opened)
            overrideStatusMessage = "Device not found — please reconnect or select another";
    }
    updateStatusLabel();
}

void MidiMappingsPanel::loadSavedOutputMappings()
{
    juce::String deviceId = AppSettings::loadMidiOutputDeviceId();
    outputMappings = MidiMappings::loadOutputMappings(deviceId);
    refreshOutputMappingsTable();

    if (!deviceId.isEmpty()) {
        bool opened = mapper.openOutputDevice(deviceId);
        if (!opened)
            overrideStatusMessage = "Output device not found — please reconnect or select another";
    }
    updateStatusLabel();
}

void MidiMappingsPanel::updateStatusLabel()
{
	if (learnModeActive) {
		statusLabel.setText("Listening...", juce::dontSendNotification);
		return;
	}
	if (overrideStatusMessage.isNotEmpty()) {
		statusLabel.setText(overrideStatusMessage, juce::dontSendNotification);
		return;
	}
	statusLabel.setText(mapper.getActiveDeviceIdentifier().isEmpty() ? "Not connected" : "Connected", juce::dontSendNotification);
}

void MidiMappingsPanel::timerCallback()
{
	if (learnModeActive)
		return;

	const auto available = juce::MidiInput::getAvailableDevices();
	const auto activeId = mapper.getActiveDeviceIdentifier();

	if (!activeId.isEmpty()) {
		bool found = false;
		for (const auto& d : available)
			if (d.identifier == activeId) { found = true; break; }

		if (!found) {
			overrideStatusMessage = "Device disconnected — please reconnect or select another";
			mapper.closeDevice();
			updateStatusLabel();
			refreshDeviceList();
		} else if (!overrideStatusMessage.isEmpty()) {
			overrideStatusMessage.clear();
			updateStatusLabel();
		}
	} else if (!overrideStatusMessage.isEmpty()) {
		overrideStatusMessage.clear();
		updateStatusLabel();
	}
}

void MidiMappingsPanel::setLearnMode(bool enable)
{
    learnModeActive = enable;
    learnTargetRow = mappingTable.getSelectedRow();
    if (enable) {
        mapper.setLearnCallback([this](int channel, int messageType, int number) {
            handleLearnMessage(channel, messageType, number);
        });
    } else {
        mapper.clearLearnCallback();
    }
    updateStatusLabel();
}

void MidiMappingsPanel::handleLearnMessage(int channel, int messageType, int number)
{
    if (learnTargetRow < 0 || learnTargetRow >= (int)mappings.size())
        return;

    mappings[learnTargetRow].channel = channel;
    mappings[learnTargetRow].messageType = messageType;
    mappings[learnTargetRow].number = number;
    mappings[learnTargetRow].target = Midi::MidiActionTarget::None;

    setLearnMode(false);
    refreshMappingsTable();
    saveMappings();
}

int MidiMappingsPanel::MappingTableModel::getNumRows()
{
    return owner.mappings.size();
}

void MidiMappingsPanel::MappingTableModel::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour::fromRGBA(0, 125, 225, 120));
    else if (rowNumber % 2 == 0)
        g.fillAll(UI::bgCard);
    else
        g.fillAll(UI::bgRoot);
}

void MidiMappingsPanel::MappingTableModel::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int)owner.mappings.size())
        return;

    const auto& entry = owner.mappings[rowNumber];
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions{ 12.0f }));

    switch (columnId) {
        case 1:
            g.drawText(juce::String(entry.channel), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        case 2:
            g.drawText(owner.getTypeLabel(entry.messageType), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        case 3:
            g.drawText(juce::String(entry.number), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        case 4:
            g.drawText(owner.getDeckLabel(entry.target), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        case 6:
            g.drawText(owner.getStatusLabel(entry), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        default:
            break;
    }
}

juce::Component* MidiMappingsPanel::MappingTableModel::refreshComponentForCell(int rowNumber, int columnId, bool, juce::Component* existingComponent)
{
	if (columnId == 1 || columnId == 3) {
		auto* editor = dynamic_cast<EditableTextEditor*>(existingComponent);
		if (editor == nullptr) {
			editor = new EditableTextEditor(owner);
			editor->setJustification(juce::Justification::centredLeft);
		}
		editor->clear();
		if (columnId == 1)
			editor->setText(juce::String(owner.mappings[rowNumber].channel), juce::dontSendNotification);
		else
			editor->setText(juce::String(owner.mappings[rowNumber].number), juce::dontSendNotification);
		editor->row = rowNumber;
		editor->columnId = columnId;
		return editor;
	}

	if (columnId == 2) {
		auto* combo = dynamic_cast<TypeComboBox*>(existingComponent);
		if (combo == nullptr) {
			combo = new TypeComboBox(owner);
			combo->addItem("Note", 0);
			combo->addItem("CC", 1);
			combo->onChange = [combo]() {
				if (combo == nullptr)
					return;
				const int row = combo->row;
				if (row < 0 || row >= (int)combo->owner.mappings.size())
					return;
				combo->owner.mappings[row].messageType = combo->getSelectedId() - 1;
				combo->owner.saveMappings();
				combo->owner.refreshMappingsTable();
			};
		}
		combo->setRow(rowNumber);
		combo->setSelectedId(owner.mappings[rowNumber].messageType + 1, juce::dontSendNotification);
		return combo;
	}

	if (columnId != 5)
		return existingComponent;

	auto* combo = dynamic_cast<ActionComboBox*>(existingComponent);
	if (combo == nullptr) {
		combo = new ActionComboBox(owner);
		auto choices = owner.getActionChoices();
		for (int i = 0; i < (int)choices.size(); ++i)
			combo->addItem(choices[i].second, i + 1);
	}

	combo->setRow(rowNumber);
	combo->onChange = [combo]() {
		if (combo == nullptr)
			return;

		const int row = combo->row;
		if (row < 0 || row >= (int)combo->owner.mappings.size())
			return;

		const int selectedId = combo->getSelectedId();
		auto choices = combo->owner.getActionChoices();
		if (selectedId <= 0 || selectedId > (int)choices.size())
			return;

		combo->owner.mappings[row].target = choices[selectedId - 1].first;
		combo->owner.saveMappings();
		combo->owner.refreshMappingsTable();
	};

	auto choices = owner.getActionChoices();
	auto it = std::find_if(choices.begin(), choices.end(), [&](auto& p){ return p.first == owner.mappings[rowNumber].target; });
	if (it != choices.end())
		combo->setSelectedId((int)std::distance(choices.begin(), it) + 1, juce::dontSendNotification);
	else
		combo->setSelectedId(1, juce::dontSendNotification);

	return combo;
}

juce::String MidiMappingsPanel::getDeckLabel(Midi::MidiActionTarget target)
{
    switch (target) {
        case Midi::MidiActionTarget::Deck1_Play:
        case Midi::MidiActionTarget::Deck1_CueMon:
        case Midi::MidiActionTarget::Deck1_Filter:
        case Midi::MidiActionTarget::Deck1_Volume:
        case Midi::MidiActionTarget::Deck1_SpeedRel:
        case Midi::MidiActionTarget::Deck1_EqLow:
        case Midi::MidiActionTarget::Deck1_EqMid:
        case Midi::MidiActionTarget::Deck1_EqHigh:
        case Midi::MidiActionTarget::Deck1_JogWheel:
        case Midi::MidiActionTarget::Deck1_BeatFxOn:
        case Midi::MidiActionTarget::Deck1_BeatFxWet:
        case Midi::MidiActionTarget::Deck1_LoopIn:
        case Midi::MidiActionTarget::Deck1_LoopOut:
        case Midi::MidiActionTarget::Deck1_Reloop:
        case Midi::MidiActionTarget::Deck1_Sync:
        case Midi::MidiActionTarget::Deck1_Master:
            return "Deck 1";
        case Midi::MidiActionTarget::Deck2_Play:
        case Midi::MidiActionTarget::Deck2_CueMon:
        case Midi::MidiActionTarget::Deck2_Filter:
        case Midi::MidiActionTarget::Deck2_Volume:
        case Midi::MidiActionTarget::Deck2_SpeedRel:
        case Midi::MidiActionTarget::Deck2_EqLow:
        case Midi::MidiActionTarget::Deck2_EqMid:
        case Midi::MidiActionTarget::Deck2_EqHigh:
        case Midi::MidiActionTarget::Deck2_JogWheel:
        case Midi::MidiActionTarget::Deck2_BeatFxOn:
        case Midi::MidiActionTarget::Deck2_BeatFxWet:
        case Midi::MidiActionTarget::Deck2_LoopIn:
        case Midi::MidiActionTarget::Deck2_LoopOut:
        case Midi::MidiActionTarget::Deck2_Reloop:
        case Midi::MidiActionTarget::Deck2_Sync:
        case Midi::MidiActionTarget::Deck2_Master:
            return "Deck 2";
        default:
            return "Global";
    }
}

juce::String MidiMappingsPanel::getActionLabel(Midi::MidiActionTarget target)
{
    return MidiMappings::actionToString(target);
}

juce::String MidiMappingsPanel::getTypeLabel(int messageType)
{
    return messageType == 0 ? "Note" : messageType == 1 ? "CC" : "Unknown";
}

juce::String MidiMappingsPanel::getStatusLabel(const Midi::MidiMappingEntry& entry)
{
    return entry.target == Midi::MidiActionTarget::None ? "Unmapped" : "Mapped";
}

std::vector<std::pair<Midi::MidiActionTarget, juce::String>> MidiMappingsPanel::getActionChoices()
{
    return {
        { Midi::MidiActionTarget::None, "None" },
        { Midi::MidiActionTarget::Deck1_Play, "Deck1 Play (BTN)" },
        { Midi::MidiActionTarget::Deck2_Play, "Deck2 Play (BTN)" },
        { Midi::MidiActionTarget::Deck1_CueMon, "Deck1 Cue (BTN)" },
        { Midi::MidiActionTarget::Deck2_CueMon, "Deck2 Cue (BTN)" },
        { Midi::MidiActionTarget::Deck1_Volume, "Deck1 Volume (KNOB)" },
        { Midi::MidiActionTarget::Deck2_Volume, "Deck2 Volume (KNOB)" },
        { Midi::MidiActionTarget::Deck1_Filter, "Deck1 Filter (KNOB)" },
        { Midi::MidiActionTarget::Deck2_Filter, "Deck2 Filter (KNOB)" },
        { Midi::MidiActionTarget::Deck1_EqLow, "Deck1 EQ Low (KNOB)" },
        { Midi::MidiActionTarget::Deck1_EqMid, "Deck1 EQ Mid (KNOB)" },
        { Midi::MidiActionTarget::Deck1_EqHigh, "Deck1 EQ High (KNOB)" },
        { Midi::MidiActionTarget::Deck2_EqLow, "Deck2 EQ Low (KNOB)" },
        { Midi::MidiActionTarget::Deck2_EqMid, "Deck2 EQ Mid (KNOB)" },
        { Midi::MidiActionTarget::Deck2_EqHigh, "Deck2 EQ High (KNOB)" },
        { Midi::MidiActionTarget::Deck1_JogWheel, "Deck1 Jog Wheel (KNOB)" },
        { Midi::MidiActionTarget::Deck2_JogWheel, "Deck2 Jog Wheel (KNOB)" },
        { Midi::MidiActionTarget::Deck1_BeatFxOn, "Deck1 Beat FX On/Off (BTN)" },
        { Midi::MidiActionTarget::Deck2_BeatFxOn, "Deck2 Beat FX On/Off (BTN)" },
        { Midi::MidiActionTarget::Deck1_BeatFxWet, "Deck1 Beat FX Wet (KNOB)" },
        { Midi::MidiActionTarget::Deck2_BeatFxWet, "Deck2 Beat FX Wet (KNOB)" },
        { Midi::MidiActionTarget::Deck1_LoopIn, "Deck1 Loop In (BTN)" },
        { Midi::MidiActionTarget::Deck2_LoopIn, "Deck2 Loop In (BTN)" },
        { Midi::MidiActionTarget::Deck1_LoopOut, "Deck1 Loop Out (BTN)" },
        { Midi::MidiActionTarget::Deck2_LoopOut, "Deck2 Loop Out (BTN)" },
        { Midi::MidiActionTarget::Deck1_Reloop, "Deck1 Reloop (BTN)" },
        { Midi::MidiActionTarget::Deck2_Reloop, "Deck2 Reloop (BTN)" },
        { Midi::MidiActionTarget::Deck1_Sync, "Deck1 Sync (BTN)" },
        { Midi::MidiActionTarget::Deck2_Sync, "Deck2 Sync (BTN)" },
        { Midi::MidiActionTarget::Deck1_Master, "Deck1 Master (BTN)" },
        { Midi::MidiActionTarget::Deck2_Master, "Deck2 Master (BTN)" },
        { Midi::MidiActionTarget::Deck1_SpeedRel, "Deck1 Speed (KNOB)" },
        { Midi::MidiActionTarget::Deck2_SpeedRel, "Deck2 Speed (KNOB)" },
        { Midi::MidiActionTarget::Crossfader, "Crossfader (KNOB)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_1, "Deck1 HotCue Set 1 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_2, "Deck1 HotCue Set 2 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_3, "Deck1 HotCue Set 3 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_4, "Deck1 HotCue Set 4 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_5, "Deck1 HotCue Set 5 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_6, "Deck1 HotCue Set 6 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueJump_1, "Deck1 HotCue Jump 1 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueJump_2, "Deck1 HotCue Jump 2 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueJump_3, "Deck1 HotCue Jump 3 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueJump_4, "Deck1 HotCue Jump 4 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueJump_5, "Deck1 HotCue Jump 5 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueJump_6, "Deck1 HotCue Jump 6 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_1, "Deck2 HotCue Set 1 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_2, "Deck2 HotCue Set 2 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_3, "Deck2 HotCue Set 3 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_4, "Deck2 HotCue Set 4 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_5, "Deck2 HotCue Set 5 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_6, "Deck2 HotCue Set 6 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueJump_1, "Deck2 HotCue Jump 1 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueJump_2, "Deck2 HotCue Jump 2 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueJump_3, "Deck2 HotCue Jump 3 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueJump_4, "Deck2 HotCue Jump 4 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueJump_5, "Deck2 HotCue Jump 5 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueJump_6, "Deck2 HotCue Jump 6 (BTN)" },
    };
}

juce::String MidiMappingsPanel::getSelectedMidiDeviceId() const
{
    const int idx = deviceSelector.getSelectedId() - 1;
    return (idx >= 0 && idx < (int)deviceIds.size()) ? deviceIds[idx] : juce::String();
}

juce::String MidiMappingsPanel::getSelectedMidiOutputDeviceId() const
{
    const int idx = deviceSelector.getSelectedId() - 1;
    return (idx >= 0 && idx < (int)outputDeviceIds.size()) ? outputDeviceIds[idx] : juce::String();
}

void MidiMappingsPanel::refreshOutputMappingsTable()
{
    outputMappingTable.updateContent();
    outputMappingTable.repaint();
}

int MidiMappingsPanel::OutputMappingTableModel::getNumRows()
{
    return owner.outputMappings.size();
}

void MidiMappingsPanel::OutputMappingTableModel::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour::fromRGBA(0, 125, 225, 120));
    else if (rowNumber % 2 == 0)
        g.fillAll(UI::bgCard);
    else
        g.fillAll(UI::bgRoot);
}

void MidiMappingsPanel::OutputMappingTableModel::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int)owner.outputMappings.size())
        return;

    const auto& entry = owner.outputMappings[rowNumber];
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions{ 12.0f }));

    switch (columnId) {
        case 2:
            g.drawText(juce::String(entry.channel), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        case 3:
            g.drawText(owner.getTypeLabel(entry.messageType), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        case 4:
            g.drawText(juce::String(entry.number), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        case 5:
            g.drawText(juce::String(entry.onValue), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        case 6:
            g.drawText(juce::String(entry.offValue), 2, 0, width - 4, height, juce::Justification::centredLeft);
            break;
        default:
            break;
    }
}

juce::Component* MidiMappingsPanel::OutputMappingTableModel::refreshComponentForCell(int rowNumber, int columnId, bool, juce::Component* existingComponent)
{
    if (columnId == 2 || columnId == 4 || columnId == 5 || columnId == 6) {
        auto* editor = dynamic_cast<OutputEditableTextEditor*>(existingComponent);
        if (editor == nullptr) {
            editor = new OutputEditableTextEditor(owner);
            editor->setJustification(juce::Justification::centredLeft);
        }

        editor->clear();
        if (columnId == 2)
            editor->setText(juce::String(owner.outputMappings[rowNumber].channel), juce::dontSendNotification);
        else if (columnId == 4)
            editor->setText(juce::String(owner.outputMappings[rowNumber].number), juce::dontSendNotification);
        else if (columnId == 5)
            editor->setText(juce::String(owner.outputMappings[rowNumber].onValue), juce::dontSendNotification);
        else if (columnId == 6)
            editor->setText(juce::String(owner.outputMappings[rowNumber].offValue), juce::dontSendNotification);

        editor->row = rowNumber;
        editor->columnId = columnId;
        return editor;
    }

    if (columnId == 3) {
        auto* combo = dynamic_cast<TypeComboBox*>(existingComponent);
        if (combo == nullptr) {
            combo = new TypeComboBox(owner);
            combo->addItem("Note", 1);
            combo->addItem("CC", 2);
            combo->onChange = [combo]() {
                if (combo == nullptr)
                    return;
                const int row = combo->row;
                if (row < 0 || row >= (int)combo->owner.outputMappings.size())
                    return;
                combo->owner.outputMappings[row].messageType = combo->getSelectedId() - 1;
                combo->owner.saveOutputMappings();
                combo->owner.refreshOutputMappingsTable();
            };
        }
        combo->setRow(rowNumber);
        combo->setSelectedId(owner.outputMappings[rowNumber].messageType + 1, juce::dontSendNotification);
        return combo;
    }

    if (columnId != 1)
        return existingComponent;

    auto* combo = dynamic_cast<OutputActionComboBox*>(existingComponent);
    if (combo == nullptr) {
        combo = new OutputActionComboBox(owner);
        auto choices = owner.getOutputActionChoices();
        for (int i = 0; i < (int)choices.size(); ++i)
            combo->addItem(choices[i].second, i + 1);
    }

    combo->setRow(rowNumber);
    combo->onChange = [combo]() {
        if (combo == nullptr)
            return;
        const int row = combo->row;
        if (row < 0 || row >= (int)combo->owner.outputMappings.size())
            return;
        const int selectedId = combo->getSelectedId();
        auto choices = combo->owner.getOutputActionChoices();
        if (selectedId <= 0 || selectedId > (int)choices.size())
            return;
        combo->owner.outputMappings[row].target = choices[selectedId - 1].first;
        combo->owner.saveOutputMappings();
        combo->owner.refreshOutputMappingsTable();
    };

    auto choices = owner.getOutputActionChoices();
    auto it = std::find_if(choices.begin(), choices.end(), [&](auto& p) { return p.first == owner.outputMappings[rowNumber].target; });
    if (it != choices.end())
        combo->setSelectedId((int)std::distance(choices.begin(), it) + 1, juce::dontSendNotification);
    else
        combo->setSelectedId(1, juce::dontSendNotification);

    return combo;
}

std::vector<std::pair<Midi::MidiActionTarget, juce::String>> MidiMappingsPanel::getOutputActionChoices()
{
    return {
        { Midi::MidiActionTarget::None, "None" },
        { Midi::MidiActionTarget::Deck1_Play, "Deck1 Play (BTN)" },
        { Midi::MidiActionTarget::Deck2_Play, "Deck2 Play (BTN)" },
        { Midi::MidiActionTarget::Deck1_CueMon, "Deck1 Cue (BTN)" },
        { Midi::MidiActionTarget::Deck2_CueMon, "Deck2 Cue (BTN)" },
        { Midi::MidiActionTarget::Deck1_BeatFxOn, "Deck1 Beat FX On/Off (BTN)" },
        { Midi::MidiActionTarget::Deck2_BeatFxOn, "Deck2 Beat FX On/Off (BTN)" },
        { Midi::MidiActionTarget::Deck1_Reloop, "Deck1 Reloop (BTN)" },
        { Midi::MidiActionTarget::Deck2_Reloop, "Deck2 Reloop (BTN)" },
        { Midi::MidiActionTarget::Deck1_Sync, "Deck1 Sync (BTN)" },
        { Midi::MidiActionTarget::Deck2_Sync, "Deck2 Sync (BTN)" },
        { Midi::MidiActionTarget::Deck1_Master, "Deck1 Master (BTN)" },
        { Midi::MidiActionTarget::Deck2_Master, "Deck2 Master (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_1, "Deck1 HotCue Set 1 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_2, "Deck1 HotCue Set 2 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_3, "Deck1 HotCue Set 3 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_4, "Deck1 HotCue Set 4 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_5, "Deck1 HotCue Set 5 (BTN)" },
        { Midi::MidiActionTarget::Deck1_HotCueSet_6, "Deck1 HotCue Set 6 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_1, "Deck2 HotCue Set 1 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_2, "Deck2 HotCue Set 2 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_3, "Deck2 HotCue Set 3 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_4, "Deck2 HotCue Set 4 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_5, "Deck2 HotCue Set 5 (BTN)" },
        { Midi::MidiActionTarget::Deck2_HotCueSet_6, "Deck2 HotCue Set 6 (BTN)" },
        { Midi::MidiActionTarget::Deck1_VuMeter, "Deck1 VU Meter (CC)" },
        { Midi::MidiActionTarget::Deck2_VuMeter, "Deck2 VU Meter (CC)" },
    };
}

void MidiMappingsPanel::setOutputMode(bool enable)
{
    outputModeActive = enable;
    inputModeBtn.setToggleState(!enable, juce::dontSendNotification);
    outputModeBtn.setToggleState(enable, juce::dontSendNotification);
    mappingTable.setVisible(!enable);
    outputMappingTable.setVisible(enable);
    learnBtn.setEnabled(!enable);
    if (enable)
        refreshOutputDeviceList();
    else
        refreshDeviceList();
}

juce::String MidiMappingsPanel::getMappingDeviceId() const
{
    return outputModeActive ? getSelectedMidiOutputDeviceId() : getSelectedMidiDeviceId();
}
