#include "MainComponent.h"
#include "FxSettings.h"

//==============================================================================

/**
 * Implementation of a constructor for MainComponent
 *
 * In the constructor, graphic component data members are made visible here.
 * Initial component configurations are performed here, such as
 * registering basic formats, configuring look and feel properties of components,
 * and adding component listeners.
 *
 */
MainComponent::MainComponent()
{
	setSize(800, 600);

	// Some platforms require permissions to open input channels so request that here
	if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
		&& !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
	{
		juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
			[&](bool granted) { setAudioChannels(granted ? 2 : 0, 2); });
	}
	else
	{
		// Specify the number of input and output channels that we want to open
		setAudioChannels(2, 2);
	}


	addAndMakeVisible(deckGUI1);
	addAndMakeVisible(deckGUI2);
	addAndMakeVisible(zoomedDisplay1);
	addAndMakeVisible(zoomedDisplay2);
	addAndMakeVisible(crossFader);

	// Per-deck library sidebars. Slide in from the opposite side of the deck
	// when the user clicks that deck's load button.
	auto loadCb1 = [this](const track& t) { deckGUI1.loadTrack(t); closeSidebar(0); };
	auto queueCb1 = [this](const track& t) { deckGUI1.enqueueTrack(t); };
	auto closeCb1 = [this]() { closeSidebar(0); };

	auto loadCb2 = [this](const track& t) { deckGUI2.loadTrack(t); closeSidebar(1); };
	auto queueCb2 = [this](const track& t) { deckGUI2.enqueueTrack(t); };
	auto closeCb2 = [this]() { closeSidebar(1); };

	sidebar1 = std::make_unique<DeckLibrarySidebar>(library, UI::deck1Accent, 0,
	                                                std::move(loadCb1), std::move(queueCb1), std::move(closeCb1));
	sidebar2 = std::make_unique<DeckLibrarySidebar>(library, UI::deck2Accent, 1,
	                                                std::move(loadCb2), std::move(queueCb2), std::move(closeCb2));

	addChildComponent(*sidebar1);
	addChildComponent(*sidebar2);

	deckGUI1.onLoadButtonClicked = [this](int) { openSidebar(0); };
	deckGUI2.onLoadButtonClicked = [this](int) { openSidebar(1); };

	// Wire the CUE buttons: toggling routes that deck to the headphone output.
	auto cueCb = [this](int deckIndex)
	{
		const int current = audioEngine.getCuedDeckIndex();
		if (current == deckIndex)
		{
			// Same deck pressed again → turn off cue.
			audioEngine.setCueDeck(-1);
			deckGUI1.setCueActive(false);
			deckGUI2.setCueActive(false);
		}
		else
		{
			audioEngine.setCueDeck(deckIndex);
			deckGUI1.setCueActive(deckIndex == 0);
			deckGUI2.setCueActive(deckIndex == 1);
		}
	};
	deckGUI1.onCueButtonClicked = cueCb;
	deckGUI2.onCueButtonClicked = cueCb;

	// Headphone / cue output device manager.
	cueCallback.setEngine(&audioEngine);
	{
		auto savedState = AppSettings::loadHeadphoneDeviceState();
		cueDeviceManager.initialise(0, 2, savedState.get(), false);
	}

	cueDeviceManager.addAudioCallback(&cueCallback);

	// Settings panel (hidden off-screen until the gear button is pressed).
	settingsPanel = std::make_unique<SettingsPanel>(
		deviceManager,
		cueDeviceManager,
		[this]() { closeSettings(); });
	addChildComponent(*settingsPanel);

	// Settings gear — DrawableButton with SVG icons (normal + hover).
	{
		auto normalImg = juce::Drawable::createFromImageData(BinaryData::iconSettings_svg, BinaryData::iconSettings_svgSize);
		auto overImg   = juce::Drawable::createFromImageData(BinaryData::iconSettingsHover_svg, BinaryData::iconSettingsHover_svgSize);
		if (normalImg != nullptr)
			normalImg->replaceColour(juce::Colours::white, juce::Colour::fromRGB(200, 200, 200));
		// Hover variant ships in white — keep it for the brighter hover state.
		settingsButton.setImages(normalImg.get(), overImg.get(), overImg.get(), nullptr,
		                         normalImg.get(), overImg.get(), overImg.get(), nullptr);
		settingsButton.setEdgeIndent(2);
	}
	settingsButton.setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
	settingsButton.setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
	settingsButton.setTooltip("Audio device settings");
	settingsButton.onClick = [this]() { openSettings(); };
	addAndMakeVisible(settingsButton);

	// Wire decks into the beat-sync manager (decks already know about it via
	// constructor injection in MainComponent.h).
	beatSyncManager.setDeck(0, &audioEngine.getPlayer(0), &deckGUI1);
	beatSyncManager.setDeck(1, &audioEngine.getPlayer(1), &deckGUI2);

	crossFader.setRange(-1, 1);
	crossFader.setValue(0);
	crossFader.addListener(this);
	crossFader.addMouseListener(this, false);

	// ── Mixer column: volume faders ───────────────────────────────────────────
	vol1Slider.setRange(0.0, 1.0);
	vol1Slider.setSkewFactorFromMidPoint(0.25);
	vol1Slider.setValue(1.0);
	vol1Slider.setColour(juce::Slider::thumbColourId, UI::deck1Accent);
	vol1Slider.addListener(this);
	addAndMakeVisible(vol1Slider);

	vol2Slider.setRange(0.0, 1.0);
	vol2Slider.setSkewFactorFromMidPoint(0.25);
	vol2Slider.setValue(1.0);
	vol2Slider.setColour(juce::Slider::thumbColourId, UI::deck2Accent);
	vol2Slider.addListener(this);
	addAndMakeVisible(vol2Slider);

	vol1Label.setText("VOL", juce::dontSendNotification);
	vol1Label.setJustificationType(juce::Justification::centred);
	vol1Label.setFont(juce::Font(juce::FontOptions(10.0f)));
	vol1Label.setColour(juce::Label::textColourId, juce::Colours::grey);
	addAndMakeVisible(vol1Label);

	vol2Label.setText("VOL", juce::dontSendNotification);
	vol2Label.setJustificationType(juce::Justification::centred);
	vol2Label.setFont(juce::Font(juce::FontOptions(10.0f)));
	vol2Label.setColour(juce::Label::textColourId, juce::Colours::grey);
	addAndMakeVisible(vol2Label);

	// ── Mixer column: filter knobs ────────────────────────────────────────────
	filter1Slider.setRange(-20000.0, 20000.0);
	filter1Slider.setValue(0.0);
	filter1Slider.setColour(juce::Slider::thumbColourId, UI::deck1Accent);
	filter1Slider.addListener(this);
	addAndMakeVisible(filter1Slider);

	filter2Slider.setRange(-20000.0, 20000.0);
	filter2Slider.setValue(0.0);
	filter2Slider.setColour(juce::Slider::thumbColourId, UI::deck2Accent);
	filter2Slider.addListener(this);
	addAndMakeVisible(filter2Slider);

	filter1Label.setText("FILTER", juce::dontSendNotification);
	filter1Label.setJustificationType(juce::Justification::centred);
	filter1Label.setFont(juce::Font(juce::FontOptions(10.0f)));
	filter1Label.setColour(juce::Label::textColourId, juce::Colours::grey);
	addAndMakeVisible(filter1Label);

	filter2Label.setText("FILTER", juce::dontSendNotification);
	filter2Label.setJustificationType(juce::Justification::centred);
	filter2Label.setFont(juce::Font(juce::FontOptions(10.0f)));
	filter2Label.setColour(juce::Label::textColourId, juce::Colours::grey);
	addAndMakeVisible(filter2Label);

	// Apply CustomLookAndFeel and per-deck accent colours to the mixer sliders.
	vol1Slider.setLookAndFeel(&customLookAndFeel);
	vol2Slider.setLookAndFeel(&customLookAndFeel);
	filter1Slider.setLookAndFeel(&customLookAndFeel);
	filter2Slider.setLookAndFeel(&customLookAndFeel);
	filter1Slider.setColour(juce::Slider::rotarySliderFillColourId, UI::deck1Accent);
	filter2Slider.setColour(juce::Slider::rotarySliderFillColourId, UI::deck2Accent);

	// Enable right-click reset on mixer sliders.
	vol1Slider.addMouseListener(this, false);
	vol2Slider.addMouseListener(this, false);
	filter1Slider.addMouseListener(this, false);
	filter2Slider.addMouseListener(this, false);

	startTimer(20); // ~50 fps repaint for volume meters

	formatManager.registerBasicFormats();

	getLookAndFeel().setColour(juce::ResizableWindow::backgroundColourId, UI::bgRoot);

	crossFader.setLookAndFeel(&customLookAndFeel);
	library.setLookAndFeel(&customLookAndFeel);
	library.addKeyListener(this);
	addKeyListener(this);
	setWantsKeyboardFocus(true);
}

