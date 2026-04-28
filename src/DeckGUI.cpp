
#include "DeckGUI.h"
#include "FileHasher.h"
#include "FxFactory.h"
#include "FxChain.h"
#include "FxProcessor.h"
#include "FxParameterModal.h"
#include "FxSettings.h"
#include "AudioEngine.h"

//============================================================================================================================================================

/**
 * Implementation of a constructor for DeckGUI
 *
 * In the constructor, binary data of svg assets are parsed in xml elements
 * and further parse into juce::Drawable members to define the appearance of
 * button components. Private data members are being initialized with hard values
 * or passed in references. Initial component configurations are performed here as
 * well
 *
 */
DeckGUI::DeckGUI(DJAudioPlayer* _player, juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, ZoomedWaveform* _zoomedDisplay, Library& _library, juce::Colour _colour, BeatSyncManager* _syncManager, int _deckIndex, AudioEngine* _audioEngine) : player(_player), audioEngine(_audioEngine), waveformDisplay(formatManagerToUse, cacheToUse, _colour), zoomedDisplay(_zoomedDisplay), jogWheel(formatManagerToUse, cacheToUse, _colour), library(&_library), theme(_colour), syncManager(_syncManager), deckIndex(_deckIndex)
{
	if (audioEngine != nullptr)
		audioEngine->addListener(this);

	// "Loading…" overlay (centred over the waveform area, hidden by default).
	loadingLabel.setJustificationType(juce::Justification::centred);
	loadingLabel.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
	loadingLabel.setColour(juce::Label::textColourId, theme);
	loadingLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGBA(25, 25, 25, 220));
	loadingLabel.setVisible(false);
	addAndMakeVisible(loadingLabel);
	std::vector<juce::Label*> labels{ &volLabel, &speedLabel, &filterLabel, &lbLabel, &mbLabel, &hbLabel };
	for (auto& label : labels) {
		label->setEditable(false);
		label->setJustificationType(juce::Justification::centred);
		addAndMakeVisible(*label);
	}

	// BPM value and percent labels
	bpmValueLabel.setEditable(false);
	bpmValueLabel.setJustificationType(juce::Justification::centred);
	bpmValueLabel.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
	bpmValueLabel.setColour(juce::Label::textColourId, theme);
	addAndMakeVisible(bpmValueLabel);

	bpmPercentLabel.setEditable(false);
	bpmPercentLabel.setJustificationType(juce::Justification::centred);
	bpmPercentLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
	bpmPercentLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	addAndMakeVisible(bpmPercentLabel);

	addAndMakeVisible(playButton);
	addAndMakeVisible(volSlider);
	addAndMakeVisible(speedSlider);
	addAndMakeVisible(loadButton);
	addAndMakeVisible(waveformDisplay);
	addAndMakeVisible(jogWheel);
	addAndMakeVisible(filter);
	addAndMakeVisible(lowBandFilter);
	addAndMakeVisible(midBandFilter);
	addAndMakeVisible(highBandFilter);

	// Per-deck queue widget. Click a row to jump to that track.
	queueWidget = std::make_unique<DeckQueue>(theme, [this](const track& t) { loadDeck(t); });
	addAndMakeVisible(*queueWidget);

	volSlider.setRange(0, 1);
	speedSlider.setRange(0.8, 1.2);
	filter.setRange(-20000, 20000);
	lowBandFilter.setRange(0.01, 2);
	midBandFilter.setRange(0.01, 2);
	highBandFilter.setRange(0.01, 2);
	waveformDisplay.setRange(0, 1);
	zoomedDisplay->setRange(0, 1);
	jogWheel.setRange(0, 1);

	filter.setValue(0);
	lowBandFilter.setValue(1);
	midBandFilter.setValue(1);
	highBandFilter.setValue(1);
	volSlider.setValue(0.5);
	speedSlider.setValue(1);

	playButton.addListener(this);
	loadButton.addListener(this);
	volSlider.addListener(this);
	speedSlider.addListener(this);
	speedSlider.addMouseListener(this, false);

	filter.addListener(this);
	lowBandFilter.addListener(this);
	midBandFilter.addListener(this);
	highBandFilter.addListener(this);

	startTimer(20);

	for (auto i = 0; i < 6; ++i) {
		cues.push_back(new juce::TextButton());
	}
	for (auto& cue : cues) {
		addAndMakeVisible(cue);
		cue->addListener(this);
		cue->addMouseListener(this, false);
		cue->setLookAndFeel(&customLookAndFeel);
	}

	// Tab buttons for cue/grid/jump/loop/sync switching
	addAndMakeVisible(cueTabButton);
	addAndMakeVisible(gridTabButton);
	addAndMakeVisible(jumpTabButton);
	addAndMakeVisible(loopTabButton);
	addAndMakeVisible(syncTabButton);
	cueTabButton.addListener(this);
	gridTabButton.addListener(this);
	jumpTabButton.addListener(this);
	loopTabButton.addListener(this);
	syncTabButton.addListener(this);
	cueTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
	gridTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	jumpTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	loopTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	syncTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));

	// FX tab buttons (P.FX / B.FX / R.FX) — registered here so the row is built
	// in tab-order. Their colour follows the same pattern as the other tabs.
	addAndMakeVisible(padFxTabButton);
	addAndMakeVisible(beatFxTabButton);
	addAndMakeVisible(releaseFxTabButton);
	padFxTabButton.addListener(this);
	beatFxTabButton.addListener(this);
	releaseFxTabButton.addListener(this);
	padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));

	// Sync tab controls (initially hidden until SYNC tab selected).
	masterToggleBtn.setClickingTogglesState(true);
	masterToggleBtn.addListener(this);
	masterToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	masterToggleBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange.withAlpha(0.8f));
	masterToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	masterToggleBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
	addChildComponent(masterToggleBtn);

	syncEngageBtn.setClickingTogglesState(true);
	syncEngageBtn.addListener(this);
	syncEngageBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	syncEngageBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::dodgerblue.withAlpha(0.85f));
	syncEngageBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	syncEngageBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
	addChildComponent(syncEngageBtn);

	for (auto* btn : { &multHalfBtn, &multOneBtn, &multTwoBtn }) {
		btn->addListener(this);
		btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		addChildComponent(*btn);
	}

	targetBpmLabel.setEditable(false);
	targetBpmLabel.setJustificationType(juce::Justification::centred);
	targetBpmLabel.setColour(juce::Label::textColourId, theme);
	targetBpmLabel.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
	addChildComponent(targetBpmLabel);

	syncStatusLabel.setEditable(false);
	syncStatusLabel.setJustificationType(juce::Justification::centred);
	syncStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	syncStatusLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
	addChildComponent(syncStatusLabel);

	// Fast-sync compact button (always visible near play/load).
	fastSyncBtn.setClickingTogglesState(true);
	fastSyncBtn.addListener(this);
	fastSyncBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	fastSyncBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::dodgerblue.withAlpha(0.85f));
	fastSyncBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	fastSyncBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
	addAndMakeVisible(fastSyncBtn);

	// Snap-quantisation combo for sync (1 BAR / 1/2 / 1/4).
	snapBox.addItem("1 BAR",   1);
	snapBox.addItem("1/2 BAR", 2);
	snapBox.addItem("1/4 BAR", 3);
	snapBox.setSelectedId(1, juce::dontSendNotification);
	snapBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	snapBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
	snapBox.setColour(juce::ComboBox::outlineColourId, theme.withAlpha(0.5f));
	snapBox.onChange = [this]() {
		if (syncManager == nullptr) return;
		switch (snapBox.getSelectedId()) {
			case 1: syncManager->setSnapBeats(4); break; // 1 bar
			case 2: syncManager->setSnapBeats(2); break; // 1/2 bar
			case 3: syncManager->setSnapBeats(1); break; // 1/4 bar
		}
	};
	addChildComponent(snapBox);

	// Reset button: clears master, disengages sync, restores speed to 1.0.
	syncResetBtn.addListener(this);
	syncResetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	syncResetBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	addChildComponent(syncResetBtn);

	// Quantize tab button and controls
	addAndMakeVisible(quantizeTabButton);
	quantizeTabButton.addListener(this);
	quantizeTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));

	quantizeLabel.setEditable(false);
	quantizeLabel.setJustificationType(juce::Justification::centredLeft);
	quantizeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	addChildComponent(quantizeLabel);

	quantizeComboBox.addItem("None", 1);
	quantizeComboBox.addItem("1 Bar", 2);
	quantizeComboBox.addItem("1/2 Bar", 3);
	quantizeComboBox.addItem("1/3 Bar", 4);
	quantizeComboBox.addItem("1/4 Bar", 5);
	quantizeComboBox.addItem("1/5 Bar", 6);
	quantizeComboBox.addItem("1/6 Bar", 7);
	quantizeComboBox.addItem("1/7 Bar", 8);
	quantizeComboBox.addItem("1/8 Bar", 9);
	quantizeComboBox.addItem("1/9 Bar", 10);
	quantizeComboBox.addItem("1/32 Bar", 11);
	quantizeComboBox.setSelectedId(1, juce::dontSendNotification);
	quantizeComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	quantizeComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
	quantizeComboBox.setColour(juce::ComboBox::outlineColourId, theme.withAlpha(0.5f));
	addChildComponent(quantizeComboBox);

	// Beat grid controls
	gridBpmLabel.setEditable(false);
	gridBpmLabel.setJustificationType(juce::Justification::centredLeft);
	gridBpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	addChildComponent(gridBpmLabel);

	gridBpmEditor.setJustification(juce::Justification::centred);
	gridBpmEditor.setInputRestrictions(7, "0123456789.");
	gridBpmEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	gridBpmEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
	gridBpmEditor.setColour(juce::TextEditor::outlineColourId, theme.withAlpha(0.5f));
	auto applyBpmFromEditor = [this]() {
		double newBpm = gridBpmEditor.getText().getDoubleValue();
		if (newBpm > 20.0 && newBpm < 300.0) {
			BeatGrid grid = player->getBeatGrid();
			grid.bpm = newBpm;
			grid.isManualBpm = true;
			player->setBeatGrid(grid);
			saveTrackData(grid);
		}
	};
	gridBpmEditor.onReturnKey = applyBpmFromEditor;
	gridBpmEditor.onFocusLost = applyBpmFromEditor;
	addChildComponent(gridBpmEditor);

	gridNudgeLeftBtn.addListener(this);
	gridNudgeRightBtn.addListener(this);
	tapTempoBtn.addListener(this);
	gridResetBtn.addListener(this);
	gridNudgeLeftBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	gridNudgeRightBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	tapTempoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	gridResetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	addChildComponent(gridNudgeLeftBtn);
	addChildComponent(gridNudgeRightBtn);
	addChildComponent(gridOffsetLabel);
	addChildComponent(tapTempoBtn);
	addChildComponent(gridResetBtn);

	gridOffsetLabel.setEditable(false);
	gridOffsetLabel.setJustificationType(juce::Justification::centred);
	gridOffsetLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	gridOffsetLabel.setFont(juce::Font(juce::FontOptions(10.0f)));

	// Beat jump controls
	std::vector<juce::TextButton*> jumpBtns{
		&jumpBackward16Btn, &jumpBackward8Btn, &jumpBackward4Btn, &jumpBackward1Btn,
		&jumpForward1Btn, &jumpForward4Btn, &jumpForward8Btn, &jumpForward16Btn
	};
	for (auto* btn : jumpBtns) {
		btn->addListener(this);
		btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		addChildComponent(*btn);
	}

	jumpLabel.setEditable(false);
	jumpLabel.setJustificationType(juce::Justification::centred);
	jumpLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	jumpLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
	addChildComponent(jumpLabel);

	// Loop controls
	std::vector<juce::TextButton*> loopBtns{
		&loopInBtn, &loopOutBtn, &reloopBtn, &loopHalveBtn, &loopDoubleBtn, &loopClearBtn
	};
	for (auto* btn : loopBtns) {
		btn->addListener(this);
		btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		addChildComponent(*btn);
	}

	const std::unique_ptr<juce::XmlElement> playButton_xml(juce::XmlDocument::parse(BinaryData::playButton_svg));
	const std::unique_ptr<juce::XmlElement> playButtonHover_xml(juce::XmlDocument::parse(BinaryData::playButtonHover_svg));
	const std::unique_ptr<juce::XmlElement> stopButton_xml(juce::XmlDocument::parse(BinaryData::pauseButton_svg));
	const std::unique_ptr<juce::XmlElement> stopButtonHover_xml(juce::XmlDocument::parse(BinaryData::pauseButtonHover_svg));
	const std::unique_ptr<juce::XmlElement> loadButton_xml(juce::XmlDocument::parse(BinaryData::loadButton_svg));
	const std::unique_ptr<juce::XmlElement> loadButtonHover_xml(juce::XmlDocument::parse(BinaryData::loadButtonHover_svg));
	playButtonImage = juce::Drawable::createFromSVG(*playButton_xml);
	playButtonHoverImage = juce::Drawable::createFromSVG(*playButtonHover_xml);
	stopButtonImage = juce::Drawable::createFromSVG(*stopButton_xml);
	stopButtonHoverImage = juce::Drawable::createFromSVG(*stopButtonHover_xml);
	loadButtonImage = juce::Drawable::createFromSVG(*loadButton_xml);
	loadButtonHoverImage = juce::Drawable::createFromSVG(*loadButtonHover_xml);

	playButton.setImages(playButtonImage.get(),
		playButtonHoverImage.get(),
		nullptr,
		nullptr,
		stopButtonImage.get(),
		stopButtonHoverImage.get(),
		nullptr,
		nullptr);
	loadButton.setImages(loadButtonImage.get(), loadButtonHoverImage.get());
	playButton.setClickingTogglesState(true);
	playButton.setEdgeIndent(0);
	loadButton.setEdgeIndent(0);

	volSlider.setLookAndFeel(&customLookAndFeel);
	speedSlider.setLookAndFeel(&customLookAndFeel);
	filter.setLookAndFeel(&customLookAndFeel);
	lowBandFilter.setLookAndFeel(&customLookAndFeel);
	midBandFilter.setLookAndFeel(&customLookAndFeel);
	highBandFilter.setLookAndFeel(&customLookAndFeel);

	// Build Pad / Beat / Release FX panels (initially hidden — HotCues is the
	// default tab). Must run after `player` is bound because tile callbacks
	// post FxSelect / FxSetEngaged commands through `player->postCommand()`.
	buildFxPanels();
}

