
#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include "FxIds.h"
#include "FxParameter.h"

/**
 * Abstract base class for any effect that can sit in a FxChain slot.
 *
 * Lifetime: instances are owned by FxChain. They are constructed on the
 * message thread before the audio device is opened, prepared once via
 * prepareToPlay(), and from then on only audio-thread-safe methods may be
 * invoked: process(), setEngaged(), setBpm(), and FxParameter::set() through
 * the parameter list.
 *
 * Engagement model:
 *  - "engaged" = effect is actively producing wet audio. Categories use this
 *    differently:
 *      Pad     : engaged only while the user holds a tile (momentary)
 *      Beat    : engaged while latched ON
 *      Release : engaged only while the user holds a tile (takeover)
 *  - "bypassed" = parameter-modal toggle that hard-skips processing even when
 *    engaged. Used by the "bypass" checkbox in the modal.
 *
 * When neither engaged nor bypassed, process() is skipped by FxChain entirely
 * (block passes through untouched).
 */
class FxProcessor
{
public:
	virtual ~FxProcessor() = default;

	/// Stable identifier for factory + persistence lookup.
	virtual FxId getId() const noexcept = 0;

	/// Display name shown on tiles and in the parameter modal title.
	virtual juce::String getName() const = 0;

	/// One-time setup. Called on the message thread before audio starts.
	virtual void prepareToPlay(int samplesPerBlockExpected, double sampleRate)
	{
		blockSize = samplesPerBlockExpected;
		this->sampleRate = sampleRate;
	}

	/// Process audio in-place. Called on the audio thread, every block,
	/// only when isEngaged() && !isBypassed(). Implementations must not
	/// allocate, lock, or block.
	virtual void process(juce::AudioBuffer<float>& buffer) = 0;

	/// Optional: called when the effect transitions from disengaged to engaged
	/// (or vice-versa). Lets DSP reset history buffers or arm capture.
	virtual void onEngageChanged(bool /*nowEngaged*/) {}

	/// Audio-thread setter. Stores in atomic + invokes onEngageChanged if changed.
	void setEngaged(bool e) noexcept
	{
		const bool prev = engaged.exchange(e, std::memory_order_acq_rel);
		if (prev != e) onEngageChanged(e);
	}

	bool isEngaged()  const noexcept { return engaged.load(std::memory_order_acquire); }

	void setBypassed(bool b) noexcept { bypassed.store(b, std::memory_order_release); }
	bool isBypassed() const noexcept { return bypassed.load(std::memory_order_acquire); }

	/// Push current playback BPM (already adjusted by speed ratio).
	/// Beat-synced effects use this to recompute internal time constants.
	virtual void setBpm(double bpmValue) noexcept { currentBpm.store(bpmValue, std::memory_order_release); }
	double getBpm() const noexcept { return currentBpm.load(std::memory_order_acquire); }

	/// Read-only parameter list. UI builds the modal from this; audio-thread
	/// process() reads parameter values via params[i].get().
	std::vector<FxParameter>&       getParameters()       noexcept { return params; }
	const std::vector<FxParameter>& getParameters() const noexcept { return params; }

	/// Reset every parameter to its default. Audio-thread safe.
	void resetParameters() noexcept
	{
		for (auto& p : params) p.resetToDefault();
		onParametersReset();
	}

protected:
	/// Override to clear delay lines / capture buffers when defaults are restored.
	virtual void onParametersReset() {}

	int    blockSize  = 0;
	double sampleRate = 44100.0;
	std::vector<FxParameter> params;

private:
	std::atomic<bool>   engaged  { false };
	std::atomic<bool>   bypassed { false };
	std::atomic<double> currentBpm { 0.0 };
};