/**
 * Implementation of a destructor for MainComponent
 *
 * Shuts down the audio device and clears the audio source.
 */
MainComponent::~MainComponent()
{
	stopTimer();
	cueDeviceManager.removeAudioCallback(&cueCallback);
	shutdownAudio();
}

//==============================================================================

/**
 * Implementation of prepareToPlay method for MainComponent
 *
 * Calls prepareToPlay methods on all AudioSource data members and adds audio sources
 * in the MainComponentLevel to the MixerAudioSource
 *
 */
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
	audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);

	// Restore persisted FX settings once both decks' FxChains are prepared.
	// Subsequent prepareToPlay calls (device changes) won't re-load — guarded
	// by a static flag.
	static bool fxSettingsLoaded = false;
	if (! fxSettingsLoaded) {
		fxSettingsLoaded = true;
		FxSettings::loadInto(audioEngine.getPlayer(0).getFxChain(),
		                     audioEngine.getPlayer(1).getFxChain());
	}
}

/**
 * Implementation of getNextAudioBlock method for MainComponent
 *
 * Delegates to AudioEngine which owns the mixer + both players.
 *
 */
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	audioEngine.getNextAudioBlock(bufferToFill);
}

/**
 * Implementation of releaseResources method for MainComponent
 *
 * Delegates to AudioEngine.
 *
 */