/**
 * Implementation of a destructor for DeckGUI
 *
 * DeckGUI instance calls it's timer to stop and free dynamically allocated juce::TextButton objects in cues
 *
 */
DeckGUI::~DeckGUI()
{
	if (audioEngine != nullptr)
		audioEngine->removeListener(this);
	stopTimer();
	for (auto& cue : cues) {
		delete cue;
	}
}

//============================================================================== 

/**
 * Implementation of paint method for DeckGUI
 *
 * Cue button colours are being set and the volume meter is drawn based on the volRMS value.
 *
 */
void DeckGUI::paint(juce::Graphics& g)
{
	g.fillAll(juce::Colour::fromRGBA(50, 50, 50, 255));

	double rowH = getHeight() / 9;
	float offset = rowH * 2.23;
	float volMeterHeight = rowH * 2.5;
	float volCurrentHeight = juce::jmap(player->getRMSLevel(), -60.0f, 0.0f, offset + volMeterHeight - 5, offset);

	for (auto i = offset + volMeterHeight - 5; i > offset; i -= volMeterHeight / 10) {
		float pos = i;
		float redStrength = juce::jmap(pos, offset + volMeterHeight - 5, offset, 0.0f, 255.0f);

		juce::Colour colorRGB(redStrength, 255 - redStrength, 0);
		g.setColour(colorRGB);

		if (volCurrentHeight < pos) {
			g.setColour(colorRGB);
		}
		else {
			g.setColour(juce::Colour::fromRGBA(25, 25, 25, 255));
		}

		double volXOffset = theme == juce::Colours::hotpink ? 62.5 : getWidth() - (double)75;

		juce::Rectangle<float> rect(volXOffset, pos, 12.5, (volMeterHeight / 10) - 2);
		g.fillRect(rect);
	}

	for (auto& cue : cues) {
		juce::TextButton* thisButton = cue;
		bool hasCue = cueTargets.find(thisButton) != cueTargets.end();

		// Skip color update if this button has a pending quantize action (orange)
		if (pendingAction.isValid() && pendingAction.srcButton == thisButton) {
			// Keep orange — don't override
		}
		else if (hasCue && flash) {
			thisButton->setColour(juce::TextButton::ColourIds::buttonColourId, juce::Colour::fromHSL(cueTargets[thisButton].second, (float)1, (float)0.5, (float)1));
		}
		else {
			thisButton->setColour(juce::TextButton::ColourIds::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		}

		if (hasCue) {
			double cueSeconds = cueTargets[thisButton].first * player->getLengthInSeconds();
			std::string timeStr = track::getLengthString(cueSeconds);
			thisButton->setButtonText(juce::String(timeStr) + "  x");
		}
		else {
			thisButton->setButtonText("");
		}
	}

	double mainXOffset = theme == juce::Colours::hotpink ? getWidth() * 7 / 32 : getWidth() * 25 / 32;
	g.setColour(juce::Colour::fromRGBA(25, 25, 25, 255));
	g.drawLine(mainXOffset, 0, mainXOffset, getHeight());
}

/**
 * Implementation of resized method for DeckGUI
 *
 * All juce::Component data members call it's setBounds method to achieve uniform space and sizing.
 *
 */
void DeckGUI::resized()
{
	double rowH = getHeight() / 9;
	double volXOffset = theme == juce::Colours::hotpink ? 5.5 : getWidth() - (double)55;
	volSlider.setBounds(volXOffset, rowH * 2, 50, rowH * 3);
	volLabel.setBounds(volXOffset, rowH * 5 + 5, 50, rowH * 0.5);
	filter.setBounds(volXOffset, rowH * 5.8, 50, 50);
	filterLabel.setBounds(volXOffset, rowH * 6.9, 50, 50);
	double mainXOffset = theme == juce::Colours::hotpink ? getWidth() * 7 / 32 : 0;

	// BPM value label above speed slider
	bpmValueLabel.setBounds(mainXOffset, rowH * 1.3, getWidth() / 8, 20);
	bpmPercentLabel.setBounds(mainXOffset, rowH * 1.3 + 18, getWidth() / 8, 14);

	speedSlider.setBounds(mainXOffset, rowH * 2, getWidth() / 8, rowH * 3);
	speedLabel.setBounds(mainXOffset, rowH * 5 + 5, getWidth() / 8, rowH * 0.5);
	jogWheel.setBounds(mainXOffset + getWidth() * 22.5 / 32 - 98.9, 5 + rowH * 2, (rowH * 3.3) - 10, (rowH * 3.3) - 10);
	loadButton.setBounds(mainXOffset + getWidth() * 22.5 / 32, rowH * 2 + 5, rowH * 0.7, rowH * 0.7);
	playButton.setBounds(mainXOffset + getWidth() * 22.5 / 32, rowH * 5 - 10, rowH * 0.7, rowH * 0.7);
	fastSyncBtn.setBounds(mainXOffset + getWidth() * 22.5 / 32, rowH * 5 - 10 + rowH * 0.7 + 4, rowH * 0.7, rowH * 0.5);

	waveformDisplay.setBounds(0, 0, getWidth(), rowH * 2);
	loadingLabel.setBounds(waveformDisplay.getBounds());

	double xOffset = mainXOffset + getWidth() * 4 / 32;
	double yOffset = 5 + rowH * 2;
	double cellLength = (getWidth() * 18.5 / 32 - 105) / 3;
	double cellHeight = 44.45;

	// Tab buttons above cue/grid/jump/loop/quantize/sync/fx area (9 tabs total)
	double tabAreaWidth = cellLength * 3;
	double tabWidth = (tabAreaWidth - 16) / 9; // 9 tabs with 2px gaps (×8 = 16)
	double tabHeight = 20;
	cueTabButton      .setBounds(xOffset + (tabWidth + 2) * 0, yOffset - tabHeight - 2, tabWidth, tabHeight);
	gridTabButton     .setBounds(xOffset + (tabWidth + 2) * 1, yOffset - tabHeight - 2, tabWidth, tabHeight);
	jumpTabButton     .setBounds(xOffset + (tabWidth + 2) * 2, yOffset - tabHeight - 2, tabWidth, tabHeight);
	loopTabButton     .setBounds(xOffset + (tabWidth + 2) * 3, yOffset - tabHeight - 2, tabWidth, tabHeight);
	quantizeTabButton .setBounds(xOffset + (tabWidth + 2) * 4, yOffset - tabHeight - 2, tabWidth, tabHeight);
	syncTabButton     .setBounds(xOffset + (tabWidth + 2) * 5, yOffset - tabHeight - 2, tabWidth, tabHeight);
	padFxTabButton    .setBounds(xOffset + (tabWidth + 2) * 6, yOffset - tabHeight - 2, tabWidth, tabHeight);
	beatFxTabButton   .setBounds(xOffset + (tabWidth + 2) * 7, yOffset - tabHeight - 2, tabWidth, tabHeight);
	releaseFxTabButton.setBounds(xOffset + (tabWidth + 2) * 8, yOffset - tabHeight - 2, tabWidth, tabHeight);

	// Cue buttons (same as before)
	for (auto i = 0; i < 3; ++i) {
		for (auto j = 0; j < 2; ++j) {
			int index = i * 2 + j;
			cues[index]->setBounds(i * cellLength + xOffset, j * cellHeight + 4 + yOffset, cellLength - 4, cellHeight - 4);
		}
	}

	// Beat grid controls layout (same area as cue buttons)
	double gridRow1Y = yOffset + 4;
	double gridRow2Y = yOffset + cellHeight + 4;
	double ctrlWidth = cellLength - 4;

	gridBpmLabel.setBounds(xOffset, gridRow1Y, ctrlWidth * 0.4, cellHeight - 4);
	gridBpmEditor.setBounds(xOffset + ctrlWidth * 0.4, gridRow1Y, ctrlWidth * 0.6, cellHeight - 4);

	gridOffsetLabel.setBounds(xOffset + cellLength, gridRow1Y, ctrlWidth, 14);
	gridNudgeLeftBtn.setBounds(xOffset + cellLength, gridRow1Y + 14, ctrlWidth * 0.5 - 2, cellHeight - 18);
	gridNudgeRightBtn.setBounds(xOffset + cellLength + ctrlWidth * 0.5, gridRow1Y + 14, ctrlWidth * 0.5 - 2, cellHeight - 18);

	tapTempoBtn.setBounds(xOffset + cellLength * 2, gridRow1Y, ctrlWidth, cellHeight - 4);

	gridResetBtn.setBounds(xOffset, gridRow2Y, ctrlWidth, cellHeight - 4);

	// Beat jump controls layout (same area as cue buttons)
	double jumpAreaWidth = cellLength * 3 - 4;
	double jumpBtnWidth = jumpAreaWidth / 4 - 3;
	double jumpRow1Y = yOffset + 4;
	double jumpRow2Y = yOffset + cellHeight + 4;

	jumpLabel.setBounds(xOffset, jumpRow1Y - 14, jumpAreaWidth, 14);
	jumpBackward16Btn.setBounds(xOffset, jumpRow1Y, jumpBtnWidth, cellHeight - 4);
	jumpBackward8Btn.setBounds(xOffset + (jumpBtnWidth + 4), jumpRow1Y, jumpBtnWidth, cellHeight - 4);
	jumpBackward4Btn.setBounds(xOffset + (jumpBtnWidth + 4) * 2, jumpRow1Y, jumpBtnWidth, cellHeight - 4);
	jumpBackward1Btn.setBounds(xOffset + (jumpBtnWidth + 4) * 3, jumpRow1Y, jumpBtnWidth, cellHeight - 4);
	jumpForward1Btn.setBounds(xOffset, jumpRow2Y, jumpBtnWidth, cellHeight - 4);
	jumpForward4Btn.setBounds(xOffset + (jumpBtnWidth + 4), jumpRow2Y, jumpBtnWidth, cellHeight - 4);
	jumpForward8Btn.setBounds(xOffset + (jumpBtnWidth + 4) * 2, jumpRow2Y, jumpBtnWidth, cellHeight - 4);
	jumpForward16Btn.setBounds(xOffset + (jumpBtnWidth + 4) * 3, jumpRow2Y, jumpBtnWidth, cellHeight - 4);

	// Loop controls layout (same area as cue buttons)
	double loopRow1Y = yOffset + 4;
	double loopRow2Y = yOffset + cellHeight + 4;
	double loopBtnWidth = (cellLength * 3 - 4) / 3 - 3;
	loopInBtn.setBounds(xOffset, loopRow1Y, loopBtnWidth, cellHeight - 4);
	loopOutBtn.setBounds(xOffset + (loopBtnWidth + 4), loopRow1Y, loopBtnWidth, cellHeight - 4);
	reloopBtn.setBounds(xOffset + (loopBtnWidth + 4) * 2, loopRow1Y, loopBtnWidth, cellHeight - 4);
	loopHalveBtn.setBounds(xOffset, loopRow2Y, loopBtnWidth, cellHeight - 4);
	loopDoubleBtn.setBounds(xOffset + (loopBtnWidth + 4), loopRow2Y, loopBtnWidth, cellHeight - 4);
	loopClearBtn.setBounds(xOffset + (loopBtnWidth + 4) * 2, loopRow2Y, loopBtnWidth, cellHeight - 4);

	// Quantize controls layout (same area as cue buttons)
	double qContentWidth = cellLength * 3 - 4;
	quantizeLabel.setBounds(xOffset, yOffset + 4, qContentWidth, 20);
	quantizeComboBox.setBounds(xOffset, yOffset + 26, qContentWidth, 28);

	// Sync controls layout (same area as cue buttons): 4 cols x 2 rows.
	double syncRow1Y = yOffset + 4;
	double syncRow2Y = yOffset + cellHeight + 4;
	double syncTotalW = cellLength * 3 - 4;
	double syncCol4W  = (syncTotalW - 4 * 3) / 4;
	masterToggleBtn.setBounds(xOffset + (syncCol4W + 4) * 0, syncRow1Y, syncCol4W, cellHeight - 4);
	syncEngageBtn  .setBounds(xOffset + (syncCol4W + 4) * 1, syncRow1Y, syncCol4W, cellHeight - 4);
	targetBpmLabel .setBounds(xOffset + (syncCol4W + 4) * 2, syncRow1Y, syncCol4W, cellHeight - 4);
	syncResetBtn   .setBounds(xOffset + (syncCol4W + 4) * 3, syncRow1Y, syncCol4W, cellHeight - 4);
	// Row 2: ×½ ×1 ×2 (3 buttons), snap dropdown, status label.
	double row2TotalW = syncTotalW;
	double multBtnWidth = row2TotalW * 0.16;
	double snapBoxWidth = row2TotalW * 0.28;
	double statusWidth  = row2TotalW - multBtnWidth * 3 - snapBoxWidth - 4 * 4;
	if (statusWidth < 0) statusWidth = 0;
	double cx = xOffset;
	multHalfBtn.setBounds(cx, syncRow2Y, multBtnWidth, cellHeight - 4); cx += multBtnWidth + 4;
	multOneBtn.setBounds (cx, syncRow2Y, multBtnWidth, cellHeight - 4); cx += multBtnWidth + 4;
	multTwoBtn.setBounds (cx, syncRow2Y, multBtnWidth, cellHeight - 4); cx += multBtnWidth + 4;
	snapBox.setBounds    (cx, syncRow2Y, snapBoxWidth, cellHeight - 4); cx += snapBoxWidth + 4;
	syncStatusLabel.setBounds(cx, syncRow2Y, statusWidth, cellHeight - 4);

	// ----- Pad FX layout: 4×2 tile grid in the cue/grid area. -----
	{
		double gridW = cellLength * 3;
		double padCellW = (gridW - 4) / 4;     // 4 columns
		double padCellH = cellHeight;          // 2 rows × cellHeight matches existing area
		for (int idx = 0; idx < (int) padFxTiles.size(); ++idx) {
			int col = idx % 4;
			int row = idx / 4;
			padFxTiles[idx]->setBounds(
				xOffset + col * padCellW,
				yOffset + 4 + row * padCellH,
				padCellW - 4,
				padCellH - 4);
		}
	}

	// ----- Beat FX layout: row 1 = effect picker + division + ON/OFF;
	// row 2 = wet slider + EDIT button. -----
	{
		double bfxRow1Y = yOffset + 4;
		double bfxRow2Y = yOffset + cellHeight + 4;
		double bfxTotalW = cellLength * 3 - 4;
		double bfxColW   = (bfxTotalW - 4 * 2) / 3; // 3 cols, 2 gaps
		beatFxSelector   .setBounds(xOffset + 0,                    bfxRow1Y, bfxColW, cellHeight - 4);
		beatFxDivisionBox.setBounds(xOffset + (bfxColW + 4),        bfxRow1Y, bfxColW, cellHeight - 4);
		beatFxOnButton   .setBounds(xOffset + (bfxColW + 4) * 2,    bfxRow1Y, bfxColW, cellHeight - 4);

		double labelW = 36;
		double editW  = bfxColW;
		double wetW   = bfxTotalW - labelW - editW - 4 * 2;
		if (wetW < 40) wetW = 40;
		beatFxWetLabel .setBounds(xOffset,                              bfxRow2Y, labelW, cellHeight - 4);
		beatFxWetSlider.setBounds(xOffset + labelW + 4,                 bfxRow2Y, wetW,   cellHeight - 4);
		beatFxEditButton.setBounds(xOffset + labelW + 4 + wetW + 4,     bfxRow2Y, editW,  cellHeight - 4);
	}

	// ----- Release FX layout: 3 tiles single row. -----
	{
		double rfxTotalW = cellLength * 3;
		double rfxCellW  = (rfxTotalW - 4) / 3;
		double rfxH      = cellHeight * 2 - 4;
		double rfxY      = yOffset + 4;
		for (int idx = 0; idx < (int) releaseFxTiles.size(); ++idx) {
			releaseFxTiles[idx]->setBounds(
				xOffset + idx * rfxCellW,
				rfxY,
				rfxCellW - 4,
				rfxH);
		}
	}

	lowBandFilter.setBounds(xOffset, rowH * 5.8, 50, 50);
	midBandFilter.setBounds(xOffset + getWidth() / 5, rowH * 5.8, 50, 50);
	highBandFilter.setBounds(xOffset + getWidth() * 2 / 5, rowH * 5.8, 50, 50);
	lbLabel.setBounds(xOffset, rowH * 6.9, 50, 50);
	mbLabel.setBounds(xOffset + getWidth() / 5, rowH * 6.9, 50, 50);
	hbLabel.setBounds(xOffset + getWidth() * 2 / 5, rowH * 6.9, 50, 50);

	// Compact queue widget — placed in the bottom-right corner of the deck
	// strip (next to the load/play column).
	if (queueWidget)
	{
		const int qW = 130;
		const int qH = (int) (rowH * 2.2);
		const int qX = (int) (mainXOffset + getWidth() * 22.5 / 32 - qW - 6);
		const int qY = (int) (rowH * 6.0);
		queueWidget->setBounds(qX, qY, qW, qH);
	}
}

//============================================================================== 

/**
 * Implementation of buttonClicked method for DeckGUI
 *
 * All juce::Button data members are compared to the triggered juce::Button pointer.
 * Based on which juce::Button data member it is, calls either a specific method in
 * the DJAudioPlayer instance, its own methods or component methods.
 *
 */
void DeckGUI::buttonClicked(juce::Button* button) {

	if (button == &playButton) {
		DBG("MainComponent::buttonClicked: They clicked the play button");
		double interval = getQuantizeIntervalSecs();
		if (interval > 0.0 && player->isLoaded()) {
			// Cancel if play button already pending
			if (pendingAction.isValid() && pendingAction.srcButton == &playButton) {
				clearPendingAction();
				// Restore toggle state (button auto-toggled, undo it)
				playButton.setToggleState(modeIsPlaying, juce::NotificationType::dontSendNotification);
				return;
			}
			// Arm pending play/stop
			bool intendToStart = !modeIsPlaying; // what user wants after toggle
			auto ptype = intendToStart ? PendingQuantizeAction::Type::PlayStart
			                           : PendingQuantizeAction::Type::PlayStop;

			if (pendingAction.isValid())
				clearPendingAction();

			double currentSecs = player->getPositionRelative() * player->getLengthInSeconds();
			double nextBoundary = getNextQuantizeBoundarySecs(currentSecs);
			double sr = juce::jmax(0.0001, player->getSpeedRatio());
			double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
			double fireAt = now + (nextBoundary - currentSecs) / sr;

			pendingAction.type = ptype;
			pendingAction.fireAtRealTime = fireAt;
			pendingAction.srcButton = &playButton;
			playButton.setColour(juce::DrawableButton::backgroundColourId,
				juce::Colours::orange.withAlpha(0.7f));
			// Undo the auto-toggle — keep current state until fire
			playButton.setToggleState(modeIsPlaying, juce::NotificationType::dontSendNotification);
			return;
		}
		// Non-quantized: immediate
		modeIsPlaying = !modeIsPlaying;
		playButton.setButtonStyle(juce::DrawableButton::ButtonStyle::ImageFitted);
		if (modeIsPlaying)
			player->start();
		else
			player->stop();
		return;
	}

	if (button == &loadButton) {
		// Open the per-deck library sidebar (wired by MainComponent). Falls
		// back to the legacy "load currently selected library row" behaviour
		// only when no host has installed a callback (e.g. unit tests).
		if (onLoadButtonClicked)
			onLoadButtonClicked(deckIndex);
		else if (library != nullptr && library->selectionIsValid())
			loadDeck(library->getSelectedTrack());
	}

	// Tab switching
	if (button == &cueTabButton) {
		cueGridMode = CueGridMode::HotCues;
		cueTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		gridTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		jumpTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		loopTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		syncTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		setCueButtonsVisible(true);
		setGridControlsVisible(false);
		setBeatJumpControlsVisible(false);
		setLoopControlsVisible(false);
		setQuantizeControlsVisible(false);
		setSyncControlsVisible(false);
		setPadFxControlsVisible(false);
		setBeatFxControlsVisible(false);
		setReleaseFxControlsVisible(false);
	}

	if (button == &gridTabButton) {
		cueGridMode = CueGridMode::BeatGrid;
		gridTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		cueTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		jumpTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		loopTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		syncTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		setCueButtonsVisible(false);
		setGridControlsVisible(true);
		setBeatJumpControlsVisible(false);
		setLoopControlsVisible(false);
		setQuantizeControlsVisible(false);
		setSyncControlsVisible(false);
		setPadFxControlsVisible(false);
		setBeatFxControlsVisible(false);
		setReleaseFxControlsVisible(false);
		updateGridBpmDisplay();
	}

	if (button == &jumpTabButton) {
		cueGridMode = CueGridMode::BeatJump;
		jumpTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		cueTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		gridTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		loopTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		syncTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		setCueButtonsVisible(false);
		setGridControlsVisible(false);
		setBeatJumpControlsVisible(true);
		setLoopControlsVisible(false);
		setQuantizeControlsVisible(false);
		setSyncControlsVisible(false);
		setPadFxControlsVisible(false);
		setBeatFxControlsVisible(false);
		setReleaseFxControlsVisible(false);
	}

	if (button == &loopTabButton) {
		cueGridMode = CueGridMode::Loop;
		loopTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		cueTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		gridTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		jumpTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		syncTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		setCueButtonsVisible(false);
		setGridControlsVisible(false);
		setBeatJumpControlsVisible(false);
		setLoopControlsVisible(true);
		setQuantizeControlsVisible(false);
		setSyncControlsVisible(false);
		setPadFxControlsVisible(false);
		setBeatFxControlsVisible(false);
		setReleaseFxControlsVisible(false);
	}

	if (button == &quantizeTabButton) {
		cueGridMode = CueGridMode::Quantize;
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		cueTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		gridTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		jumpTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		loopTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		syncTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		setCueButtonsVisible(false);
		setGridControlsVisible(false);
		setBeatJumpControlsVisible(false);
		setLoopControlsVisible(false);
		setQuantizeControlsVisible(true);
		setSyncControlsVisible(false);
		setPadFxControlsVisible(false);
		setBeatFxControlsVisible(false);
		setReleaseFxControlsVisible(false);
	}

	if (button == &syncTabButton) {
		cueGridMode = CueGridMode::Sync;
		syncTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		cueTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		gridTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		jumpTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		loopTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		setCueButtonsVisible(false);
		setGridControlsVisible(false);
		setBeatJumpControlsVisible(false);
		setLoopControlsVisible(false);
		setQuantizeControlsVisible(false);
		setSyncControlsVisible(true);
		setPadFxControlsVisible(false);
		setBeatFxControlsVisible(false);
		setReleaseFxControlsVisible(false);
		syncStateChanged();
	}

	// FX tab handlers ----------------------------------------------------------
	auto dimAllNonFxTabs = [this]() {
		cueTabButton     .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		gridTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		jumpTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		loopTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		syncTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
	};
	auto hideAllPanels = [this]() {
		setCueButtonsVisible(false);
		setGridControlsVisible(false);
		setBeatJumpControlsVisible(false);
		setLoopControlsVisible(false);
		setQuantizeControlsVisible(false);
		setSyncControlsVisible(false);
		setPadFxControlsVisible(false);
		setBeatFxControlsVisible(false);
		setReleaseFxControlsVisible(false);
	};

	if (button == &padFxTabButton) {
		cueGridMode = CueGridMode::PadFx;
		dimAllNonFxTabs();
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		hideAllPanels();
		setPadFxControlsVisible(true);
		refreshFxUi();
	}

	if (button == &beatFxTabButton) {
		cueGridMode = CueGridMode::BeatFx;
		dimAllNonFxTabs();
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		hideAllPanels();
		setBeatFxControlsVisible(true);
		refreshFxUi();
	}

	if (button == &releaseFxTabButton) {
		cueGridMode = CueGridMode::ReleaseFx;
		dimAllNonFxTabs();
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		hideAllPanels();
		setReleaseFxControlsVisible(true);
		refreshFxUi();
	}

	if (button == &beatFxOnButton) {
		postFxEngaged(FxCategory::Beat, beatFxOnButton.getToggleState());
	}

	if (button == &beatFxEditButton) {
		showFxParameterModal(FxCategory::Beat, &beatFxEditButton);
	}

	// Sync controls
	if (syncManager != nullptr) {
		if (button == &masterToggleBtn) {
			if (masterToggleBtn.getToggleState())
				syncManager->setMaster(deckIndex == 0 ? BeatSyncManager::MasterDeck::Deck1 : BeatSyncManager::MasterDeck::Deck2);
			else
				syncManager->setMaster(BeatSyncManager::MasterDeck::None);
		}
		if (button == &syncEngageBtn) {
			if (syncEngageBtn.getToggleState()) {
				if (!syncManager->engageSync(deckIndex))
					syncEngageBtn.setToggleState(false, juce::dontSendNotification);
			}
			else {
				syncManager->disengageSync(deckIndex);
			}
		}
		if (button == &fastSyncBtn) {
			if (fastSyncBtn.getToggleState()) {
				if (!syncManager->engageSync(deckIndex))
					fastSyncBtn.setToggleState(false, juce::dontSendNotification);
			}
			else {
				syncManager->disengageSync(deckIndex);
			}
		}
		if (button == &multHalfBtn) syncManager->setSlaveMultiplier(deckIndex, 0.5);
		if (button == &multOneBtn)  syncManager->setSlaveMultiplier(deckIndex, 1.0);
		if (button == &multTwoBtn)  syncManager->setSlaveMultiplier(deckIndex, 2.0);
		if (button == &syncResetBtn) {
			// Restore everything to default: disengage sync, clear master if
			// this deck was master, reset multiplier, and slam speed to 1.0.
			syncManager->disengageSync(deckIndex);
			if (syncManager->isMaster(deckIndex))
				syncManager->setMaster(BeatSyncManager::MasterDeck::None);
			syncManager->setSlaveMultiplier(deckIndex, 1.0);
			speedSlider.setValue(1.0, juce::sendNotification);
		}
	}

	// Beat jump buttons (quantized)
	if (button == &jumpBackward16Btn) queueOrExecute(PendingQuantizeAction::Type::BeatJump, &jumpBackward16Btn, -16);
	if (button == &jumpBackward8Btn)  queueOrExecute(PendingQuantizeAction::Type::BeatJump, &jumpBackward8Btn, -8);
	if (button == &jumpBackward4Btn)  queueOrExecute(PendingQuantizeAction::Type::BeatJump, &jumpBackward4Btn, -4);
	if (button == &jumpBackward1Btn)  queueOrExecute(PendingQuantizeAction::Type::BeatJump, &jumpBackward1Btn, -1);
	if (button == &jumpForward1Btn)   queueOrExecute(PendingQuantizeAction::Type::BeatJump, &jumpForward1Btn, 1);
	if (button == &jumpForward4Btn)   queueOrExecute(PendingQuantizeAction::Type::BeatJump, &jumpForward4Btn, 4);
	if (button == &jumpForward8Btn)   queueOrExecute(PendingQuantizeAction::Type::BeatJump, &jumpForward8Btn, 8);
	if (button == &jumpForward16Btn)  queueOrExecute(PendingQuantizeAction::Type::BeatJump, &jumpForward16Btn, 16);

	// Loop buttons (quantized except reloop and clear)
	if (button == &loopInBtn)     queueOrExecute(PendingQuantizeAction::Type::LoopIn, &loopInBtn);
	if (button == &loopOutBtn)    queueOrExecute(PendingQuantizeAction::Type::LoopOut, &loopOutBtn);
	if (button == &reloopBtn)     player->toggleReloop();
	if (button == &loopHalveBtn)  queueOrExecute(PendingQuantizeAction::Type::LoopHalve, &loopHalveBtn);
	if (button == &loopDoubleBtn) queueOrExecute(PendingQuantizeAction::Type::LoopDouble, &loopDoubleBtn);
	if (button == &loopClearBtn)  player->clearLoop();

	// Grid control buttons
	if (button == &gridNudgeLeftBtn) {
		BeatGrid grid = player->getBeatGrid();
		grid.gridOffsetSecs -= 0.01;
		grid.isManualOffset = true;
		player->setBeatGrid(grid);
		saveTrackData(grid);
	}

	if (button == &gridNudgeRightBtn) {
		BeatGrid grid = player->getBeatGrid();
		grid.gridOffsetSecs += 0.01;
		grid.isManualOffset = true;
		player->setBeatGrid(grid);
		saveTrackData(grid);
	}

	if (button == &tapTempoBtn) {
		double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
		if (!tapTimes.empty() && (now - tapTimes.back()) > 3.0)
			tapTimes.clear(); // Reset if too long between taps

		tapTimes.push_back(now);

		if (tapTimes.size() >= 2) {
			// Average the last 8 intervals (or fewer if not enough taps)
			size_t count = std::min(tapTimes.size() - 1, static_cast<size_t>(8));
			double totalInterval = tapTimes.back() - tapTimes[tapTimes.size() - 1 - count];
			double avgInterval = totalInterval / static_cast<double>(count);
			double tapBpm = 60.0 / avgInterval;

			if (tapBpm > 20.0 && tapBpm < 300.0) {
				BeatGrid grid = player->getBeatGrid();
				grid.bpm = std::round(tapBpm * 10.0) / 10.0;
				grid.isManualBpm = true;
				player->setBeatGrid(grid);
				updateGridBpmDisplay();
				saveTrackData(grid);
			}
		}
	}

	if (button == &gridResetBtn) {
		double detectedBpm = player->getDetectedBpm();
		BeatGrid grid;
		grid.bpm = detectedBpm;
		player->setBeatGrid(grid);
		updateGridBpmDisplay();
		saveTrackData(grid);
	}

	// Hot cue buttons (quantized for set and jump)
	if (player->isLoaded()) {
		for (auto& cue : cues) {
			juce::TextButton* thisButton = cue;
			if (button == thisButton) {
				auto clickPos = juce::Desktop::getInstance().getMainMouseSource().getLastMouseDownPosition();
				auto btnScreenBounds = thisButton->getScreenBounds();
				auto localClick = clickPos - btnScreenBounds.getPosition().toFloat();

				// Check if "x" area was clicked (top-right 14x14)
				bool xClicked = cueTargets.find(thisButton) != cueTargets.end() &&
					localClick.getX() > thisButton->getWidth() - 14 &&
					localClick.getY() < 14;

				if (xClicked) {
					// Remove is always immediate
					cueTargets.erase(thisButton);
					waveformDisplay.setCuePoints(cueTargets);
					zoomedDisplay->setCuePoints(cueTargets);
				}
				else if (cueTargets.find(thisButton) != cueTargets.end()) {
					// Jump to cue (quantized)
					queueOrExecute(PendingQuantizeAction::Type::HotCueJump, thisButton,
						0, cueTargets[thisButton].first, -1.0, 0.0f, thisButton);
				}
				else {
					// Set cue (quantized) — capture position now
					float hue = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
					queueOrExecute(PendingQuantizeAction::Type::HotCueSet, thisButton,
						0, -1.0, player->getPositionRelative(), hue, thisButton);
				}
			}
		}
	}
};

//============================================================================== 

/**
 * Implementation of mouseDown method for DeckGUI
 *
 * Handles right-click context menu on cue buttons for set/jump/remove actions.
 */
void DeckGUI::mouseDown(const juce::MouseEvent& event) {
	if (!event.mods.isPopupMenu())
		return;

	auto* source = event.eventComponent;

	if (source == &speedSlider) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to 0%");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1) {
					speedSlider.setValue(1.0, juce::sendNotification);
				}
			});
		return;
	}

	if (!player->isLoaded())
		return;

	for (auto& cue : cues) {
		if (source == cue) {
			bool hasCue = cueTargets.find(cue) != cueTargets.end();

			juce::PopupMenu menu;
			menu.addItem(1, "Set Cue Here");
			menu.addItem(2, "Jump to Cue", hasCue);
			menu.addItem(3, "Remove Cue", hasCue);

			menu.showMenuAsync(juce::PopupMenu::Options(),
				[this, cue](int result) {
					if (result == 1) {
						float hue = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
						queueOrExecute(PendingQuantizeAction::Type::HotCueSet, cue,
							0, -1.0, player->getPositionRelative(), hue, cue);
					}
					else if (result == 2) {
						queueOrExecute(PendingQuantizeAction::Type::HotCueJump, cue,
							0, cueTargets[cue].first, -1.0, 0.0f, cue);
					}
					else if (result == 3) {
						cueTargets.erase(cue);
						waveformDisplay.setCuePoints(cueTargets);
						zoomedDisplay->setCuePoints(cueTargets);
					}
				});
			break;
		}
	}
}

