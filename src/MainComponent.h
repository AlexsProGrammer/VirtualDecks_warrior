#pragma once

#include <JuceHeader.h>
#include "core/audio/DJAudioPlayer.h"
#include "core/audio/AudioEngine.h"
#include "ui/decks/DeckGUI.h"
#include "core/data/Library.h"
#include "ui/settings/CustomLookAndFeel.h"
#include "core/analysis/BeatSyncManager.h"
#include "core/midi/MidiMapper.h"
#include "core/data/MidiMappings.h"
#include "ui/decks/DeckLibrarySidebar.h"
#include "utilities/CueAudioCallback.h"
#include "core/data/AppSettings.h"
#include "ui/settings/SettingsPanel.h"

struct MidiFeedbackState
{
    bool playing[2] = { false, false };
    bool cueActive[2] = { false, false };
    bool beatFxOn[2] = { false, false };
    bool loopActive[2] = { false, false };
    bool sync[2] = { false, false };
    bool master[2] = { false, false };
    bool hotCues[2][6] = { { false, false, false, false, false, false }, { false, false, false, false, false, false } };
    float vuValue[2] = { -999.0f, -999.0f };
};

//==============================================================================
/*
	This component lives inside our window, and this is where you should put all
	your controls and content.
*/
class MainComponent : public juce::AudioAppComponent,
                      public juce::Slider::Listener,
                      public juce::KeyListener,
                      public juce::Timer
{
public:
	//==============================================================================

	/**
		* Class Constructor for MainComponent, initializes member variables and configures component details.
	*/
	MainComponent();

	/**
		* Class Destructor for MainComponent, initializes member variables and configures component details.
	*/
	~MainComponent() override;

	//==============================================================================

	/**
		* Called when key is pressed.
		*
		* @param juce::KeyPress object
		* @param Component that has added this MainComponent as its listener
	*/
	bool keyPressed(const juce::KeyPress& key, Component* originatingComponent);

	/// Handle a decoded MIDI action from the MIDI mapper.
	void onMidiAction(Midi::MidiActionTarget target, int value, bool isPress);

	/// Toggle headphone cue routing for the requested deck.
	void toggleCue(int deckIndex);

	/// Start the hold-to-preview transport cue on the requested deck.
	void startCuePreview(int deckIndex);

	/// Stop the hold-to-preview transport cue and restore prior headphone routing.
	void stopCuePreview(int deckIndex);

	/// Saved deck index before a preview started, used to restore headphone routing.
	int cuePreviewSavedDeckIndex { -1 };

	/// True while a hold-to-preview cue is active.
	bool cuePreviewActive { false };
	//==============================================================================

	/**
		* Prepares audio source members
		*
		* @param Expected samples in a block
		* @param Number of samples per second
	*/
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

	/**
		* Called repeatedly to fetch subsequent blocks of audio data.
		*
		* @param juce::AudioSourceChannelInfo&: Buffer to be filled by audio source
	*/
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

	/**
		* Release resources on audio sources.
	*/
	void releaseResources() override;
	//==============================================================================

	/**
		* Called periodically to repaint the mixer column volume meters.
	*/
	void timerCallback() override;

	//==============================================================================

	/**
		* Paints the MainComponent.
		*
		* @param juce::Graphics object for the component to draw itself on
	*/
	void paint(juce::Graphics& g) override;

	/**
		* Set bounds of member components
	*/
	void resized() override;
	//==============================================================================

	/**
		* Called when slider knob in MainComponent listener is dragged.
		*
		* @param juce::Slider object that has added this component as its listener
	*/
	void sliderValueChanged(juce::Slider* slider) override;

	//==============================================================================

	/**
		* Right-click context menu for resettable sliders (crossfader).
	*/
	void mouseDown(const juce::MouseEvent& event) override;

	//==============================================================================

private:
	//==============================================================================

	/// Instance of CustomLookAndFeel class.
	CustomLookAndFeel customLookAndFeel;

	/// Instance of AudioFormatManager class. Declared BEFORE library so Library's
	/// constructor reference is bound to a fully-constructed object.
	juce::AudioFormatManager formatManager;

	/// Instance of Library class.
	Library library{ formatManager };

	/// Instance of AudioThumbnailCache class.
	juce::AudioThumbnailCache thumbCache{ 100 };

	/// AudioEngine: owns both DJAudioPlayers, the MixerAudioSource, and the
	/// off-thread track-loading pool.
	AudioEngine audioEngine{ formatManager };

	/// Instance of ZoomedWaveform class for the left DJ Deck's audio track.
	ZoomedWaveform zoomedDisplay1{ formatManager, thumbCache, UI::deck1Accent };

	/// Instance of ZoomedWaveform class for the right DJ Deck's audio track.
	ZoomedWaveform zoomedDisplay2{ formatManager, thumbCache, UI::deck2Accent };

	/// Cross-deck beat-sync manager (master/slave coordinator).
	BeatSyncManager beatSyncManager;

	/// MIDI input mapper for controller actions.
	Midi::MidiMapper midiMapper;

	/// Whether the app should start playback at the first saved hot cue.
	bool startAtFirstHotCueSetting = false;

	/// EQ filter chain lock state per band (0=Low, 1=Mid, 2=High).
	/// When locked, moving one deck's knob mirrors the other with inverted movement.
	bool filterChainLocked[3] = { false, false, false };

	/// Baseline filter values captured when chain lock is engaged, per deck per band.
	/// Used for delta-based mirroring: when knob moves, calculate delta from baseline
	/// and apply the inverse delta to the other deck's baseline.
	double chainBaseline[2][3] = { { 1.0, 1.0, 1.0 }, { 1.0, 1.0, 1.0 } };

	/// Instance of DeckGUI class for the left DJ Deck.
	DeckGUI deckGUI1{ &audioEngine.getPlayer(0), formatManager, thumbCache,&zoomedDisplay1 , library, UI::deck1Accent, &beatSyncManager, 0, &audioEngine };

	/// Instance of DeckGUI class for the right DJ Deck.
	DeckGUI deckGUI2{ &audioEngine.getPlayer(1), formatManager, thumbCache,&zoomedDisplay2 , library, UI::deck2Accent, &beatSyncManager, 1, &audioEngine };

	/// Horizontal fader subclass that passes right-click to the parent so the
	/// context-menu handler in MainComponent::mouseDown can show a reset popup.
	struct ResetableFader : public juce::Slider {
		ResetableFader() : juce::Slider(juce::Slider::SliderStyle::LinearHorizontal, juce::Slider::TextEntryBoxPosition::NoTextBox) {}
		void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDown(e); }
		void mouseDrag(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDrag(e); }
	};

	/// Vertical fader subclass that blocks right-click from moving the slider.
	struct ResetableVerticalFader : public juce::Slider {
		ResetableVerticalFader() : juce::Slider(juce::Slider::SliderStyle::LinearVertical, juce::Slider::TextEntryBoxPosition::NoTextBox) {}
		void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDown(e); }
		void mouseDrag(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDrag(e); }
	};

	/// Instance of juce::Slider for cross fading functionality.
	ResetableFader crossFader;

	// ── Mixer column: per-deck volume faders and filter knobs ──────────────────

	/// Volume fader for Deck 1 (linear vertical, range 0–1, log skew).
	ResetableVerticalFader vol1Slider;

	/// Volume fader for Deck 2 (linear vertical, range 0–1, log skew).
	ResetableVerticalFader vol2Slider;

	/// Filter sweep knob for Deck 1 (LP→HP, range −20000…20000).
	juce::Slider filter1Slider{ juce::Slider::SliderStyle::RotaryVerticalDrag,
	                             juce::Slider::TextEntryBoxPosition::NoTextBox };

	/// Filter sweep knob for Deck 2 (LP→HP, range −20000…20000).
	juce::Slider filter2Slider{ juce::Slider::SliderStyle::RotaryVerticalDrag,
	                             juce::Slider::TextEntryBoxPosition::NoTextBox };

	/// Labels for the mixer column controls.
	juce::Label vol1Label, vol2Label, filter1Label, filter2Label;

	/// Per-deck library sidebars. Slide in from the opposite side of the deck.
	std::unique_ptr<DeckLibrarySidebar> sidebar1;
	std::unique_ptr<DeckLibrarySidebar> sidebar2;

	/// Open or close a deck's sidebar with an animated slide.
	void openSidebar(int deckIndex);
	void closeSidebar(int deckIndex);

	/// @return The on-screen and off-screen bounds for the given deck's sidebar.
	juce::Rectangle<int> sidebarOpenBounds (int deckIndex) const;
	juce::Rectangle<int> sidebarClosedBounds(int deckIndex) const;

	//==============================================================================
	// Headphone cue output

	/// Separate device manager for the headphone / cue output channel.
	juce::AudioDeviceManager cueDeviceManager;

	/// Callback that reads the cued deck's ring buffer and feeds the headphone device.
	CueAudioCallback cueCallback;

	/// Slide-in panel for configuring master and headphone output devices.
	std::unique_ptr<SettingsPanel> settingsPanel;

	/// Gear button (SVG-icon) that opens / closes the settings panel above the crossfader.
	juce::DrawableButton settingsButton{ "settings", juce::DrawableButton::ImageFitted };

	/// Animate the settings panel into view from the top.
	void openSettings();

	/// Animate the settings panel out of view.
	void closeSettings();

	/// @return Bounds used when settings panel is fully open.
	juce::Rectangle<int> settingsPanelOpenBounds()  const;

	/// @return Bounds used when settings panel is fully closed (off-screen top).
	juce::Rectangle<int> settingsPanelClosedBounds() const;

	/// Cached RMS levels used to gate timer repaints (only repaint when meters change).
	float lastRms1 = -999.0f;
	/// Cached RMS level for deck 2.
	float lastRms2 = -999.0f;

    /// Tracks MIDI feedback state so only changed boolean targets are resent.
    MidiFeedbackState midiFeedbackState;

    /// Last MIDI output device identifier seen on the timer callback.
    juce::String lastMidiOutputDeviceIdentifier;

    /// Application-wide tooltip window - must be owned by the top-level component.
    juce::TooltipWindow tooltipWindow{ this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