void MainComponent::releaseResources()
{
	audioEngine.releaseResources();
}

//==============================================================================

/**
 * Implementation of paint method for MainComponent
 *
 * Sets global background
 */
void MainComponent::paint(juce::Graphics& g)
{
	g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

	// Dedicated mixer card: a rounded card strip centred between the two decks.
	constexpr int kMixerW    = 200;
	constexpr int kDeckBaseY = 150;
	const int     deckY      = kDeckBaseY + getHeight() / 16;

	juce::Rectangle<float> mixCard(
		(float)(getWidth() / 2 - kMixerW / 2),
		(float)deckY,
		(float)kMixerW,
		(float)(getHeight() - deckY));
	CustomLookAndFeel::paintCardBackground(g, mixCard, UI::kCardRadius);

	// Erase the card background behind the crossfader so it sits flush.
	g.setColour(UI::bgRoot);
	g.fillRect(crossFader.getBounds());

	// ── Volume meters (drawn beside each fader, updated at 50 fps via timer) ──
	auto drawMeter = [&](const juce::Slider& slider, DJAudioPlayer& player, bool rightSide)
	{
		const auto  sb      = slider.getBounds();
		constexpr float kMeterW = 8.0f;
		const float meterX  = rightSide ? (float)sb.getRight() + 3.0f
		                                 : (float)sb.getX() - kMeterW - 3.0f;
		const float meterTop = (float)sb.getY();
		const float meterH   = (float)sb.getHeight();

		// Map RMS (dB, −60→0) to a Y threshold: 0 dB = top, −60 dB = bottom.
		const float rms       = player.getRMSLevel();
		const float threshold = juce::jmap(rms, -60.0f, 0.0f,
		                                   meterTop + meterH, meterTop);

		constexpr int kSegments = 12;
		const float   segH      = meterH / kSegments;
		for (int i = 0; i < kSegments; ++i)
		{
			// i=0 = bottom segment (green), i=kSegments-1 = top (red)
			const float posY   = meterTop + meterH - (i + 1) * segH;
			const float frac   = (float)i / (float)(kSegments - 1);
			const float r      = frac * 255.0f;
			const juce::Colour seg(juce::uint8(r), juce::uint8(255.0f - r), juce::uint8(0));
			// Segment is active when its centre Y is below (larger than) the threshold.
			const bool  active = (posY + segH * 0.5f) > threshold;
			g.setColour(active ? seg.withAlpha(0.85f) : UI::bgRoot);
			g.fillRect(meterX, posY + 1.0f, kMeterW, segH - 2.0f);
		}
	};
	drawMeter(vol1Slider, audioEngine.getPlayer(0), true);
	drawMeter(vol2Slider, audioEngine.getPlayer(1), false);
}