//============================================================================== 

/**
 * Implementation of sliderValueChanged method for DeckGUI
 *
 * All juce::Slider data members are compared to the triggered juce::Slider pointer.
 * Based on which juce::Slider data member it is, calls a specific method in the
 * DJAudioPlayer instance.
 *
 */
void DeckGUI::sliderValueChanged(juce::Slider* slider) {

	if (slider == &volSlider) {
		DBG("MainComponent::sliderValueChanged: They change the volume slider " << slider->getValue());
		player->setGain(slider->getValue());
	}

	if (slider == &speedSlider) {
		DBG("MainComponent::sliderValueChanged: They change the speed slider " << slider->getValue());
		// Capture the user's intended value before any side-effects can overwrite it.
		double userSpeed = slider->getValue();
		// User-initiated speed change while synced — break sync. (Programmatic
		// updates from BeatSyncManager use dontSendNotification, so this path
		// only triggers from real user input or the right-click reset.)
		if (syncManager != nullptr && syncManager->isSynced(deckIndex)) {
			syncManager->disengageSync(deckIndex);
			// syncStateChanged() stamped the OLD sync ratio onto the slider;
			// restore the user's intended value now that the player speed is correct.
			if (std::abs(speedSlider.getValue() - userSpeed) > 1e-4)
				speedSlider.setValue(userSpeed, juce::dontSendNotification);
		}
		player->setSpeed(userSpeed);
		// Immediately propagate new speed to waveform displays for live visual feedback
		const BeatGrid& grid = player->getBeatGrid();
		for (auto* display : displays)
			display->setBeatGrid(grid.bpm, grid.gridOffsetSecs, userSpeed);
	}

	if (slider == &filter) {
		DBG("MainComponent::sliderValueChanged: They change the filter slider " << slider->getValue());
		player->setFilter(slider->getValue());
	}

	if (slider == &lowBandFilter) {
		DBG("MainComponent::sliderValueChanged: They change the LB slider " << slider->getValue());
		player->setLBFilter(slider->getValue());
	}

	if (slider == &midBandFilter) {
		DBG("MainComponent::sliderValueChanged: They change the MB slider " << slider->getValue());
		player->setMBFilter(slider->getValue());
	}

	if (slider == &highBandFilter) {
		DBG("MainComponent::sliderValueChanged: They change the HB slider " << slider->getValue());
		player->setHBFilter(slider->getValue());
	}
};

