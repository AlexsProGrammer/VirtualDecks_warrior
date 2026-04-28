#pragma once

#include <JuceHeader.h>
#include "DJAudioPlayer.h"
#include "AudioEngine.h"
#include "DeckGUI.h"
#include "Library.h"
#include "CustomLookAndFeel.h"
#include "BeatSyncManager.h"
#include "DeckLibrarySidebar.h"
#include "CueAudioCallback.h"
#include "AppSettings.h"
#include "SettingsPanel.h"

//==============================================================================
/*
	This component lives inside our window, and this is where you should put all
	your controls and content.
*/
class MainComponent : public juce::AudioAppComponent, public juce::Slider::Listener, public juce::KeyListener
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
	ZoomedWaveform zoomedDisplay1{ formatManager, thumbCache, juce::Colours::aqua };

	/// Instance of ZoomedWaveform class for the right DJ Deck's audio track.
	ZoomedWaveform zoomedDisplay2{ formatManager, thumbCache,juce::Colours::hotpink };

	/// Cross-deck beat-sync manager (master/slave coordinator).
	BeatSyncManager beatSyncManager;

	/// Instance of DeckGUI class for the left DJ Deck.
	DeckGUI deckGUI1{ &audioEngine.getPlayer(0), formatManager, thumbCache,&zoomedDisplay1 , library,juce::Colours::aqua, &beatSyncManager, 0, &audioEngine };

	/// Instance of DeckGUI class for the right DJ Deck.
	DeckGUI deckGUI2{ &audioEngine.getPlayer(1), formatManager, thumbCache,&zoomedDisplay2 , library,juce::Colours::hotpink, &beatSyncManager, 1, &audioEngine };

	/// Horizontal fader subclass that passes right-click to the parent so the
	/// context-menu handler in MainComponent::mouseDown can show a reset popup.
	struct ResetableFader : public juce::Slider {
		ResetableFader() : juce::Slider(juce::Slider::SliderStyle::LinearHorizontal, juce::Slider::TextEntryBoxPosition::NoTextBox) {}
		void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDown(e); }
		void mouseDrag(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDrag(e); }
	};

	/// Instance of juce::Slider for cross fading functionality.
	ResetableFader crossFader;

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

	/// Gear button that opens / closes the settings panel (placed above crossfader).
	juce::TextButton settingsButton{ "\u2699" };

	/// Animate the settings panel into view from the top.
	void openSettings();

	/// Animate the settings panel out of view.
	void closeSettings();

	/// @return Bounds used when settings panel is fully open.
	juce::Rectangle<int> settingsPanelOpenBounds()  const;

	/// @return Bounds used when settings panel is fully closed (off-screen top).
	juce::Rectangle<int> settingsPanelClosedBounds() const;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