/**
 * Implementation of resized method for MainComponent
 *
 * All juce::Component data members call it's setBounds method to achieve coherent space and sizing.
 *
 */
void MainComponent::resized()
{
	DBG("MainComponent::resized");

	// ── Shared layout constants ───────────────────────────────────────────────
	constexpr int kZoomBaseH       = 75;   // zoomed waveform base height
	constexpr int kZoomGrowDivisor = 32;   // grow zoom strip slowly with window height
	constexpr int kDeckBaseY       = 150;  // top of deck panel (below 2 zoom strips)
	constexpr int kDeckGrowDivisor = 16;
	constexpr int kMixerW          = 200;  // mixer column width (matches paint() card)
	constexpr int kCrossFaderW     = 160;
	constexpr int kCrossFaderH     = 37;
	constexpr int kSettingsBtnSize = 32;   // gear button (larger for better hit area)

	const int zoomH = kZoomBaseH + getHeight() / kZoomGrowDivisor;
	const int deckY = kDeckBaseY + getHeight() / kDeckGrowDivisor;
	const int deckH = getHeight() - deckY;

	// ── Waveform strips — full width ──────────────────────────────────────────
	zoomedDisplay1.setBounds(0, 0,     getWidth(), zoomH);
	zoomedDisplay2.setBounds(0, zoomH, getWidth(), zoomH);

	// ── Deck panels — carve out the centre mixer column ───────────────────────
	const int deckW = (getWidth() - kMixerW) / 2;
	deckGUI1.setBounds(0,              deckY, deckW, deckH);
	deckGUI2.setBounds(deckW + kMixerW, deckY, deckW, deckH);

	// ── Mixer column layout ───────────────────────────────────────────────────
	const int mixX    = getWidth() / 2 - kMixerW / 2;
	const int halfCol = kMixerW / 2;   // 100 px — left = Deck 1, right = Deck 2

	// Settings gear — centred at top of mixer column.
	settingsButton.setBounds(mixX + halfCol - kSettingsBtnSize / 2,
	                         deckY + 10,
	                         kSettingsBtnSize, kSettingsBtnSize);

	// Crossfader — bottom-anchored with generous gap above.
	const int kCrossFaderY = deckY + deckH - kCrossFaderH - 20;
	crossFader.setBounds(getWidth() / 2 - kCrossFaderW / 2, kCrossFaderY,
	                     kCrossFaderW, kCrossFaderH);

	// Vol faders + filter knobs fill the space between settings button and crossfader.
	constexpr int kSliderW   = 20;
	constexpr int kKnobSize  = 50;
	constexpr int kLabelH    = 14;

	const int volSliderTop = deckY + 10 + kSettingsBtnSize + 16;
	// Fixed height consumed below the vol slider: label + gap + knob + label + padding.
	const int fixedBelow   = kLabelH + 8 + kKnobSize + kLabelH + 12;
	const int volSliderH   = std::max(kCrossFaderY - volSliderTop - fixedBelow, 40);
	const int volLabelY    = volSliderTop + volSliderH + 4;
	const int filterKnobY  = volLabelY + kLabelH + 8;
	const int filterLabelY = filterKnobY + kKnobSize + 4;

	// Deck 1 — left half of mixer (centre at mixX + halfCol/2).
	const int col1 = mixX + halfCol / 2;
	vol1Slider.setBounds   (col1 - kSliderW / 2,  volSliderTop, kSliderW,  volSliderH);
	vol1Label.setBounds    (col1 - 20,             volLabelY,    40,        kLabelH);
	filter1Slider.setBounds(col1 - kKnobSize / 2,  filterKnobY,  kKnobSize, kKnobSize);
	filter1Label.setBounds (col1 - kKnobSize / 2,  filterLabelY, kKnobSize, kLabelH);

	// Deck 2 — right half of mixer (centre at mixX + halfCol + halfCol/2).
	const int col2 = mixX + halfCol + halfCol / 2;
	vol2Slider.setBounds   (col2 - kSliderW / 2,  volSliderTop, kSliderW,  volSliderH);
	vol2Label.setBounds    (col2 - 20,             volLabelY,    40,        kLabelH);
	filter2Slider.setBounds(col2 - kKnobSize / 2,  filterKnobY,  kKnobSize, kKnobSize);
	filter2Label.setBounds (col2 - kKnobSize / 2,  filterLabelY, kKnobSize, kLabelH);

	// ── Settings panel (full-width, slides from top) ──────────────────────────
	if (settingsPanel != nullptr)
	{
		if (settingsPanel->isVisible())
			settingsPanel->setBounds(settingsPanelOpenBounds());
		else
			settingsPanel->setBounds(settingsPanelClosedBounds());
	}

	// ── Sidebars ──────────────────────────────────────────────────────────────
	auto fitSidebar = [this](DeckLibrarySidebar* s, int idx)
	{
		if (s == nullptr) return;
		if (s->isVisible())
			s->setBounds(sidebarOpenBounds(idx));
		else
			s->setBounds(sidebarClosedBounds(idx));
	};
	fitSidebar(sidebar1.get(), 0);
	fitSidebar(sidebar2.get(), 1);
}