//============================================================================== 

/**
 * Implementation of isInterestedInFileDrag method for DeckGUI
 *
 * returns true
 *
 */
bool DeckGUI::isInterestedInFileDrag(const juce::StringArray& files) {
	return true;
};


/**
 * Implementation of filesDropped method for DeckGUI
 *
 * Checks if the files array is of size 1. Converts the files element
 * into a juce::File object, into a track object before loading the deck
 * with the track object.
 *
 */
void DeckGUI::filesDropped(const juce::StringArray& files, int x, int y) {
	DBG("DeckGUI::filesDropped");
	if (files.size() == 1 && x < getWidth() && y < getHeight()) {
		juce::File file = juce::File{ files[0] };
		// Hash on a worker thread to avoid blocking the message thread on disk I/O.
		juce::Component::SafePointer<DeckGUI> safeSelf(this);
		FileHasher::computeHashAsync(file, [safeSelf, file](juce::String hash) {
			if (auto* self = safeSelf.getComponent()) {
				track track{ file.getFileNameWithoutExtension(), 0, juce::URL{ file } };
				track.fileHash = hash;
				self->loadDeck(track);
			}
		});
	}
};

//============================================================================== 

/**
 * Implementation of timerCallback method for DeckGUI
 *
 * Continuously update any WaveformDisplay objects from the player's position.
 * Check if any WaveformDisplay objects' playback control is triggered, and setting
 * the DJAudioPlayer instance's playback with the triggered playback control value.
 * This is also where the DJAudioPlayer instance's root mean square value is updated.
 *
 */
