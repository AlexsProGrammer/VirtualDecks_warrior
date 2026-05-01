#include <JuceHeader.h>
#include "CueAudioCallback.h"

void CueAudioCallback::audioDeviceIOCallbackWithContext(
	const float* const* /*inputChannelData*/,
	int                 /*numInputChannels*/,
	float* const*       outputChannelData,
	int                 numOutputChannels,
	int                 numSamples,
	const juce::AudioIODeviceCallbackContext& /*context*/)
{
	DJAudioPlayer* const cuedPlayer = audioEngine ? audioEngine->getCuedPlayer() : nullptr;

	if (cuedPlayer == nullptr)
	{
		// No deck is currently cued - output silence.
		for (int ch = 0; ch < numOutputChannels; ++ch)
			juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
		return;
	}

	const int read = cuedPlayer->readCueTap(outputChannelData, numOutputChannels, numSamples);

	// Fill any remaining frames (buffer underrun) with silence.
	if (read < numSamples)
	{
		for (int ch = 0; ch < numOutputChannels; ++ch)
			juce::FloatVectorOperations::clear(outputChannelData[ch] + read, numSamples - read);
	}
}