//==============================================================================

juce::Rectangle<int> MainComponent::sidebarOpenBounds(int deckIndex) const
{
	const int w = getWidth() / 2;
	const int h = getHeight();
	// Deck 0 (left) → sidebar opens on the RIGHT half. Deck 1 (right) → LEFT half.
	if (deckIndex == 0)
		return { getWidth() - w, 0, w, h };
	return { 0, 0, w, h };
}

juce::Rectangle<int> MainComponent::sidebarClosedBounds(int deckIndex) const
{
	const int w = getWidth() / 2;
	const int h = getHeight();
	if (deckIndex == 0)
		return { getWidth(), 0, w, h };   // off-screen right
	return { -w, 0, w, h };               // off-screen left
}

void MainComponent::openSidebar(int deckIndex)
{
	DeckLibrarySidebar* target = (deckIndex == 0 ? sidebar1.get() : sidebar2.get());
	DeckLibrarySidebar* other  = (deckIndex == 0 ? sidebar2.get() : sidebar1.get());
	if (target == nullptr) return;

	// Mutual exclusion: close the peer first.
	if (other != nullptr && other->isVisible())
		closeSidebar(deckIndex == 0 ? 1 : 0);

	target->setBounds(sidebarClosedBounds(deckIndex));
	target->setVisible(true);
	target->toFront(false);
	juce::Desktop::getInstance().getAnimator().animateComponent(
		target, sidebarOpenBounds(deckIndex), 1.0f, 220, false, 1.0, 0.0);
}

void MainComponent::closeSidebar(int deckIndex)
{
	DeckLibrarySidebar* target = (deckIndex == 0 ? sidebar1.get() : sidebar2.get());
	if (target == nullptr || ! target->isVisible()) return;

	juce::Desktop::getInstance().getAnimator().animateComponent(
		target, sidebarClosedBounds(deckIndex), 1.0f, 220, false, 1.0, 0.0);

	// Defer hiding until the slide-out finishes.
	juce::Component::SafePointer<DeckLibrarySidebar> safe(target);
	juce::Timer::callAfterDelay(240, [safe]() {
		if (auto* s = safe.getComponent()) s->setVisible(false);
	});
}

//==============================================================================
// Settings panel

juce::Rectangle<int> MainComponent::settingsPanelOpenBounds() const
{
	return { 0, 0, getWidth(), juce::jmin(getHeight(), 380) };
}

juce::Rectangle<int> MainComponent::settingsPanelClosedBounds() const
{
	return { 0, -380, getWidth(), 380 };
}

