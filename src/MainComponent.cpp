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

	sidebar1 = std::make_unique<DeckLibrarySidebar>(library, juce::Colours::aqua,    0,
	                                                std::move(loadCb1), std::move(queueCb1), std::move(closeCb1));
	sidebar2 = std::make_unique<DeckLibrarySidebar>(library, juce::Colours::hotpink, 1,
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

	settingsButton.setColour(juce::TextButton::buttonColourId,
	                         juce::Colour::fromRGBA(60, 60, 60, 255));
	settingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
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

	formatManager.registerBasicFormats();

	getLookAndFeel().setColour(juce::ResizableWindow::backgroundColourId, juce::Colour::fromRGBA(25, 25, 25, 255));

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
	g.setFont(20.0f);
	g.setColour(juce::Colour::fromRGBA(25, 25, 25, 255));
	g.fillRect(crossFader.getLocalBounds());
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
	double rowH = getHeight() / 8;

	const int zoomH  = 75 + getHeight() / 32;
	const int deckY  = 150 + getHeight() / 16;
	const int deckH  = getHeight() - deckY;

	zoomedDisplay1.setBounds(0, 0,        getWidth(),       zoomH);
	zoomedDisplay2.setBounds(0, zoomH,    getWidth(),       zoomH);
	deckGUI1.setBounds(0,              deckY, getWidth() / 2, deckH);
	deckGUI2.setBounds(getWidth() / 2, deckY, getWidth() / 2, deckH);
	// Crossfader: vertically track the capped rowH used by DeckGUI so it
	// always sits between the filter knobs and filter labels.
	const double deckRowH = std::min(deckH / 9.0, 40.0);
	crossFader.setBounds(getWidth() / 2 - 80, (int)(deckY + deckRowH * 6.5), 160, 37);

	// Gear / settings button — centred, just above the crossfader.
	settingsButton.setBounds(getWidth() / 2 - 13, (int)(deckY + deckRowH * 6.5) - 30, 26, 26);

	// Settings panel: full-width strip at the top when open, parked off-screen when closed.
	if (settingsPanel != nullptr)
	{
		if (settingsPanel->isVisible())
			settingsPanel->setBounds(settingsPanelOpenBounds());
		else
			settingsPanel->setBounds(settingsPanelClosedBounds());
	}

	// Sidebars track their visibility-driven position. If currently visible,
	// reposition them to the open bounds; otherwise park them off-screen.
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



