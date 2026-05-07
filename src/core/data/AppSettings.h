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
} // namespace AppSettings