void MainComponent::openSettings()
{
	if (settingsPanel == nullptr) return;
	if (settingsPanel->isVisible()) return;

	settingsPanel->setBounds(settingsPanelClosedBounds());
	settingsPanel->setVisible(true);
	settingsPanel->toFront(false);
	juce::Desktop::getInstance().getAnimator().animateComponent(
		settingsPanel.get(), settingsPanelOpenBounds(), 1.0f, 220, false, 1.0, 0.0);
}

void MainComponent::closeSettings()
{
	if (settingsPanel == nullptr || !settingsPanel->isVisible()) return;

	juce::Desktop::getInstance().getAnimator().animateComponent(
		settingsPanel.get(), settingsPanelClosedBounds(), 1.0f, 220, false, 1.0, 0.0);

	juce::Component::SafePointer<SettingsPanel> safe(settingsPanel.get());
	juce::Timer::callAfterDelay(240, [safe]() {
		if (auto* p = safe.getComponent()) p->setVisible(false);
	});
}

void MainComponent::timerCallback()
{
	// Repaint only the mixer column to update the volume meters efficiently.
	constexpr int kMixerW    = 200;
	constexpr int kDeckBaseY = 150;
	const int     deckY      = kDeckBaseY + getHeight() / 16;
	repaint(getWidth() / 2 - kMixerW / 2, deckY, kMixerW, getHeight() - deckY);
}

//==============================================================================

/**
 * Implementation of sliderValueChanged method for MainComponent
 *
 * juce::Slider cross fader is compared to the triggered juce::Slider pointer.
 * If the cross fader called this function, calls a setGain method in the
 * DJAudioPlayer instances. The gain set is inversely proportional to the distance
 * of the knob from the deck.
 *
 */
void MainComponent::mouseDown(const juce::MouseEvent& event)
{
	if (!event.mods.isPopupMenu()) return;

	if (event.eventComponent == &crossFader) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to centre");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1)
					crossFader.setValue(0.0, juce::sendNotification);
			});
	}

	if (event.eventComponent == &vol1Slider) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to maximum");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1)
					vol1Slider.setValue(1.0, juce::sendNotification);
			});
	}

	if (event.eventComponent == &vol2Slider) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to maximum");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1)
					vol2Slider.setValue(1.0, juce::sendNotification);
			});
	}

	if (event.eventComponent == &filter1Slider) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to centre");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1)
					filter1Slider.setValue(0.0, juce::sendNotification);
			});
	}

	if (event.eventComponent == &filter2Slider) {
		juce::PopupMenu menu;
		menu.addItem(1, "Reset to centre");
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this](int result) {
				if (result == 1)
					filter2Slider.setValue(0.0, juce::sendNotification);
			});
	}
}

void MainComponent::sliderValueChanged(juce::Slider* slider) {
	if (slider == &crossFader) {
		double val;
		if (slider->getValue() > 0) {
			val = 1 - slider->getValue();
			audioEngine.getPlayer(0).setGain(val, false);
			audioEngine.getPlayer(1).setGain(1, false);
		}
		else if (slider->getValue() < 0) {
			val = 1 + slider->getValue();
			audioEngine.getPlayer(1).setGain(val, false);
			audioEngine.getPlayer(0).setGain(1, false);
		}
	}
	else if (slider == &vol1Slider) {
		audioEngine.getPlayer(0).setGain(slider->getValue());
	}
	else if (slider == &vol2Slider) {
		audioEngine.getPlayer(1).setGain(slider->getValue());
	}
	else if (slider == &filter1Slider) {
		audioEngine.getPlayer(0).setFilter(slider->getValue());
	}
	else if (slider == &filter2Slider) {
		audioEngine.getPlayer(1).setFilter(slider->getValue());
	}
}

//==============================================================================

/**
 * Implementation of keyPressed method for MainComponent
 *
 * Checks if the key pressed is the 'd' key.
 * If so calls on the library to delete an item.
 *
 */
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) {
	DBG(key.getKeyCode());
	if (key.getKeyCode() == 68) {
		DBG("Delete Match");
		library.deleteItem();
	}
	return true;
};

//==============================================================================



