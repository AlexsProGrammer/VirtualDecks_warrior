#pragma once
#include <JuceHeader.h>

#include "../core/audio/AudioEngine.h"

/**
 * AudioIODeviceCallback that routes the cued deck's processed audio to a
 * separate output device (e.g. headphones). The callback reads from the
 * cued player's lock-free ring buffer; it fills silence if the buffer is
 * starved or no deck is currently cued.
 *
 * Register this with a dedicated AudioDeviceManager (not the main one):
 * @code
 *   cueDeviceManager.addAudioCallback(&cueCallback);
 * @endcode
 */
class CueAudioCallback : public juce::AudioIODeviceCallback
{
public:
	CueAudioCallback() = default;

	/** Must be set before the device manager starts calling the callback. */
	void setEngine(AudioEngine* engine) noexcept { audioEngine = engine; }

	//==============================================================================
	// AudioIODeviceCallback

	void audioDeviceIOCallbackWithContext(
		const float* const*                          inputChannelData,
		int                                          numInputChannels,
		float* const*                                outputChannelData,
		int                                          numOutputChannels,
		int                                          numSamples,
		const juce::AudioIODeviceCallbackContext&    context) override;

	void audioDeviceAboutToStart(juce::AudioIODevice* /*device*/) override {}
	void audioDeviceStopped() override {}

private:
	AudioEngine* audioEngine = nullptr;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CueAudioCallback)
};