void DeckGUI::timerCallback() {
	counter++;
	if (counter % 10 == 0) {
		flash = !flash;
		repaint();
	}

	for (auto i = 0; i < displays.size(); ++i) {
		if (displays[i]->isFileLoaded()) {
			double pos = displays[i]->getValue();
			if (displays[i]->isSliderDragged()) {
				draggedIndex = i;
				canContinue = false;
				// User scrubbing the waveform breaks sync.
				if (syncManager != nullptr && syncManager->isSynced(deckIndex))
					syncManager->disengageSync(deckIndex);
				if (displays[i] == &waveformDisplay) {
					player->stop();
				}
				else {
					if (prevPlayerPos == pos) {
						player->stop();
					}
					else {
						if (!player->isPlaying())
							player->start();
					}
				}
				player->setPositionRelative(pos);
				prevPlayerPos = pos;
			}
			else if (canContinue == false && !(displays[i]->isSliderDragged()) && draggedIndex == i) {
				DBG("YESSSS " << (displays[i]->isSliderDragged() ? "true" : "false"));
				if (modeIsPlaying)
					player->start();
				else
					player->stop();
				canContinue = true;
				draggedIndex = -1;
			}
			else {
				displays[i]->setPositionRelative(player->getPositionRelative());
			}
		}
	}

	if (volRMS != player->getRMSLevel()) {
		volRMS = player->getRMSLevel();
		repaint();
	}

	// Update BPM display
	double currentBpm = player->getCurrentBpm();
	if (currentBpm > 0.0) {
		bpmValueLabel.setText(juce::String(currentBpm, 1), juce::dontSendNotification);
	}
	else {
		bpmValueLabel.setText("---", juce::dontSendNotification);
	}

	// Push BPM into the FX chain (drives beat-synced effect timings).
	// Only post when it actually changes to avoid spamming the FIFO.
	if (std::abs(currentBpm - lastFxBpmPushed) > 0.05) {
		AudioCommand bpmCmd;
		bpmCmd.tag = AudioCommand::Tag::FxSetBpm;
		bpmCmd.doublePayload = currentBpm;
		if (player->postCommand(bpmCmd))
			lastFxBpmPushed = currentBpm;
	}

	double speedRatio = player->getSpeedRatio();
	if (std::abs(speedRatio - 1.0) > 0.001 && currentBpm > 0.0) {
		double pct = (speedRatio - 1.0) * 100.0;
		juce::String sign = pct > 0 ? "+" : "";
		bpmPercentLabel.setText(sign + juce::String(pct, 1) + "%", juce::dontSendNotification);
	}
	else {
		bpmPercentLabel.setText("", juce::dontSendNotification);
	}

	// Update beat grid data on waveform displays
	const BeatGrid& grid = player->getBeatGrid();
	for (auto* display : displays) {
		display->setBeatGrid(grid.bpm, grid.gridOffsetSecs, speedRatio);
		display->setLoopRegion(player->getLoopInRelative(), player->getLoopOutRelative(), player->isLooping());
	}

	// Update loop button highlights
	bool inSet = player->getLoopInRelative() >= 0.0;
	bool loopOn = player->isLooping();
	if (!pendingAction.isValid() || pendingAction.srcButton != &loopInBtn)
		loopInBtn.setColour(juce::TextButton::buttonColourId,
			inSet ? juce::Colours::blue.withAlpha(0.7f) : juce::Colour::fromRGBA(25, 25, 25, 255));
	reloopBtn.setColour(juce::TextButton::buttonColourId,
		loopOn ? juce::Colours::limegreen.withAlpha(0.6f) : juce::Colour::fromRGBA(25, 25, 25, 255));

	// Fire pending quantize action if its time has arrived
	if (pendingAction.isValid()) {
		double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
		if (now >= pendingAction.fireAtRealTime)
			executePendingAction();
	}

	// Auto-advance: if the deck was playing and just finished a track, pop the
	// queue and load the next entry. Heuristic: position has hit ~end and the
	// transport stopped on its own. Keep modeIsPlaying = true so finishLoadDeck
	// auto-plays the next track.
	if (queueWidget != nullptr && !queueWidget->isEmpty() && modeIsPlaying) {
		double pos = player->getPositionRelative();
		if (pos >= 0.9995 && !player->isPlaying()) {
			auto next = queueWidget->popFront();
			if (next.url.toString(false).isNotEmpty())
				loadDeck(next);
		}
	}
}

