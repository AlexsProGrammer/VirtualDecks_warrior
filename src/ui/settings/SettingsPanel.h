#pragma once
#include <JuceHeader.h>

/**
 * Slide-in settings panel that hosts device selectors for the master output
 * and the headphone (cue) output, plus per-device volume sliders.
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
	 * @param masterGainSetter Callback to update master output gain (0-1).
	 * @param headphoneGainSetter Callback to update headphone output gain (0-1).
	 * @param initialMasterGain Initial master gain value (0-1).
	 * @param initialHeadphoneGain Initial headphone gain value (0-1).
	 */
	SettingsPanel(juce::AudioDeviceManager& masterManager,
	              juce::AudioDeviceManager& headphoneManager,
	              std::function<void()>     onClose,
	              std::function<void(float)> masterGainSetter,
	              std::function<void(float)> headphoneGainSetter,
	              float                      initialMasterGain = 1.0f,
	              float                      initialHeadphoneGain = 1.0f);

	~SettingsPanel() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

private:
	std::function<void()> closeCallback;
	std::function<void(float)> masterGainCallback;
	std::function<void(float)> headphoneGainCallback;

	/// Reference held so we can persist device state when the panel closes.
	juce::AudioDeviceManager& headphoneManager;

	juce::Label masterLabel;
	juce::Label headphoneLabel;
	juce::Label masterVolLabel{ "masterVolLabel", "Master Volume" };
	juce::Label headphoneVolLabel{ "headphoneVolLabel", "Headphone Volume" };

	juce::AudioDeviceSelectorComponent masterSelector;
	juce::AudioDeviceSelectorComponent headphoneSelector;

	juce::Slider masterVolSlider;
	juce::Slider headphoneVolSlider;

	juce::TextButton closeButton{ "X" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
