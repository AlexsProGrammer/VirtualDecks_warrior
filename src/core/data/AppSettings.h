#pragma once
#include <JuceHeader.h>

/**
 * Thin namespace for persisting application settings (other than the main
 * JUCE AudioDeviceManager state) to ~/.otodecks/AppSettings.xml.
 */
namespace AppSettings
{
	/// Returns the on-disk path for the settings XML file.
	juce::File getSettingsFile();

	/**
	 * Load and return the serialised AudioDeviceManager state for the
	 * headphone (cue) output device. Returns nullptr if the file does not
	 * exist or contains no headphone state.
	 */
	std::unique_ptr<juce::XmlElement> loadHeadphoneDeviceState();

	/**
	 * Persist the headphone device manager state. Pass nullptr to remove
	 * any previously stored state.
	 */
	void saveHeadphoneDeviceState(const juce::XmlElement* state);

	/**
	 * Load and return the serialised AudioDeviceManager state for the
	 * master output device. Returns nullptr if the file does not exist or
	 * contains no master device state.
	 */
	std::unique_ptr<juce::XmlElement> loadMasterDeviceState();

	/**
	 * Persist the master device manager state. Pass nullptr to remove any
	 * previously stored state.
	 */
	void saveMasterDeviceState(const juce::XmlElement* state);

	/**
	 * Load whether playback should start at the first saved hot cue.
	 * Returns false if not found.
	 */
	bool loadStartAtFirstHotCue();

	/**
	 * Save whether playback should start at the first saved hot cue.
	 */
	void saveStartAtFirstHotCue(bool value);

	/**
	 * Load the master output gain from persisted settings.
	 * Returns 1.0f if not found or invalid.
	 */
	float loadMasterGain();

	/**
	 * Save the master output gain to persisted settings.
	 */
	void saveMasterGain(float gain);

	/**
	 * Load the headphone output gain from persisted settings.
	 * Returns 1.0f if not found or invalid.
	 */
	float loadHeadphoneGain();

	/**
	 * Load the previously selected MIDI input device identifier.
	 */
	juce::String loadMidiDeviceId();

	/**
	 * Save the selected MIDI input device identifier.
	 */
	void saveMidiDeviceId(const juce::String& deviceId);

	/**
	 * Save the headphone output gain to persisted settings.
	 */
	void saveHeadphoneGain(float gain);
} // namespace AppSettings