//============================================================================== 

/**
 * Implementation of loadDeck method for DeckGUI
 *
 * Loads the player with the track object.
 * Loads all WaveformDisplay objects with the track object.
 * Cue point data from previously loaded tracks are cleared.
 *
 */
void DeckGUI::loadDeck(track track) {
	clearPendingAction();

	// Notify sync manager BEFORE loading so any active sync involving this
	// deck (or this deck as master) is disengaged cleanly first.
	if (syncManager != nullptr)
		syncManager->onTrackLoaded(deckIndex);

	// Stash the track — finishLoadDeck() needs it once loading completes.
	pendingTrack = track;

	if (audioEngine != nullptr) {
		// Asynchronous path: requestLoad returns immediately; the listener
		// callback (deckLoadingStateChanged) drives the loading overlay, and
		// the completion lambda invokes finishLoadDeck() on the message thread.
		audioEngine->requestLoad(deckIndex, track.url, [this](bool success) {
			if (success)
				finishLoadDeck();
		});
	}
	else {
		// Legacy synchronous path (shouldn't be hit when MainComponent wires
		// the engine, but kept for safety / future test harnesses).
		player->loadURL(track.url);
		if (player->isLoaded())
			finishLoadDeck();
	}
};

//==============================================================================

/**
 * Implementation of finishLoadDeck method for DeckGUI
 *
 * Final post-load setup that depends on the audio source being ready:
 * thumbnails, default gain, hot-cue clear, BPM cache lookup, autoplay.
 * Always runs on the message thread.
 */
void DeckGUI::finishLoadDeck() {
	if (! player->isLoaded())
		return;

	track& t = pendingTrack;
	for (auto& display : displays) {
		display->loadTrack(t);
		display->addListener(this);
	}

	player->setGain(volSlider.getValue(), true);
	cueTargets.clear();

	// Load beat grid config for this track on a worker thread to avoid
	// blocking the message thread on JSON disk I/O.
	currentTrackIdentity = t.identity;
	currentFileHash = t.fileHash;
	if (currentFileHash.isNotEmpty()) {
		juce::Component::SafePointer<DeckGUI> safeSelf(this);
		const juce::String requestedHash = currentFileHash;
		TrackDataCache::loadAsync(currentFileHash, [safeSelf, requestedHash](TrackData cached) {
			auto* self = safeSelf.getComponent();
			if (self == nullptr) return;
			// Stale-result guard: another track may have been loaded in the meantime.
			if (self->currentFileHash != requestedHash) return;
			if (cached.beatGrid.bpm > 0.0) {
				self->player->setBeatGrid(cached.beatGrid);
			} else if (cached.detectedBpm > 0.0) {
				BeatGrid grid;
				grid.bpm = cached.detectedBpm;
				self->player->setBeatGrid(grid);
			}
			self->updateGridBpmDisplay();
		});
	}
	updateGridBpmDisplay();

	if (modeIsPlaying) {
		playButton.setToggleState(true, juce::NotificationType::dontSendNotification);
		player->start();
	}
	else {
		playButton.setToggleState(false, juce::NotificationType::dontSendNotification);
	}
}

//==============================================================================

/**
 * Implementation of deckLoadingStateChanged method for DeckGUI
 *
 * AudioEngine listener callback. Drives the "Loading…" overlay and disables
 * the play / load buttons while the deck is busy loading a track.
 */
void DeckGUI::deckLoadingStateChanged(int deckIdx, DJAudioPlayer::LoadingState newState) {
	if (deckIdx != deckIndex)
		return;

	const bool busy = (newState == DJAudioPlayer::LoadingState::Loading);
	playButton.setEnabled(! busy);
	loadButton.setEnabled(! busy);
	for (auto* cue : cues)
		cue->setEnabled(! busy);

	loadingLabel.setVisible(busy);
	loadingLabel.toFront(false);

	if (newState == DJAudioPlayer::LoadingState::Failed)
		loadingLabel.setText("Load failed", juce::dontSendNotification);
	else if (busy)
		loadingLabel.setText("Loading…", juce::dontSendNotification);

	repaint();
}

//==============================================================================

/**
 * Implementation of saveTrackData method for DeckGUI
 *
 * Persists the given beat grid along with the detected BPM to the
 * track data cache, keyed by the current file hash.
 */
void DeckGUI::saveTrackData(const BeatGrid& grid) {
	if (currentFileHash.isEmpty())
		return;
	// Snapshot the detected BPM on the message thread (player getter is
	// thread-safe but we want a value bound to *this* save), then dispatch
	// the read-modify-write to a single-worker pool that serializes updates.
	const double detectedBpm = player->getDetectedBpm();
	TrackDataCache::updateAsync(currentFileHash, [grid, detectedBpm](TrackData& data) {
		data.beatGrid = grid;
		data.detectedBpm = detectedBpm;
	});
}

//==============================================================================

/**
 * Implementation of setCueButtonsVisible method for DeckGUI
 *
 * Shows or hides all cue buttons.
 */
void DeckGUI::setCueButtonsVisible(bool visible) {
	for (auto& cue : cues)
		cue->setVisible(visible);
}

/**
 * Implementation of setGridControlsVisible method for DeckGUI
 *
 * Shows or hides all beat grid control components.
 */
void DeckGUI::setGridControlsVisible(bool visible) {
	gridBpmLabel.setVisible(visible);
	gridBpmEditor.setVisible(visible);
	gridNudgeLeftBtn.setVisible(visible);
	gridNudgeRightBtn.setVisible(visible);
	gridOffsetLabel.setVisible(visible);
	tapTempoBtn.setVisible(visible);
	gridResetBtn.setVisible(visible);
}

/**
 * Implementation of updateGridBpmDisplay method for DeckGUI
 *
 * Updates the grid BPM editor text from the player's current beat grid.
 */
void DeckGUI::updateGridBpmDisplay() {
	double bpm = player->getBeatGrid().bpm;
	if (bpm > 0.0)
		gridBpmEditor.setText(juce::String(bpm, 1), false);
	else
		gridBpmEditor.setText("", false);
}

//==============================================================================

/**
 * Implementation of setBeatJumpControlsVisible method for DeckGUI
 *
 * Shows or hides all beat jump control components.
 */
void DeckGUI::setBeatJumpControlsVisible(bool visible) {
	jumpBackward16Btn.setVisible(visible);
	jumpBackward8Btn.setVisible(visible);
	jumpBackward4Btn.setVisible(visible);
	jumpBackward1Btn.setVisible(visible);
	jumpForward1Btn.setVisible(visible);
	jumpForward4Btn.setVisible(visible);
	jumpForward8Btn.setVisible(visible);
	jumpForward16Btn.setVisible(visible);
	jumpLabel.setVisible(visible);
}

//==============================================================================

/**
 * Implementation of setLoopControlsVisible method for DeckGUI
 *
 * Shows or hides all loop control components.
 */
void DeckGUI::setLoopControlsVisible(bool visible) {
	loopInBtn.setVisible(visible);
	loopOutBtn.setVisible(visible);
	reloopBtn.setVisible(visible);
	loopHalveBtn.setVisible(visible);
	loopDoubleBtn.setVisible(visible);
	loopClearBtn.setVisible(visible);
}

//==============================================================================

/**
 * Implementation of setQuantizeControlsVisible method for DeckGUI
 *
 * Shows or hides the quantize label and combo box.
 */
void DeckGUI::setQuantizeControlsVisible(bool visible) {
	quantizeLabel.setVisible(visible);
	quantizeComboBox.setVisible(visible);
}

//==============================================================================

/**
 * Implementation of setSyncControlsVisible method for DeckGUI
 *
 * Shows or hides the sync tab controls.
 */
void DeckGUI::setSyncControlsVisible(bool visible) {
	masterToggleBtn.setVisible(visible);
	syncEngageBtn.setVisible(visible);
	multHalfBtn.setVisible(visible);
	multOneBtn.setVisible(visible);
	multTwoBtn.setVisible(visible);
	targetBpmLabel.setVisible(visible);
	syncStatusLabel.setVisible(visible);
	snapBox.setVisible(visible);
	syncResetBtn.setVisible(visible);
}

//==============================================================================

/**
 * Implementation of syncStateChanged method for DeckGUI
 *
 * Called by BeatSyncManager when sync state changes. Refreshes UI controls
 * (button toggle states, target BPM, status label) and locks/unlocks the
 * speed slider for slaves.
 */
