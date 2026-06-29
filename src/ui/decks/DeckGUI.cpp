
#include "DeckGUI.h"
#include "../../core/data/FileHasher.h"
#include "../../core/effects/FxFactory.h"
#include "../../core/effects/FxChain.h"
#include "../../core/effects/FxProcessor.h"
#include "../settings/FxParameterModal.h"
#include "../../core/effects/FxSettings.h"
#include "../../core/audio/AudioEngine.h"
#include "../../core/analysis/WaveformBandAnalyzer.h"

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

	formatManager = &formatManagerToUse;

	// Phase 2 - Register structural layout containers. Bounds are set in Phase 3 (resized()).
	addAndMakeVisible(topHeaderContainer);
	addAndMakeVisible(waveformContainer);
	addAndMakeVisible(jogWheelContainer);
	addAndMakeVisible(transportContainer);
	addAndMakeVisible(mixerContainer);
	addAndMakeVisible(sidebarContainer);

	// "Loading..." overlay (centred over the waveform area, hidden by default).
	loadingLabel.setJustificationType(juce::Justification::centred);
	loadingLabel.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
	loadingLabel.setColour(juce::Label::textColourId, theme);
	loadingLabel.setColour(juce::Label::backgroundColourId, UI::bgRoot.withAlpha(0.86f));
	loadingLabel.setVisible(false);
	waveformContainer.addAndMakeVisible(loadingLabel);
	for (auto* label : { &speedLabel, &lbLabel, &mbLabel, &hbLabel }) {
		label->setEditable(false);
		label->setJustificationType(juce::Justification::centred);
	}
	jogWheelContainer.addAndMakeVisible(speedLabel);
	jogWheelContainer.addAndMakeVisible(lbLabel);
	jogWheelContainer.addAndMakeVisible(mbLabel);
	jogWheelContainer.addAndMakeVisible(hbLabel);

	// BPM value and percent labels - live beside the jog wheel, not in the top header.
	bpmValueLabel.setEditable(false);
	bpmValueLabel.setJustificationType(juce::Justification::centred);
	bpmValueLabel.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
	bpmValueLabel.setColour(juce::Label::textColourId, theme);
	jogWheelContainer.addAndMakeVisible(bpmValueLabel);

	jogWheelContainer.addAndMakeVisible(playButton);
	jogWheelContainer.addAndMakeVisible(speedSlider);

	// Speed deviation label - static strip above the speed slider.
	bpmPercentLabel.setEditable(false);
	bpmPercentLabel.setJustificationType(juce::Justification::centred);
	bpmPercentLabel.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
	bpmPercentLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	bpmPercentLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
	bpmPercentLabel.setVisible(false);
	jogWheelContainer.addAndMakeVisible(bpmPercentLabel);
	jogWheelContainer.addAndMakeVisible(loadButton);
	waveformContainer.addAndMakeVisible(waveformDisplay);
	jogWheelContainer.addAndMakeVisible(jogWheel);
	jogWheelContainer.addAndMakeVisible(lowBandFilter);
	jogWheelContainer.addAndMakeVisible(midBandFilter);
	jogWheelContainer.addAndMakeVisible(highBandFilter);

	cueButtonImage = juce::Drawable::createFromImageData(BinaryData::iconHeadphone_svg, (size_t)BinaryData::iconHeadphone_svgSize);
	cueButtonImageActive = CustomLookAndFeel::loadIcon(BinaryData::iconHeadphone_svg, theme);
	cueButton.setImages(cueButtonImage.get(), nullptr, nullptr, nullptr, cueButtonImageActive.get());
	cueButton.setColour(juce::DrawableButton::backgroundColourId,   juce::Colours::transparentBlack);
	cueButton.setColour(juce::DrawableButton::backgroundOnColourId, theme.withAlpha(0.85f));
	cueButton.setClickingTogglesState(false);
	cueButton.addListener(this);
	jogWheelContainer.addAndMakeVisible(cueButton);

	// Per-deck queue widget. Click a row to jump to that track.
	queueWidget = std::make_unique<DeckQueue>(theme, [this](const track& t) { loadDeck(t); });
	mixerContainer.addAndMakeVisible(*queueWidget);

	speedSlider.setRange(0.5, 2.0);
	speedSlider.setSkewFactorFromMidPoint(1.0); // 1.0 sits at the visual centre of a 50%–200% range
	// Transparent track background so the deck panel shows through (no dark pill fill).
	speedSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
	lowBandFilter.setRange(0.01, 2);
	midBandFilter.setRange(0.01, 2);
	highBandFilter.setRange(0.01, 2);
	waveformDisplay.setRange(0, 1);
	zoomedDisplay->setRange(0, 1);
	jogWheel.setRange(0, 1);

	lowBandFilter.setValue(1);
	midBandFilter.setValue(1);
	highBandFilter.setValue(1);
	speedSlider.setValue(1);

	playButton.addListener(this);
	loadButton.addListener(this);
	speedSlider.addListener(this);
	speedSlider.addMouseListener(this, false);

	lowBandFilter.addListener(this);
	lowBandFilter.addMouseListener(this, false);
	midBandFilter.addListener(this);
	midBandFilter.addMouseListener(this, false);
	highBandFilter.addListener(this);
	highBandFilter.addMouseListener(this, false);

	startTimer(33); // 30 fps is imperceptible for waveform updates; halving from 50 Hz reduces render work

	for (auto i = 0; i < 6; ++i) {
		cues.push_back(new juce::TextButton());
	}
	for (auto& cue : cues) {
		transportContainer.addAndMakeVisible(cue);
		cue->addListener(this);
		cue->addMouseListener(this, false);
		cue->setLookAndFeel(&customLookAndFeel);
	}

	// Tab buttons for cue/grid/jump/loop/sync switching - these live in the
	// full-height sidebar rail, NOT in the transport content panel.
	sidebarContainer.addAndMakeVisible(cueTabButton);
	sidebarContainer.addAndMakeVisible(gridTabButton);
	sidebarContainer.addAndMakeVisible(jumpTabButton);
	sidebarContainer.addAndMakeVisible(loopTabButton);
	sidebarContainer.addAndMakeVisible(syncTabButton);
	cueTabButton.addListener(this);
	gridTabButton.addListener(this);
	jumpTabButton.addListener(this);
	loopTabButton.addListener(this);
	syncTabButton.addListener(this);
	cueTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
	gridTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	jumpTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	loopTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	syncTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);

	// FX tab buttons (P.FX / B.FX / R.FX) - registered here so the row is built
	// in tab-order. Their colour follows the same pattern as the other tabs.
	sidebarContainer.addAndMakeVisible(padFxTabButton);
	sidebarContainer.addAndMakeVisible(beatFxTabButton);
	sidebarContainer.addAndMakeVisible(releaseFxTabButton);
	padFxTabButton.addListener(this);
	beatFxTabButton.addListener(this);
	releaseFxTabButton.addListener(this);
	padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);

	// Sync tab controls (initially hidden until SYNC tab selected).
	masterToggleBtn.setClickingTogglesState(true);
	masterToggleBtn.addListener(this);
	masterToggleBtn.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	masterToggleBtn.setColour(juce::TextButton::buttonOnColourId, UI::accentWarning.withAlpha(0.8f));
	masterToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	masterToggleBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
	masterToggleBtn.setLookAndFeel(&customLookAndFeel);
	transportContainer.addChildComponent(masterToggleBtn);

	syncEngageBtn.setClickingTogglesState(true);
	syncEngageBtn.addListener(this);
	syncEngageBtn.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	syncEngageBtn.setColour(juce::TextButton::buttonOnColourId, UI::accentPositive.withAlpha(0.85f));
	syncEngageBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	syncEngageBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
	syncEngageBtn.setLookAndFeel(&customLookAndFeel);
	transportContainer.addChildComponent(syncEngageBtn);

	for (auto* btn : { &multHalfBtn, &multOneBtn, &multTwoBtn }) {
		btn->addListener(this);
		btn->setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		btn->setLookAndFeel(&customLookAndFeel);
		transportContainer.addChildComponent(*btn);
	}

	targetBpmLabel.setEditable(false);
	targetBpmLabel.setJustificationType(juce::Justification::centred);
	targetBpmLabel.setColour(juce::Label::textColourId, theme);
	targetBpmLabel.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
	transportContainer.addChildComponent(targetBpmLabel);

	syncStatusLabel.setEditable(false);
	syncStatusLabel.setJustificationType(juce::Justification::centred);
	syncStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	syncStatusLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
	transportContainer.addChildComponent(syncStatusLabel);

	// Fast-sync compact button (always visible near play/load).
	fastSyncBtnImage = juce::Drawable::createFromImageData(BinaryData::iconSyncBolt_svg, (size_t)BinaryData::iconSyncBolt_svgSize);
	fastSyncBtnImageActive = CustomLookAndFeel::loadIcon(BinaryData::iconSyncBolt_svg, UI::accentPositive);
	fastSyncBtn.setImages(fastSyncBtnImage.get(), nullptr, nullptr, nullptr, fastSyncBtnImageActive.get());
	fastSyncBtn.setColour(juce::DrawableButton::backgroundColourId,   juce::Colours::transparentBlack);
	fastSyncBtn.setColour(juce::DrawableButton::backgroundOnColourId, UI::accentPositive.withAlpha(0.85f));
	fastSyncBtn.setClickingTogglesState(true);
	fastSyncBtn.addListener(this);
	jogWheelContainer.addAndMakeVisible(fastSyncBtn);

	// Right-click guards: prevent right-click from triggering the 4 jog-wheel buttons.
	// fastSyncGuard also shows a context menu for the sync-reset action.
	playBtnGuard.init(playButton);
	loadBtnGuard.init(loadButton);
	cueBtnGuard .init(cueButton);
	fastSyncGuard.init(fastSyncBtn, [this]()
	{
		if (syncManager == nullptr) return;
		juce::PopupMenu menu;
		menu.addItem(1, "Reset sync (disengage + restore speed)");
		menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(fastSyncBtn),
			[this](int result)
			{
				if (result == 1)
				{
					syncManager->disengageSync(deckIndex);
					if (syncManager->isMaster(deckIndex))
						syncManager->setMaster(BeatSyncManager::MasterDeck::None);
					syncManager->setSlaveMultiplier(deckIndex, 1.0);
					speedSlider.setValue(1.0, juce::sendNotification);
				}
			});
	});

	// Snap-quantisation combo for sync (1 BAR / 1/2 / 1/4).
	snapBox.addItem("1 BAR",   1);
	snapBox.addItem("1/2 BAR", 2);
	snapBox.addItem("1/4 BAR", 3);
	snapBox.setSelectedId(1, juce::dontSendNotification);
	snapBox.setColour(juce::ComboBox::backgroundColourId, UI::bgRoot);
	snapBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
	snapBox.setColour(juce::ComboBox::outlineColourId, theme.withAlpha(0.5f));
	snapBox.setLookAndFeel(&customLookAndFeel);
	snapBox.onChange = [this]() {
		if (syncManager == nullptr) return;
		switch (snapBox.getSelectedId()) {
			case 1: syncManager->setSnapBeats(4); break; // 1 bar
			case 2: syncManager->setSnapBeats(2); break; // 1/2 bar
			case 3: syncManager->setSnapBeats(1); break; // 1/4 bar
		}
	};
	transportContainer.addChildComponent(snapBox);

	// Reset button: clears master, disengages sync, restores speed to 1.0.
	syncResetBtn.addListener(this);
	syncResetBtn.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	syncResetBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	syncResetBtn.setLookAndFeel(&customLookAndFeel);
	transportContainer.addChildComponent(syncResetBtn);

	// Quantize tab button and controls
	sidebarContainer.addAndMakeVisible(quantizeTabButton);
	quantizeTabButton.addListener(this);
	quantizeTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);

	quantizeLabel.setEditable(false);
	quantizeLabel.setJustificationType(juce::Justification::centredLeft);
	quantizeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	transportContainer.addChildComponent(quantizeLabel);

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
	quantizeComboBox.setColour(juce::ComboBox::backgroundColourId, UI::bgRoot);
	quantizeComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
	quantizeComboBox.setColour(juce::ComboBox::outlineColourId, theme.withAlpha(0.5f));
	quantizeComboBox.setLookAndFeel(&customLookAndFeel);
	transportContainer.addChildComponent(quantizeComboBox);

	// Beat grid controls
	gridBpmLabel.setEditable(false);
	gridBpmLabel.setJustificationType(juce::Justification::centredLeft);
	gridBpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	transportContainer.addChildComponent(gridBpmLabel);

	gridBpmEditor.setJustification(juce::Justification::centred);
	gridBpmEditor.setInputRestrictions(7, "0123456789.");
	gridBpmEditor.setColour(juce::TextEditor::backgroundColourId, UI::bgRoot);
	gridBpmEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
	gridBpmEditor.setColour(juce::TextEditor::outlineColourId, theme.withAlpha(0.5f));
	gridBpmEditor.setLookAndFeel(&customLookAndFeel);
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
	transportContainer.addChildComponent(gridBpmEditor);

	gridNudgeLeftBtn.addListener(this);
	gridNudgeRightBtn.addListener(this);
	tapTempoBtn.addListener(this);
	gridResetBtn.addListener(this);
	gridNudgeLeftBtn.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	gridNudgeLeftBtn.setLookAndFeel(&customLookAndFeel);
	gridNudgeRightBtn.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	gridNudgeRightBtn.setLookAndFeel(&customLookAndFeel);
	tapTempoBtn.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	tapTempoBtn.setLookAndFeel(&customLookAndFeel);
	gridResetBtn.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
	gridResetBtn.setLookAndFeel(&customLookAndFeel);
	transportContainer.addChildComponent(gridNudgeLeftBtn);
	transportContainer.addChildComponent(gridNudgeRightBtn);
	transportContainer.addChildComponent(gridOffsetLabel);
	transportContainer.addChildComponent(tapTempoBtn);
	transportContainer.addChildComponent(gridResetBtn);

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
		btn->setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		btn->setLookAndFeel(&customLookAndFeel);
		transportContainer.addChildComponent(*btn);
	}

	jumpLabel.setEditable(false);
	jumpLabel.setJustificationType(juce::Justification::centred);
	jumpLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	jumpLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
	transportContainer.addChildComponent(jumpLabel);

	// Loop controls
	std::vector<juce::TextButton*> loopBtns{
		&loopInBtn, &loopOutBtn, &reloopBtn, &loopHalveBtn, &loopDoubleBtn, &loopClearBtn
	};
	for (auto* btn : loopBtns) {
		btn->addListener(this);
		btn->setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		btn->setLookAndFeel(&customLookAndFeel);
		transportContainer.addChildComponent(*btn);
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
	playButton.setColour(juce::DrawableButton::backgroundColourId,   juce::Colours::transparentBlack);
	playButton.setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
	loadButton.setColour(juce::DrawableButton::backgroundColourId,   juce::Colours::transparentBlack);
	loadButton.setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);

	// Library button - placeholder icon (iconSync) until a dedicated asset is added in Phase 4.
	libraryButtonImage = juce::Drawable::createFromImageData(BinaryData::iconSync_svg, (size_t)BinaryData::iconSync_svgSize);
	libraryButton.setImages(libraryButtonImage.get());
	libraryButton.setColour(juce::DrawableButton::backgroundColourId,   juce::Colours::transparentBlack);
	libraryButton.setColour(juce::DrawableButton::backgroundOnColourId, theme.withAlpha(0.85f));
	libraryButton.setEdgeIndent(0);
	libraryButton.addListener(this);
	jogWheelContainer.addAndMakeVisible(libraryButton);
	libraryButton.setVisible(false); // redundant with loadButton in the new wireframe layout

	// Phase 4 - Mark the four jog-wheel corner buttons so the LookAndFeel
	// renders them as a unified set (matching circular outline + hover ring).
	for (auto* b : { (juce::Button*) &loadButton, (juce::Button*) &playButton,
	                 (juce::Button*) &cueButton,  (juce::Button*) &fastSyncBtn })
		b->getProperties().set("circularOutline", true);

	playButton.setLookAndFeel(&customLookAndFeel);
	loadButton.setLookAndFeel(&customLookAndFeel);
	cueButton.setLookAndFeel(&customLookAndFeel);
	fastSyncBtn.setLookAndFeel(&customLookAndFeel);
	libraryButton.setLookAndFeel(&customLookAndFeel);
	cueTabButton.setLookAndFeel(&customLookAndFeel);
	gridTabButton.setLookAndFeel(&customLookAndFeel);
	jumpTabButton.setLookAndFeel(&customLookAndFeel);
	loopTabButton.setLookAndFeel(&customLookAndFeel);
	syncTabButton.setLookAndFeel(&customLookAndFeel);
	padFxTabButton.setLookAndFeel(&customLookAndFeel);
	beatFxTabButton.setLookAndFeel(&customLookAndFeel);
	releaseFxTabButton.setLookAndFeel(&customLookAndFeel);
	speedSlider.setLookAndFeel(&customLookAndFeel);
	lowBandFilter.setLookAndFeel(&customLookAndFeel);
	midBandFilter.setLookAndFeel(&customLookAndFeel);
	highBandFilter.setLookAndFeel(&customLookAndFeel);

	// Per-deck rotary accent colour: deck 2 uses pink-red, deck 1 uses blue.
	{
		const juce::Colour knobAccent = (deckIndex == 1) ? UI::deck2Accent : UI::deck1Accent;
		for (auto* s : { &lowBandFilter, &midBandFilter, &highBandFilter })
			s->setColour(juce::Slider::rotarySliderFillColourId, knobAccent);
	}

	// =========================================================================
	// Tooltips - set on every interactive control so hovering shows a hint.
	// =========================================================================

	// Jog-wheel corner buttons
	playButton  .setTooltip("Play / Pause");
	loadButton  .setTooltip("Open library to load a track");
	cueButton   .setTooltip("Headphone cue - monitor this deck through your cue output");
	fastSyncBtn .setTooltip("Beat sync - left-click to engage, right-click to reset speed");

	// EQ knobs and speed slider
	speedSlider  .setTooltip("Speed - drag to adjust playback rate (right-click to reset to 1.0x)");
	lowBandFilter.setTooltip("Low EQ - boost or cut bass frequencies (right-click to reset)");
	midBandFilter.setTooltip("Mid EQ - boost or cut mid frequencies (right-click to reset)");
	highBandFilter.setTooltip("High EQ - boost or cut treble frequencies (right-click to reset)");

	// Tab rail
	cueTabButton      .setTooltip("Hot Cues - set and jump to up to 6 cue points");
	gridTabButton     .setTooltip("Beat Grid - edit BPM, tap tempo, nudge grid offset");
	jumpTabButton     .setTooltip("Beat Jump - jump backward or forward by a fixed number of beats");
	loopTabButton     .setTooltip("Loop - set in/out points and control loop length");
	quantizeTabButton .setTooltip("Quantize - snap performance actions to the beat grid");
	syncTabButton     .setTooltip("Sync - lock this deck's tempo to the master deck");
	padFxTabButton    .setTooltip("Pad FX - momentary effects: hold to engage, release to disengage");
	beatFxTabButton   .setTooltip("Beat FX - latched effects: click to toggle on or off");
	releaseFxTabButton.setTooltip("Release FX - effects triggered on button release");

	// Beat Grid tab
	gridBpmEditor    .setTooltip("Type a BPM value and press Enter to override the detected tempo");
	gridNudgeLeftBtn .setTooltip("Nudge beat grid earlier");
	gridNudgeRightBtn.setTooltip("Nudge beat grid later");
	tapTempoBtn      .setTooltip("Tap Tempo - tap repeatedly to set BPM");
	gridResetBtn     .setTooltip("Reset beat grid to auto-detected values");

	// Beat Jump tab
	jumpBackward16Btn.setTooltip("Jump back 16 beats");
	jumpBackward8Btn .setTooltip("Jump back 8 beats");
	jumpBackward4Btn .setTooltip("Jump back 4 beats");
	jumpBackward1Btn .setTooltip("Jump back 1 beat");
	jumpForward1Btn  .setTooltip("Jump forward 1 beat");
	jumpForward4Btn  .setTooltip("Jump forward 4 beats");
	jumpForward8Btn  .setTooltip("Jump forward 8 beats");
	jumpForward16Btn .setTooltip("Jump forward 16 beats");

	// Loop tab
	loopInBtn    .setTooltip("Set loop-in point at current playhead position");
	loopOutBtn   .setTooltip("Set loop-out point at current playhead position");
	reloopBtn    .setTooltip("Re-enable or disable the active loop");
	loopHalveBtn .setTooltip("Halve loop length");
	loopDoubleBtn.setTooltip("Double loop length");
	loopClearBtn .setTooltip("Clear all loop points");

	// Quantize tab
	quantizeComboBox.setTooltip("Select the beat subdivision to snap actions to (None = off)");

	// Sync tab
	masterToggleBtn.setTooltip("Set this deck as the sync master");
	syncEngageBtn  .setTooltip("Engage beat sync - lock this deck's tempo to the master");
	multHalfBtn    .setTooltip("Play at half the master's tempo (x 0.5)");
	multOneBtn     .setTooltip("Match the master's tempo exactly (x 1)");
	multTwoBtn     .setTooltip("Play at double the master's tempo (x 2)");
	syncResetBtn   .setTooltip("Disengage sync, clear master, and restore speed to 1.0x");
	snapBox        .setTooltip("Phase-snap granularity: how many beats to align on sync engage");

	// Build Pad / Beat / Release FX panels (initially hidden - HotCues is the
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
	// Modern rounded panel background with subtle vertical gradient.
	auto r = getLocalBounds().toFloat().reduced(2.0f);
	CustomLookAndFeel::paintPanelBackground(g, r, true, UI::kPanelRadius);
	// Theme accent strip on the deck's INNER edge (toward crossfader)
	{
		const float stripW = 3.0f;
		float stripX = (deckIndex == 0) ? r.getRight() - stripW : r.getX();
		g.setColour(theme.withAlpha(0.85f));
		g.fillRect(juce::Rectangle<float>(stripX, r.getY() + 6.0f, stripW, r.getHeight() - 12.0f));
	}

}

