#pragma once
#include <JuceHeader.h>

/**
 * Slide-in settings panel that hosts device selectors for the master output
 * and the headphone (cue) output. Constructed with references to both
 * AudioDeviceManagers so JUCE's built-in AudioDeviceSelectorComponent can
 * drive them directly.
 *
 * Call openSettings() / closeSettings() on the parent (MainComponent) to
 * animate this panel into / out of view.
 */
class SettingsPanel : public juce::Component
{
public:
	/**
	 * @param masterManager   The main AudioDeviceManager (from AudioAppComponent).
	 * @param headphoneManager The cue/headphone AudioDeviceManager.
	 * @param onClose          Called when the user presses the × button.
	 */
	SettingsPanel(juce::AudioDeviceManager& masterManager,
	              juce::AudioDeviceManager& headphoneManager,
	              std::function<void()>     onClose);

	~SettingsPanel() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

private:
	std::function<void()> closeCallback;

	/// Reference held so we can persist device state when the panel closes.
	juce::AudioDeviceManager& headphoneManager;

	juce::Label masterLabel;
	juce::Label headphoneLabel;

	juce::AudioDeviceSelectorComponent masterSelector;
	juce::AudioDeviceSelectorComponent headphoneSelector;

	juce::TextButton closeButton{ "\u00d7" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