void DeckGUI::syncStateChanged() {
	if (syncManager == nullptr) return;

	bool isMaster = syncManager->isMaster(deckIndex);
	bool isSynced = syncManager->isSynced(deckIndex);
	juce::String status = syncManager->getStatus(deckIndex);
	bool outOfRange = (status == "OUT OF RANGE");

	// Toggle states (without firing handlers).
	masterToggleBtn.setToggleState(isMaster, juce::dontSendNotification);
	syncEngageBtn.setToggleState(isSynced && !outOfRange, juce::dontSendNotification);
	fastSyncBtn.setToggleState(isSynced && !outOfRange, juce::dontSendNotification);

	// Status-driven tint on the engage + fast-sync buttons.
	juce::Colour offColour = juce::Colour::fromRGBA(25, 25, 25, 255);
	if (isSynced && outOfRange)
		offColour = juce::Colours::orange.withAlpha(0.6f);
	syncEngageBtn.setColour(juce::TextButton::buttonColourId, offColour);
	fastSyncBtn.setColour  (juce::TextButton::buttonColourId, offColour);

	// Target BPM display.
	double tgt = syncManager->getTargetBpm(deckIndex);
	if (isSynced && tgt > 0.0)
		targetBpmLabel.setText(juce::String(juce::CharPointer_UTF8("\xe2\x86\x92 ")) + juce::String(tgt, 1), juce::dontSendNotification);
	else
		targetBpmLabel.setText(juce::String(juce::CharPointer_UTF8("\xe2\x86\x92 ---")), juce::dontSendNotification);

	// Status label.
	syncStatusLabel.setText(status, juce::dontSendNotification);

	// Lock slave's speed slider while synced; master and disengaged decks
	// retain manual control. The actual sync ratio may exceed the slider's
	// visual range; we don't try to mirror it on the slider — targetBpmLabel
	// shows the resolved BPM instead.
	// Keep the slider enabled and mirror the engine ratio onto it (slider
	// auto-clamps to its visual range). Any user interaction will fire
	// sliderValueChanged, where we then disengage sync — so the slider acts
	// as a quick-disengage handle while still showing the synced position.
	if (!speedSlider.isEnabled())
		speedSlider.setEnabled(true);

	double sr = player->getSpeedRatio();
	if (std::abs(speedSlider.getValue() - sr) > 1e-4)
		speedSlider.setValue(sr, juce::dontSendNotification);

	// Master can't simultaneously be a slave: disable the SYNC engage button.
	syncEngageBtn.setEnabled(!isMaster);
	fastSyncBtn.setEnabled(!isMaster);

	// Reflect manager's snap selection in the combo box.
	int sb = syncManager->getSnapBeats();
	int wantId = (sb == 4 ? 1 : sb == 2 ? 2 : 3);
	if (snapBox.getSelectedId() != wantId)
		snapBox.setSelectedId(wantId, juce::dontSendNotification);

	repaint();
}

//==============================================================================

/**
 * Implementation of propagateSpeedToDisplays method for DeckGUI
 *
 * Pushes the current speed ratio to all waveform displays so they visually
 * stretch/squish to reflect the post-sync tempo.
 */
void DeckGUI::propagateSpeedToDisplays() {
	const BeatGrid& grid = player->getBeatGrid();
	double sr = player->getSpeedRatio();
	for (auto* display : displays)
		display->setBeatGrid(grid.bpm, grid.gridOffsetSecs, sr);
}

//==============================================================================

/**
 * Implementation of getQuantizeIntervalSecs method for DeckGUI
 *
 * Maps the quantize ComboBox selection to a duration in seconds
 * based on the current beat grid BPM. Returns 0.0 when quantize
 * is disabled or no BPM is available.
 */
double DeckGUI::getQuantizeIntervalSecs() const {
	double bpm = player->getBeatGrid().bpm;
	if (bpm <= 0.0)
		return 0.0;

	double beatSecs = 60.0 / bpm;
	int id = quantizeComboBox.getSelectedId();

	switch (id) {
		case 2:  return 4.0 * beatSecs;             // 1 Bar
		case 3:  return 2.0 * beatSecs;             // 1/2 Bar
		case 4:  return (4.0 / 3.0) * beatSecs;    // 1/3 Bar
		case 5:  return beatSecs;                    // 1/4 Bar (1 beat)
		case 6:  return (4.0 / 5.0) * beatSecs;    // 1/5 Bar
		case 7:  return (4.0 / 6.0) * beatSecs;    // 1/6 Bar
		case 8:  return (4.0 / 7.0) * beatSecs;    // 1/7 Bar
		case 9:  return 0.5 * beatSecs;             // 1/8 Bar
		case 10: return (4.0 / 9.0) * beatSecs;    // 1/9 Bar
		case 11: return (4.0 / 32.0) * beatSecs;   // 1/32 Bar
		default: return 0.0;                         // None (id 1) or unknown
	}
}

//==============================================================================

/**
 * Implementation of getNextQuantizeBoundarySecs method for DeckGUI
 *
 * Given the current playback position in seconds, returns the next
 * beat-grid-aligned quantize boundary in track time. If quantize is
 * off, returns the current position unchanged.
 */
double DeckGUI::getNextQuantizeBoundarySecs(double currentSecs) const {
	double interval = getQuantizeIntervalSecs();
	if (interval <= 0.0)
		return currentSecs;

	double offset = player->getBeatGrid().gridOffsetSecs;
	double intervals = (currentSecs - offset) / interval;
	double nextIdx = std::floor(intervals + 1e-9) + 1.0;
	return offset + nextIdx * interval;
}

//==============================================================================

/**
 * Implementation of clearPendingAction method for DeckGUI
 *
 * Reverts the visual state of the pending button and clears the action.
 */
void DeckGUI::clearPendingAction() {
	if (pendingAction.srcButton != nullptr) {
		if (pendingAction.srcButton == &playButton) {
			playButton.setColour(juce::DrawableButton::backgroundColourId,
				juce::Colours::transparentBlack);
		}
		else {
			pendingAction.srcButton->setColour(juce::TextButton::buttonColourId,
				juce::Colour::fromRGBA(25, 25, 25, 255));
		}
	}
	pendingAction.clear();
}

//==============================================================================

/**
 * Implementation of executePendingAction method for DeckGUI
 *
 * Dispatches the stored pending action to the appropriate player method,
 * then clears the pending state and reverts the button colour.
 */
void DeckGUI::executePendingAction() {
	auto action = pendingAction;
	clearPendingAction();

	switch (action.type) {
		case PendingQuantizeAction::Type::PlayStart:
			modeIsPlaying = true;
			playButton.setToggleState(true, juce::NotificationType::dontSendNotification);
			player->start();
			break;
		case PendingQuantizeAction::Type::PlayStop:
			modeIsPlaying = false;
			playButton.setToggleState(false, juce::NotificationType::dontSendNotification);
			player->stop();
			break;
		case PendingQuantizeAction::Type::LoopIn:
			player->setLoopIn();
			break;
		case PendingQuantizeAction::Type::LoopOut:
			player->setLoopOut();
			break;
		case PendingQuantizeAction::Type::LoopHalve:
			player->halveLoop();
			break;
		case PendingQuantizeAction::Type::LoopDouble:
			player->doubleLoop();
			break;
		case PendingQuantizeAction::Type::BeatJump:
			player->beatJump(action.beatJumpBeats);
			break;
		case PendingQuantizeAction::Type::HotCueJump:
			player->setPositionRelative(action.hotCueRelPos);
			if (!modeIsPlaying) {
				modeIsPlaying = true;
				playButton.setToggleState(true, juce::NotificationType::dontSendNotification);
				player->start();
			}
			break;
		case PendingQuantizeAction::Type::HotCueSet:
			if (action.cueButtonTarget != nullptr) {
				double setPos = player->getPositionRelative();
				cueTargets[action.cueButtonTarget] = std::make_pair(setPos, action.hotCueHue);
				waveformDisplay.setCuePoints(cueTargets);
				zoomedDisplay->setCuePoints(cueTargets);
			}
			break;
		default:
			break;
	}
}

//==============================================================================

/**
 * Implementation of queueOrExecute method for DeckGUI
 *
 * If quantize is active and the player is playing, calculates the next
 * quantize boundary and arms the action with an orange button highlight.
 * If the same button is already pending, cancels the pending action.
 * Otherwise executes the action immediately.
 */
void DeckGUI::queueOrExecute(PendingQuantizeAction::Type type, juce::Button* btn,
                              int beats, double hotCueRelPos,
                              double hotCueSetPos, float hotCueHue,
                              juce::TextButton* cueBtnTarget) {
	double interval = getQuantizeIntervalSecs();

	// Cancel if same button already pending
	if (pendingAction.isValid() && pendingAction.srcButton == btn) {
		clearPendingAction();
		return;
	}

	// Execute immediately if quantize is off or player isn't playing
	if (interval <= 0.0 || !player->isPlaying()) {
		// Clear any existing pending action first
		if (pendingAction.isValid())
			clearPendingAction();

		PendingQuantizeAction immediate;
		immediate.type = type;
		immediate.beatJumpBeats = beats;
		immediate.hotCueRelPos = hotCueRelPos;
		immediate.hotCueSetPos = hotCueSetPos;
		immediate.hotCueHue = hotCueHue;
		immediate.cueButtonTarget = cueBtnTarget;
		pendingAction = immediate;
		executePendingAction();
		return;
	}

	// Queue: calculate fire time
	if (pendingAction.isValid())
		clearPendingAction();

	double currentSecs = player->getPositionRelative() * player->getLengthInSeconds();
	double nextBoundary = getNextQuantizeBoundarySecs(currentSecs);
	double sr = juce::jmax(0.0001, player->getSpeedRatio());
	double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
	double fireAt = now + (nextBoundary - currentSecs) / sr;

	pendingAction.type = type;
	pendingAction.fireAtRealTime = fireAt;
	pendingAction.srcButton = btn;
	pendingAction.beatJumpBeats = beats;
	pendingAction.hotCueRelPos = hotCueRelPos;
	pendingAction.hotCueSetPos = hotCueSetPos;
	pendingAction.hotCueHue = hotCueHue;
	pendingAction.cueButtonTarget = cueBtnTarget;

	// Highlight button orange
	if (btn == &playButton) {
		playButton.setColour(juce::DrawableButton::backgroundColourId,
			juce::Colours::orange.withAlpha(0.7f));
	}
	else {
		btn->setColour(juce::TextButton::buttonColourId,
			juce::Colours::orange.withAlpha(0.8f));
	}
}

//==============================================================================
//
//                              FX panels (P.FX / B.FX / R.FX)
//
//==============================================================================

/**
 * Builds the Pad / Beat / Release FX UI controls and attaches their listeners.
 *
 * Pad FX  : 4×2 grid of momentary tiles (mouseDown engages, mouseUp releases).
 * Beat FX : drop-down + division box + ON/OFF toggle + WET slider + EDIT button.
 * Release FX : 3 momentary tiles (V.Brake, R.Echo, Back Spin).
 *
 * Tiles are added as child components but kept hidden initially — the HotCues
 * tab is the default selected tab.
 */
