#include <JuceHeader.h>
#include "CueAudioCallback.h"

void CueAudioCallback::setTestToneActive(bool active) noexcept
{
	testToneActive.store(active, std::memory_order_release);
}

void CueAudioCallback::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
	if (device != nullptr)
		testToneSampleRate = static_cast<float>(device->getCurrentSampleRate());
	testTonePhase = 0.0f;
}

void CueAudioCallback::audioDeviceIOCallbackWithContext(
	const float* const* /*inputChannelData*/,
	int                 /*numInputChannels*/,
	float* const*       outputChannelData,
	int                 numOutputChannels,
	int                 numSamples,
	const juce::AudioIODeviceCallbackContext& /*context*/)
{
	// Test tone: plays a 1 kHz sine wave at the current headphone gain so the
	// user can verify and fine-tune the volume slider. Takes full priority over
	// any ring-buffer audio.
	if (testToneActive.load(std::memory_order_acquire))
	{
		const float twoPi = juce::MathConstants<float>::twoPi;
		const float inc   = twoPi * 1000.0f / testToneSampleRate;
		const float gain  = audioEngine ? audioEngine->getHeadphoneOutputGain() : 1.0f;

		for (int i = 0; i < numSamples; ++i)
		{
			const float sample = 0.2f * std::sin(testTonePhase) * gain;
			testTonePhase += inc;
			for (int ch = 0; ch < numOutputChannels; ++ch)
				if (outputChannelData[ch] != nullptr)
					outputChannelData[ch][i] = sample;
		}
		// Keep phase in [0, 2π) to prevent floating-point drift.
		while (testTonePhase >= twoPi) testTonePhase -= twoPi;
		return;
	}

	DJAudioPlayer* const cuedPlayer = audioEngine ? audioEngine->getCuedPlayer() : nullptr;

	if (cuedPlayer == nullptr)
	{
		// No deck is currently cued - output silence.
		for (int ch = 0; ch < numOutputChannels; ++ch)
		{
			if (outputChannelData[ch] != nullptr)
				juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
		}
		return;
	}

	const int read = cuedPlayer->readCueTap(outputChannelData, numOutputChannels, numSamples);

	// Fill any remaining frames (buffer underrun) with silence.
	if (read < numSamples)
	{
		for (int ch = 0; ch < numOutputChannels; ++ch)
		{
			if (outputChannelData[ch] != nullptr)
				juce::FloatVectorOperations::clear(outputChannelData[ch] + read, numSamples - read);
		}
	}

	// Apply headphone output gain.
	const float gain = audioEngine->getHeadphoneOutputGain();
	if (std::abs(gain - 1.0f) > 0.001f) // Skip if gain ≈ 1.0 (common case)
	{
		for (int ch = 0; ch < numOutputChannels; ++ch)
		{
			if (outputChannelData[ch] != nullptr)
				juce::FloatVectorOperations::multiply(outputChannelData[ch], gain, numSamples);
		}
	}
}