/**
 * Implementation of resized method for DeckGUI
 *
 * All juce::Component data members call it's setBounds method to achieve uniform space and sizing.
 *
 */
void DeckGUI::resized()
{
	const bool isDeck2 = (deckIndex == 1);
	const int  gap     = UI::kComponentPadding; // 6 px

	// =========================================================================
	// Phase 1 - Slice the full-height sidebar (icon-tab rail) FIRST so it
	// reaches the absolute top and bottom edges of the deck panel.
	// =========================================================================
	auto deckBounds = getLocalBounds().reduced(UI::kDeckMargin);

	const int sidebarW = UI::kRailWidth;
	auto sidebarBounds = isDeck2 ? deckBounds.removeFromRight(sidebarW)
	                             : deckBounds.removeFromLeft(sidebarW);
	sidebarContainer.setBounds(sidebarBounds);

	// Small breathing room between sidebar and the rest of the deck.
	if (isDeck2) deckBounds.removeFromRight(gap);
	else         deckBounds.removeFromLeft(gap);

	// =========================================================================
	// Phase 2 - Top-to-bottom stacking inside the remaining content column.
	// Top header (BPM)  →  Waveform  →  Tab content  →  ... center ...  →  Queue
	// =========================================================================
	topHeaderContainer.setBounds(juce::Rectangle<int>()); // zero size - BPM moved to jogWheelContainer

	waveformContainer.setBounds(deckBounds.removeFromTop(UI::kWaveformHeight + 30));
	deckBounds.removeFromTop(gap);

	transportContainer.setBounds(deckBounds.removeFromTop(80));
	deckBounds.removeFromTop(gap);

	// Queue at the bottom - noticeably taller than before.
	const int queueH = 150;
	mixerContainer.setBounds(deckBounds.removeFromBottom(queueH));
	deckBounds.removeFromBottom(gap);

	// =========================================================================
	// Phase 3 - Central deck area: jog wheel, 4 corner buttons, BPM, speed
	//           slider, and Low/Mid/High EQ knobs.
	// =========================================================================
	jogWheelContainer.setBounds(deckBounds);

	// =========================================================================
	// Sub-layout: sidebarContainer - 9 IconTabButtons stacked top-to-bottom.
	// =========================================================================
	{
		auto sb = sidebarContainer.getLocalBounds();
		const int tabCount = 9;
		const int tabGap   = 2;
		const int tabSlot  = juce::jmax(28, (sb.getHeight() - (tabCount - 1) * tabGap) / tabCount);
		auto setTab = [&](juce::Component& c, int row) {
			c.setBounds(sb.getX(),
			            sb.getY() + row * (tabSlot + tabGap),
			            sb.getWidth(), tabSlot);
		};
		setTab(cueTabButton,       0);
		setTab(gridTabButton,      1);
		setTab(jumpTabButton,      2);
		setTab(loopTabButton,      3);
		setTab(quantizeTabButton,  4);
		setTab(syncTabButton,      5);
		setTab(padFxTabButton,     6);
		setTab(beatFxTabButton,    7);
		setTab(releaseFxTabButton, 8);
	}

	// =========================================================================
	// Sub-layout: topHeaderContainer (BPM block moved to jogWheelContainer).
	// =========================================================================
	// bpmValueLabel and bpmPercentLabel are now children of jogWheelContainer;
	// their bounds are set inside the jogWheelContainer sub-layout below.

	// =========================================================================
	// Sub-layout: waveformContainer.
	// =========================================================================
	{
		const auto wb = waveformContainer.getLocalBounds();
		waveformDisplay.setBounds(wb);
		loadingLabel.setBounds(wb);
	}

	// =========================================================================
	// Sub-layout: mixerContainer (now: queue only, full inner width).
	// =========================================================================
	if (queueWidget)
		queueWidget->setBounds(mixerContainer.getLocalBounds().reduced(gap, 4));

	// =========================================================================
	// Sub-layout: jogWheelContainer
	//   - JogWheel: large centred square in the upper portion
	//   - 4 corner buttons positioned relative to the jog-wheel rect
	//   - Speed slider: vertical strip on the inner edge
	//   - BPM-side label: opposite edge of speed
	//   - Low / Mid / High knobs: equal columns under the jog wheel
	// =========================================================================
	{
		auto jb = jogWheelContainer.getLocalBounds().reduced(gap);

		// Reserve the bottom strip for the EQ knob row.
		const int eqRowH = UI::kKnobSize + UI::kKnobLabelHeight + gap;
		auto eqRow      = jb.removeFromBottom(eqRowH);
		jb.removeFromBottom(gap);

		// Speed slider lives on the INNER edge of the deck (toward crossfader).
		const int speedColW = juce::jmax(28, jb.getWidth() / 7);
		auto speedCol = isDeck2 ? jb.removeFromLeft(speedColW)
		                        : jb.removeFromRight(speedColW);
		speedLabel.setBounds(speedCol.removeFromBottom(UI::kKnobLabelHeight + 2));
		bpmPercentLabel.setBounds(speedCol.removeFromTop(14));
		speedSlider.setBounds(speedCol);

		// Centre the jog wheel as the largest square fitting the remaining area,
		// leaving room for the four corner buttons.
		const int btnSz   = juce::jmin(48, juce::jmax(34, juce::jmin(jb.getWidth(), jb.getHeight()) / 5));
		const int wheelMax = juce::jmin(jb.getWidth() - btnSz - gap * 2,
		                                jb.getHeight() - btnSz - gap * 2);
		const int jogSz   = juce::jmax(80, wheelMax);
		auto jogRect = jb.withSizeKeepingCentre(jogSz, jogSz);
		jogWheel.setBounds(jogRect);

		// Phase 3 - Place the 4 corner buttons relative to the jog wheel.
		// TL = load (+), TR = cue (headphones), BL = play, BR = fast-sync.
		loadButton .setBounds(jogRect.getX() - btnSz - gap,    jogRect.getY(),                       btnSz, btnSz);
		cueButton  .setBounds(jogRect.getRight() + gap,        jogRect.getY(),                       btnSz, btnSz);
		playButton .setBounds(jogRect.getX() - btnSz - gap,    jogRect.getBottom() - btnSz,          btnSz, btnSz);
		fastSyncBtn.setBounds(jogRect.getRight() + gap,        jogRect.getBottom() - btnSz,          btnSz, btnSz);

		// BPM block: centred in the vertical strip between top and bottom corner
		// buttons, on the outer column beside the jog wheel.
		const int bpmH    = 13 + 10; // value (13 px) + percent (10 px)
		const int bpmColX = isDeck2 ? cueButton.getX()     : loadButton.getX();
		const int bpmColW = isDeck2 ? cueButton.getWidth()  : loadButton.getWidth();
		const int midTop  = isDeck2 ? cueButton.getBottom() : loadButton.getBottom();
		const int midBot  = isDeck2 ? fastSyncBtn.getY()    : playButton.getY();
		const int bpmY    = midTop + (midBot - midTop - bpmH) / 2;
		bpmValueLabel.setBounds(bpmColX, bpmY, bpmColW, 13);
		// bpmPercentLabel is positioned dynamically above the speed slider thumb.

		// Low / Mid / High knobs: 3 equal columns spanning the EQ row.
		const int eqColW = eqRow.getWidth() / 3;
		auto placeKnob = [&](juce::Slider& knob, juce::Label& lbl, int col) {
			auto cell = eqRow.withX(eqRow.getX() + col * eqColW).withWidth(eqColW);
			knob.setBounds(cell.removeFromTop(UI::kKnobSize)
			                   .withSizeKeepingCentre(UI::kKnobSize, UI::kKnobSize));
			lbl .setBounds(cell.removeFromTop(UI::kKnobLabelHeight));
		};
		placeKnob(lowBandFilter,  lbLabel, 0);
		placeKnob(midBandFilter,  mbLabel, 1);
		placeKnob(highBandFilter, hbLabel, 2);
	}

	// =========================================================================
	// Sub-layout: transportContainer (tab CONTENT only - rail moved to sidebar).
	// =========================================================================
	{
		auto tb = transportContainer.getLocalBounds();

		const auto ca    = tb.reduced(gap);
		const int  xOff  = ca.getX();
		const int  yOff  = ca.getY();
		const int  totalW = ca.getWidth();
		const int  totalH = ca.getHeight();
		const int  cellW  = juce::jmax(1, totalW / 3);
		const int  cellH  = juce::jmax(20, totalH / 2);
		const int  ctrlW  = cellW - 4;

		// -- Cue buttons: 3 columns × 2 rows --
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 2; ++j)
				cues[i * 2 + j]->setBounds(xOff + i * cellW, yOff + j * cellH, cellW - 4, cellH - 4);

		// -- Beat grid controls --
		gridBpmLabel    .setBounds(xOff,                        yOff, ctrlW * 4 / 10, cellH - 4);
		gridBpmEditor   .setBounds(xOff + ctrlW * 4 / 10,       yOff, ctrlW * 6 / 10, cellH - 4);
		gridOffsetLabel .setBounds(xOff + cellW,                yOff,        ctrlW,      14);
		gridNudgeLeftBtn .setBounds(xOff + cellW,               yOff + 14, ctrlW / 2 - 2, cellH - 18);
		gridNudgeRightBtn.setBounds(xOff + cellW + ctrlW / 2,   yOff + 14, ctrlW / 2 - 2, cellH - 18);
		tapTempoBtn     .setBounds(xOff + cellW * 2,            yOff,        ctrlW,      cellH - 4);
		gridResetBtn    .setBounds(xOff,                        yOff + cellH, ctrlW,     cellH - 4);

		// -- Beat jump: 4×2 grid --
		{
			const int jumpBtnW = juce::jmax(1, (totalW - 4) / 4 - 3);
			jumpLabel.setBounds(xOff, juce::jmax(0, yOff - 14), totalW - 4, 14);
			juce::TextButton* row1[] = { &jumpBackward16Btn, &jumpBackward8Btn,  &jumpBackward4Btn,  &jumpBackward1Btn };
			juce::TextButton* row2[] = { &jumpForward1Btn,   &jumpForward4Btn,   &jumpForward8Btn,   &jumpForward16Btn };
			for (int i = 0; i < 4; ++i)
			{
				const int bx = xOff + i * (jumpBtnW + 4);
				row1[i]->setBounds(bx, yOff,         jumpBtnW, cellH - 4);
				row2[i]->setBounds(bx, yOff + cellH, jumpBtnW, cellH - 4);
			}
		}

		// -- Loop: 3×2 grid --
		{
			const int loopBtnW = juce::jmax(1, (totalW - 4) / 3 - 3);
			juce::TextButton* row1[] = { &loopInBtn,    &loopOutBtn,    &reloopBtn    };
			juce::TextButton* row2[] = { &loopHalveBtn, &loopDoubleBtn, &loopClearBtn };
			for (int i = 0; i < 3; ++i)
			{
				const int lx = xOff + i * (loopBtnW + 4);
				row1[i]->setBounds(lx, yOff,         loopBtnW, cellH - 4);
				row2[i]->setBounds(lx, yOff + cellH, loopBtnW, cellH - 4);
			}
		}

		// -- Quantize --
		quantizeLabel   .setBounds(xOff, yOff,      totalW - 4, 20);
		quantizeComboBox.setBounds(xOff, yOff + 22, totalW - 4, 28);

		// -- Sync --
		{
			const int syncTotalW = totalW - 4;
			const int syncColW   = juce::jmax(1, (syncTotalW - 4 * 3) / 4);
			masterToggleBtn.setBounds(xOff + (syncColW + 4) * 0, yOff, syncColW, cellH - 4);
			syncEngageBtn  .setBounds(xOff + (syncColW + 4) * 1, yOff, syncColW, cellH - 4);
			targetBpmLabel .setBounds(xOff + (syncColW + 4) * 2, yOff, syncColW, cellH - 4);
			syncResetBtn   .setBounds(xOff + (syncColW + 4) * 3, yOff, syncColW, cellH - 4);

			const int multBtnW = syncTotalW * 16 / 100;
			const int snapBoxW = syncTotalW * 28 / 100;
			const int statusW  = juce::jmax(0, syncTotalW - multBtnW * 3 - snapBoxW - 16);
			int cx = xOff;
			multHalfBtn    .setBounds(cx, yOff + cellH, multBtnW, cellH - 4); cx += multBtnW + 4;
			multOneBtn     .setBounds(cx, yOff + cellH, multBtnW, cellH - 4); cx += multBtnW + 4;
			multTwoBtn     .setBounds(cx, yOff + cellH, multBtnW, cellH - 4); cx += multBtnW + 4;
			snapBox        .setBounds(cx, yOff + cellH, snapBoxW, cellH - 4); cx += snapBoxW + 4;
			syncStatusLabel.setBounds(cx, yOff + cellH, statusW,  cellH - 4);
		}

		// -- Pad FX: 4×2 tile grid --
		{
			const int padCellW = juce::jmax(1, totalW / 4);
			const int padCellH = juce::jmax(1, totalH / 2);
			for (int idx = 0; idx < (int) padFxTiles.size(); ++idx)
				padFxTiles[idx]->setBounds(
					xOff + (idx % 4) * padCellW,
					yOff + (idx / 4) * padCellH,
					padCellW - 4, padCellH - 4);
		}

		// -- Beat FX --
		{
			const int bfxTotalW = totalW - 4;
			const int bfxColW   = juce::jmax(1, (bfxTotalW - 8) / 3);
			beatFxSelector   .setBounds(xOff,                     yOff, bfxColW, cellH - 4);
			beatFxDivisionBox.setBounds(xOff + bfxColW + 4,       yOff, bfxColW, cellH - 4);
			beatFxOnButton   .setBounds(xOff + (bfxColW + 4) * 2, yOff, bfxColW, cellH - 4);

			const int labelW = 36;
			const int editW  = bfxColW;
			const int wetW   = juce::jmax(40, bfxTotalW - labelW - editW - 8);
			beatFxWetLabel  .setBounds(xOff,                            yOff + cellH, labelW, cellH - 4);
			beatFxWetSlider .setBounds(xOff + labelW + 4,               yOff + cellH, wetW,   cellH - 4);
			beatFxEditButton.setBounds(xOff + labelW + 4 + wetW + 4,    yOff + cellH, editW,  cellH - 4);
		}

		// -- Release FX: 3 full-height tiles --
		{
			const int rfxCellW = juce::jmax(1, (totalW - 4) / 3);
			for (int idx = 0; idx < (int) releaseFxTiles.size(); ++idx)
				releaseFxTiles[idx]->setBounds(
					xOff + idx * rfxCellW, yOff,
					rfxCellW - 4, totalH - 4);
		}
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
				UI::accentWarning.withAlpha(0.7f));
			// Undo the auto-toggle - keep current state until fire
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

	if (button == &libraryButton) {
		if (onLoadButtonClicked)
			onLoadButtonClicked(deckIndex);
	}

	if (button == &cueButton)
	{
		if (onCueButtonClicked)
			onCueButtonClicked(deckIndex);
	}

	// Tab switching
	if (button == &cueTabButton) {
		cueGridMode = CueGridMode::HotCues;
		cueTabButton.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		gridTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		jumpTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		loopTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		syncTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
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
		cueTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		jumpTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		loopTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		syncTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
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
		cueTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		gridTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		loopTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		syncTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
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
		cueTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		gridTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		jumpTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		syncTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
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
		cueTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		gridTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		jumpTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		loopTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		syncTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
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
		cueTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		gridTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		jumpTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		loopTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
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
		cueTabButton     .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		gridTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		jumpTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		loopTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		quantizeTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		syncTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
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
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		hideAllPanels();
		setPadFxControlsVisible(true);
		refreshFxUi();
	}

	if (button == &beatFxTabButton) {
		cueGridMode = CueGridMode::BeatFx;
		dimAllNonFxTabs();
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.8f));
		releaseFxTabButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		hideAllPanels();
		setBeatFxControlsVisible(true);
		refreshFxUi();
	}

	if (button == &releaseFxTabButton) {
		cueGridMode = CueGridMode::ReleaseFx;
		dimAllNonFxTabs();
		padFxTabButton    .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxTabButton   .setColour(juce::TextButton::buttonColourId, UI::bgRoot);
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
					// Set cue (quantized) - capture position now
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
				if (result == 1)
					speedSlider.setValue(1.0, juce::sendNotification);
			});
		return;
	}

	if (source == &lowBandFilter) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to unity gain");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1)
					lowBandFilter.setValue(1.0, juce::sendNotification);
			});
		return;
	}

	if (source == &midBandFilter) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to unity gain");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1)
					midBandFilter.setValue(1.0, juce::sendNotification);
			});
		return;
	}

	if (source == &highBandFilter) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to unity gain");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1)
					highBandFilter.setValue(1.0, juce::sendNotification);
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

	if (slider == &speedSlider) {
		DBG("MainComponent::sliderValueChanged: They change the speed slider " << slider->getValue());
		// Capture the user's intended value before any side-effects can overwrite it.
		double userSpeed = slider->getValue();
		// User-initiated speed change while synced - break sync. (Programmatic
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
		updateCueButtons();
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

	volRMS = player->getRMSLevel(); // cached for external readers; DeckGUI doesn't draw meters

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
	{
		const bool nonZero = std::abs(speedRatio - 1.0) > 0.001;
		if (nonZero)
		{
			double pct = (speedRatio - 1.0) * 100.0;
			juce::String sign = pct > 0.0 ? "+" : "";
			bpmPercentLabel.setText(sign + juce::String(pct, 1) + "%", juce::dontSendNotification);
			const auto textCol = pct > 0.0 ? juce::Colour(0x66, 0xff, 0x88)
			                                : juce::Colour(0xff, 0x66, 0x66);
			bpmPercentLabel.setColour(juce::Label::textColourId, textCol);
			bpmPercentLabel.setVisible(true);
		}
		else
		{
			bpmPercentLabel.setVisible(false);
		}
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
			inSet ? juce::Colours::blue.withAlpha(0.7f) : UI::bgRoot);
	reloopBtn.setColour(juce::TextButton::buttonColourId,
		loopOn ? juce::Colours::limegreen.withAlpha(0.6f) : UI::bgRoot);

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

void DeckGUI::updateCueButtons()
{
	for (auto& cue : cues) {
		juce::TextButton* thisButton = cue;
		bool hasCue = cueTargets.find(thisButton) != cueTargets.end();

		if (pendingAction.isValid() && pendingAction.srcButton == thisButton) {
			// Keep orange - don't override
		}
		else if (hasCue && flash) {
			thisButton->setColour(juce::TextButton::ColourIds::buttonColourId,
				juce::Colour::fromHSL(cueTargets[thisButton].second, 1.0f, 0.5f, 1.0f));
		}
		else {
			thisButton->setColour(juce::TextButton::ColourIds::buttonColourId, UI::bgRoot);
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

	// Stash the track - finishLoadDeck() needs it once loading completes.
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
		display->setBandData(nullptr); // clear stale colours; will be filled when analysis completes
	}

	player->setGain(1.0, true);
	cueTargets.clear();

	// Update current track identity early so async callbacks below can stale-guard against it.
	currentTrackIdentity = t.identity;
	currentFileHash = t.fileHash;

	// Kick off off-thread 3-band waveform analysis. Result is delivered on the
	// message thread; we stale-guard against a newer track being loaded.
	if (formatManager != nullptr && t.fileHash.isNotEmpty()) {
		juce::Component::SafePointer<DeckGUI> safeSelf(this);
		const juce::String requestedHash = t.fileHash;
		WaveformBandAnalyzer::analyzeAsync(
			t.url.getLocalFile(),
			t.fileHash,
			*formatManager,
			[safeSelf, requestedHash](BandDataPtr bands) {
				auto* self = safeSelf.getComponent();
				if (self == nullptr) return;
				if (self->currentFileHash != requestedHash) return;
				if (bands == nullptr || bands->empty()) return;
				for (auto* display : self->displays) {
					display->setBandData(bands);
				}
			});
	}

	// Load beat grid config for this track on a worker thread to avoid
	// blocking the message thread on JSON disk I/O.
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
		loadingLabel.setText("Loading...", juce::dontSendNotification);

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
	juce::Colour offColour = UI::bgRoot;
	if (isSynced && outOfRange)
		offColour = UI::accentWarning.withAlpha(0.6f);
	syncEngageBtn.setColour(juce::TextButton::buttonColourId, offColour);
	// fastSyncBtn is an icon DrawableButton - the normalImageOn (green bolt)
	// is shown via toggle state. Also set backgroundColourId so any
	// circularOutline LookAndFeel path reflects the active colour.
	juce::Colour fastSyncBg = juce::Colours::transparentBlack;
	if (isSynced && outOfRange)
		fastSyncBg = UI::accentWarning.withAlpha(0.6f);
	else if (isSynced)
		fastSyncBg = UI::accentPositive.withAlpha(0.85f);
	fastSyncBtn.setColour(juce::DrawableButton::backgroundColourId, fastSyncBg);
	fastSyncBtn.repaint();

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
	// visual range; we don't try to mirror it on the slider - targetBpmLabel
	// shows the resolved BPM instead.
	// Keep the slider enabled and mirror the engine ratio onto it (slider
	// auto-clamps to its visual range). Any user interaction will fire
	// sliderValueChanged, where we then disengage sync - so the slider acts
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
				UI::bgRoot);
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
			UI::accentWarning.withAlpha(0.7f));
	}
	else {
		btn->setColour(juce::TextButton::buttonColourId,
			UI::accentWarning.withAlpha(0.8f));
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
 * Tiles are added as child components but kept hidden initially - the HotCues
 * tab is the default selected tab.
 */
void DeckGUI::buildFxPanels()
{
	const auto themeMid = juce::Colours::black;

	// ---- Pad FX tiles --------------------------------------------------------
	{
		auto procs = FxFactory::buildCategory(FxCategory::Pad);
		// Tile 0 of FxFactory output is "None" - we skip it for the display
		// grid but keep its index inside the chain.
		const int kTiles = 8;
		padFxTiles.reserve(kTiles);
		for (int slot = 0; slot < kTiles; ++slot) {
			auto tile = std::make_unique<MomentaryFxTile>();
			int procIdx = slot + 1; // skip None at index 0
			juce::String label = "-";
			if (procIdx < (int) procs.size()) {
				label = procs[procIdx]->getName();
			}
			tile->setButtonText(label);
			tile->setColour(juce::TextButton::buttonColourId, themeMid);
			tile->setColour(juce::TextButton::buttonOnColourId, theme.withAlpha(0.85f));
			tile->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
			tile->setColour(juce::TextButton::textColourOnId,  juce::Colours::black);
			tile->setLookAndFeel(&customLookAndFeel);

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

			transportContainer.addChildComponent(*tile);
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
		beatFxSelector.setColour(juce::ComboBox::backgroundColourId, UI::bgRoot);
		beatFxSelector.setColour(juce::ComboBox::textColourId, juce::Colours::white);
		beatFxSelector.setColour(juce::ComboBox::outlineColourId, theme.withAlpha(0.5f));
		beatFxSelector.setLookAndFeel(&customLookAndFeel);
		beatFxSelector.onChange = [this]() {
			int idx = beatFxSelector.getSelectedId() - 1;
			if (idx >= 0) postFxSelect(FxCategory::Beat, idx);
		};
		transportContainer.addChildComponent(beatFxSelector);

		// Beat division: maps 1..7 → 1/16, 1/8, 1/4, 1/2, 1, 2, 4 beats.
		beatFxDivisionBox.clear(juce::dontSendNotification);
		const char* divNames[7] = { "1/16", "1/8", "1/4", "1/2", "1 BEAT", "2 BEAT", "4 BEAT" };
		for (int i = 0; i < 7; ++i)
			beatFxDivisionBox.addItem(divNames[i], i + 1);
		beatFxDivisionBox.setSelectedId(3, juce::dontSendNotification); // 1/4 default
		beatFxDivisionBox.setColour(juce::ComboBox::backgroundColourId, UI::bgRoot);
		beatFxDivisionBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
		beatFxDivisionBox.setColour(juce::ComboBox::outlineColourId, theme.withAlpha(0.5f));
		beatFxDivisionBox.setLookAndFeel(&customLookAndFeel);
		beatFxDivisionBox.onChange = [this]() {
			// Atomic parameter write - bypass FIFO.
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
		transportContainer.addChildComponent(beatFxDivisionBox);

		beatFxOnButton.setClickingTogglesState(true);
		beatFxOnButton.addListener(this);
		beatFxOnButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxOnButton.setColour(juce::TextButton::buttonOnColourId, theme.withAlpha(0.85f));
		beatFxOnButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		beatFxOnButton.setColour(juce::TextButton::textColourOnId,  juce::Colours::black);
		beatFxOnButton.setLookAndFeel(&customLookAndFeel);
		transportContainer.addChildComponent(beatFxOnButton);

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
		transportContainer.addChildComponent(beatFxWetSlider);

		beatFxWetLabel.setJustificationType(juce::Justification::centred);
		beatFxWetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
		transportContainer.addChildComponent(beatFxWetLabel);

		beatFxEditButton.addListener(this);
		beatFxEditButton.setColour(juce::TextButton::buttonColourId, UI::bgRoot);
		beatFxEditButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		beatFxEditButton.setLookAndFeel(&customLookAndFeel);
		transportContainer.addChildComponent(beatFxEditButton);
	}

	// ---- Release FX tiles ----------------------------------------------------
	{
		auto procs = FxFactory::buildCategory(FxCategory::Release);
		const int kTiles = 3;
		releaseFxTiles.reserve(kTiles);
		for (int slot = 0; slot < kTiles; ++slot) {
			auto tile = std::make_unique<MomentaryFxTile>();
			int procIdx = slot + 1;
			juce::String label = "-";
			if (procIdx < (int) procs.size()) label = procs[procIdx]->getName();
			tile->setButtonText(label);
			tile->setColour(juce::TextButton::buttonColourId, themeMid);
			tile->setColour(juce::TextButton::buttonOnColourId, theme.withAlpha(0.85f));
			tile->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
			tile->setColour(juce::TextButton::textColourOnId,  juce::Colours::black);
			tile->setLookAndFeel(&customLookAndFeel);

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
			transportContainer.addChildComponent(*tile);
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
 * Public wrapper for loadDeck - used by external library sidebars.
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

/**
 * Update the CUE button's visual state to reflect whether this deck is
 * currently routed to the headphone output.
 */
void DeckGUI::setCueActive(bool active) noexcept
{
	// Drive the toggle state so DrawableButton switches to the theme-tinted
	// normalImageOn (set in the constructor via setImages).
	cueButton.setToggleState(active, juce::dontSendNotification);
	// Belt-and-suspenders: also set the background colour so any
	// circularOutline path in the LookAndFeel lights up correctly.
	cueButton.setColour(juce::DrawableButton::backgroundColourId,
	                    active ? theme.withAlpha(0.85f)
	                           : juce::Colours::transparentBlack);
	cueButton.repaint();
}

//==============================================================================