void DeckGUI::buildFxPanels()
{
	const auto themeMid = juce::Colour::fromRGBA(40, 40, 40, 255);

	// ---- Pad FX tiles --------------------------------------------------------
	{
		auto procs = FxFactory::buildCategory(FxCategory::Pad);
		// Tile 0 of FxFactory output is "None" — we skip it for the display
		// grid but keep its index inside the chain.
		const int kTiles = 8;
		padFxTiles.reserve(kTiles);
		for (int slot = 0; slot < kTiles; ++slot) {
			auto tile = std::make_unique<MomentaryFxTile>();
			int procIdx = slot + 1; // skip None at index 0
			juce::String label = "—";
			if (procIdx < (int) procs.size()) {
				label = procs[procIdx]->getName();
			}
			tile->setButtonText(label);
			tile->setColour(juce::TextButton::buttonColourId, themeMid);
			tile->setColour(juce::TextButton::buttonOnColourId, theme.withAlpha(0.85f));
			tile->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
			tile->setColour(juce::TextButton::textColourOnId,  juce::Colours::black);

			tile->onEngageChanged = [this, procIdx](bool engaged) {
				if (procIdx >= 1) {
					if (engaged) {
						postFxSelect(FxCategory::Pad, procIdx);
						postFxEngaged(FxCategory::Pad, true);
						padFxHeldIndex = procIdx;
					}
					else {
						postFxEngaged(FxCategory::Pad, false);
						padFxHeldIndex = -1;
					}
				}
			};
			tile->onShowParameters = [this, procIdx]() {
				postFxSelect(FxCategory::Pad, procIdx);
				showFxParameterModal(FxCategory::Pad, padFxTiles[(size_t) (procIdx - 1)].get());
			};

			addChildComponent(*tile);
			padFxTiles.push_back(std::move(tile));
		}
	}

	// ---- Beat FX selector / division / on / wet / edit -----------------------
	{
		auto procs = FxFactory::buildCategory(FxCategory::Beat);
		beatFxSelector.clear(juce::dontSendNotification);
		for (int i = 0; i < (int) procs.size(); ++i) {
			beatFxSelector.addItem(procs[i]->getName(), i + 1); // ComboBox ids are 1-based
		}
		beatFxSelector.setSelectedId(1, juce::dontSendNotification); // None
		beatFxSelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxSelector.setColour(juce::ComboBox::textColourId, juce::Colours::white);
		beatFxSelector.setColour(juce::ComboBox::outlineColourId, theme.withAlpha(0.5f));
		beatFxSelector.onChange = [this]() {
			int idx = beatFxSelector.getSelectedId() - 1;
			if (idx >= 0) postFxSelect(FxCategory::Beat, idx);
		};
		addChildComponent(beatFxSelector);

		// Beat division: maps 1..7 → 1/16, 1/8, 1/4, 1/2, 1, 2, 4 beats.
		beatFxDivisionBox.clear(juce::dontSendNotification);
		const char* divNames[7] = { "1/16", "1/8", "1/4", "1/2", "1 BEAT", "2 BEAT", "4 BEAT" };
		for (int i = 0; i < 7; ++i)
			beatFxDivisionBox.addItem(divNames[i], i + 1);
		beatFxDivisionBox.setSelectedId(3, juce::dontSendNotification); // 1/4 default
		beatFxDivisionBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxDivisionBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
		beatFxDivisionBox.setColour(juce::ComboBox::outlineColourId, theme.withAlpha(0.5f));
		beatFxDivisionBox.onChange = [this]() {
			// Atomic parameter write — bypass FIFO.
			auto& chain = player->getFxChain();
			if (auto* fx = chain.getActiveProcessor(FxCategory::Beat)) {
				for (auto& p : fx->getParameters()) {
					if (p.name == "Time") {
						double code = beatFxDivisionBox.getSelectedId() - 1; // 0..6
						p.set(code);
						break;
					}
				}
			}
		};
		addChildComponent(beatFxDivisionBox);

		beatFxOnButton.setClickingTogglesState(true);
		beatFxOnButton.addListener(this);
		beatFxOnButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxOnButton.setColour(juce::TextButton::buttonOnColourId, theme.withAlpha(0.85f));
		beatFxOnButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		beatFxOnButton.setColour(juce::TextButton::textColourOnId,  juce::Colours::black);
		addChildComponent(beatFxOnButton);

		beatFxWetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
		beatFxWetSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
		beatFxWetSlider.setRange(0.0, 1.0, 0.01);
		beatFxWetSlider.setValue(1.0, juce::dontSendNotification);
		beatFxWetSlider.setColour(juce::Slider::thumbColourId, theme);
		beatFxWetSlider.onValueChange = [this]() {
			auto& chain = player->getFxChain();
			if (auto* fx = chain.getActiveProcessor(FxCategory::Beat)) {
				for (auto& p : fx->getParameters()) {
					if (p.name == "Wet" || p.name == "Mix") {
						p.set(beatFxWetSlider.getValue());
						break;
					}
				}
			}
		};
		addChildComponent(beatFxWetSlider);

		beatFxWetLabel.setJustificationType(juce::Justification::centred);
		beatFxWetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
		addChildComponent(beatFxWetLabel);

		beatFxEditButton.addListener(this);
		beatFxEditButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(25, 25, 25, 255));
		beatFxEditButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		addChildComponent(beatFxEditButton);
	}

	// ---- Release FX tiles ----------------------------------------------------
	{
		auto procs = FxFactory::buildCategory(FxCategory::Release);
		const int kTiles = 3;
		releaseFxTiles.reserve(kTiles);
		for (int slot = 0; slot < kTiles; ++slot) {
			auto tile = std::make_unique<MomentaryFxTile>();
			int procIdx = slot + 1;
			juce::String label = "—";
			if (procIdx < (int) procs.size()) label = procs[procIdx]->getName();
			tile->setButtonText(label);
			tile->setColour(juce::TextButton::buttonColourId, themeMid);
			tile->setColour(juce::TextButton::buttonOnColourId, theme.withAlpha(0.85f));
			tile->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
			tile->setColour(juce::TextButton::textColourOnId,  juce::Colours::black);

			tile->onEngageChanged = [this, procIdx](bool engaged) {
				if (engaged) {
					postFxSelect(FxCategory::Release, procIdx);
					postFxEngaged(FxCategory::Release, true);
					releaseFxHeldIndex = procIdx;
				}
				else {
					postFxEngaged(FxCategory::Release, false);
					releaseFxHeldIndex = -1;
				}
			};
			tile->onShowParameters = [this, procIdx]() {
				postFxSelect(FxCategory::Release, procIdx);
				showFxParameterModal(FxCategory::Release, releaseFxTiles[(size_t) (procIdx - 1)].get());
			};
			addChildComponent(*tile);
			releaseFxTiles.push_back(std::move(tile));
		}
	}
}

//==============================================================================

/** Shows or hides the Pad FX tiles. */
void DeckGUI::setPadFxControlsVisible(bool visible)
{
	for (auto& tile : padFxTiles) tile->setVisible(visible);
}

/** Shows or hides the Beat FX controls. */
void DeckGUI::setBeatFxControlsVisible(bool visible)
{
	beatFxSelector   .setVisible(visible);
	beatFxDivisionBox.setVisible(visible);
	beatFxOnButton   .setVisible(visible);
	beatFxWetSlider  .setVisible(visible);
	beatFxWetLabel   .setVisible(visible);
	beatFxEditButton .setVisible(visible);
}

/** Shows or hides the Release FX tiles. */
void DeckGUI::setReleaseFxControlsVisible(bool visible)
{
	for (auto& tile : releaseFxTiles) tile->setVisible(visible);
}

//==============================================================================

/**
 * Refreshes the on-screen state (toggle/selection) of the FX panels to match
 * the underlying FxChain. Called whenever an FX tab is opened so the UI
 * reflects values that may have been loaded from FxSettings.xml.
 */
void DeckGUI::refreshFxUi()
{
	auto& chain = player->getFxChain();

	// Beat FX selector + ON/OFF + wet/division.
	int beatIdx = chain.getActiveIndex(FxCategory::Beat);
	if (beatIdx >= 0) beatFxSelector.setSelectedId(beatIdx + 1, juce::dontSendNotification);
	if (auto* fx = chain.getActiveProcessor(FxCategory::Beat)) {
		beatFxOnButton.setToggleState(fx->isEngaged(), juce::dontSendNotification);
		for (auto& p : fx->getParameters()) {
			if (p.name == "Wet" || p.name == "Mix") {
				beatFxWetSlider.setValue(p.get(), juce::dontSendNotification);
			}
			else if (p.name == "Time") {
				int code = (int) std::round(p.get());
				if (code >= 0 && code <= 6)
					beatFxDivisionBox.setSelectedId(code + 1, juce::dontSendNotification);
			}
		}
	}
}

//==============================================================================

/** Posts an FxSelect command. UI thread → audio thread via the SPSC fifo. */
void DeckGUI::postFxSelect(FxCategory cat, int processorIndex)
{
	AudioCommand cmd;
	cmd.tag = AudioCommand::Tag::FxSelect;
	cmd.intPayload = (int) cat * 1000 + processorIndex;
	player->postCommand(cmd);
}

/** Posts an FxSetEngaged command. */
void DeckGUI::postFxEngaged(FxCategory cat, bool engaged)
{
	AudioCommand cmd;
	cmd.tag = AudioCommand::Tag::FxSetEngaged;
	cmd.intPayload = (int) cat * 1000 + (engaged ? 1 : 0);
	player->postCommand(cmd);
}

//==============================================================================

/**
 * Opens the parameter modal for the active processor in the given category.
 *
 * The Save button persists *both* decks' current FX state to disk via the
 * AudioEngine handle. The Reset button only resets parameters of the
 * processor inside the modal (handled inside FxParameterModal).
 */
void DeckGUI::showFxParameterModal(FxCategory cat, juce::Component* anchor)
{
	auto& chain = player->getFxChain();
	auto* fx = chain.getActiveProcessor(cat);
	if (fx == nullptr) return;

	auto* engine = audioEngine;
	auto saveCb = [engine]() {
		if (engine == nullptr) return;
		FxSettings::saveAll(engine->getPlayer(0).getFxChain(),
		                    engine->getPlayer(1).getFxChain());
	};

	auto modal = std::make_unique<FxParameterModal>(*fx, theme, std::move(saveCb));

	auto bounds = anchor != nullptr
		? anchor->getScreenBounds()
		: juce::Rectangle<int>(getScreenX() + getWidth() / 2 - 4, getScreenY() + 60, 8, 8);
	juce::CallOutBox::launchAsynchronously(std::move(modal), bounds, nullptr);
}

//==============================================================================

/**
 * Public wrapper for loadDeck — used by external library sidebars.
 */
void DeckGUI::loadTrack(const track& t) {
	loadDeck(t);
}

/**
 * Append a track to the deck's queue widget.
 */
void DeckGUI::enqueueTrack(const track& t) {
	if (queueWidget != nullptr)
		queueWidget->pushBack(t);
}

//==============================================================================
