
#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "FxIds.h"
#include "FxProcessor.h"
#include "FxFactory.h"

/**
 * juce::AudioSource that wraps an upstream source and runs three FX slots
 * (Pad -> Beat -> Release) on every block.
 *
 * Threading model:
 *  - All processors are pre-allocated on the message thread before audio
 *    starts. The audio thread NEVER allocates.
 *  - Slot selection (which processor is active per category) is held in an
 *    atomic<int> and switched lock-free. Selection becomes visible to the
 *    audio thread on the next block.
 *  - Per-parameter atomics (FxParameter::value) make UI -> audio parameter
 *    updates lock-free.
 */
class FxChain : public juce::AudioSource
{
public:
	/// Number of FX categories ( = number of slots, in processing order).
	static constexpr int kNumSlots = (int) FxCategory::Count;

	explicit FxChain(juce::AudioSource* upstream) noexcept : input(upstream) {}

	//==============================================================================
	// juce::AudioSource

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
	{
		// NOTE: upstream is prepared explicitly by the owning DJAudioPlayer
		// (each link in the IIR chain is prepared individually). Re-preparing
		// here would walk that chain a second time. We only prepare our
		// own per-slot processors.
		for (int s = 0; s < kNumSlots; ++s)
			for (auto& fx : slots[s])
				fx->prepareToPlay(samplesPerBlockExpected, sampleRate);
	}

	void releaseResources() override {}

	void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
	{
		if (input != nullptr) input->getNextAudioBlock(info);
		else                  info.clearActiveBufferRegion();

		// Run slots in fixed order: Pad -> Beat -> Release.
		// Use a thin AudioBuffer view for the active region.
		juce::AudioBuffer<float> view (info.buffer->getArrayOfWritePointers(),
		                                info.buffer->getNumChannels(),
		                                info.startSample,
		                                info.numSamples);

		for (int s = 0; s < kNumSlots; ++s)
		{
			const int idx = activeIndex[s].load(std::memory_order_acquire);
			if (idx <= 0 || idx >= (int) slots[s].size()) continue; // 0 = None
			auto& fx = *slots[s][(size_t) idx];
			if (! fx.isEngaged() || fx.isBypassed()) continue;
			fx.process(view);
		}
	}

	//==============================================================================
	// Setup (message-thread only, before audio starts)

	/// Build all processors for every category. Must be called once before
	/// prepareToPlay.
	void buildSlots()
	{
		for (int s = 0; s < kNumSlots; ++s)
		{
			slots[s] = FxFactory::buildCategory((FxCategory) s);
			activeIndex[s].store(0, std::memory_order_release);
		}
	}

	//==============================================================================
	// Selection / state (audio-thread safe - atomic store on the active index)

	/// Activate a processor by index within the slot's list. 0 = None.
	void setActiveIndex(FxCategory cat, int index) noexcept
	{
		const int s = (int) cat;
		if (s < 0 || s >= kNumSlots) return;
		const int clamped = juce::jlimit(0, (int) slots[s].size() - 1, index);
		// Disengage previous selection so it does not keep producing audio.
		const int prev = activeIndex[s].exchange(clamped, std::memory_order_acq_rel);
		if (prev != clamped && prev > 0 && prev < (int) slots[s].size())
			slots[s][(size_t) prev]->setEngaged(false);
	}

	int getActiveIndex(FxCategory cat) const noexcept
	{
		const int s = (int) cat;
		if (s < 0 || s >= kNumSlots) return 0;
		return activeIndex[s].load(std::memory_order_acquire);
	}

	FxProcessor* getActiveProcessor(FxCategory cat) noexcept
	{
		const int s = (int) cat;
		if (s < 0 || s >= kNumSlots) return nullptr;
		const int idx = activeIndex[s].load(std::memory_order_acquire);
		if (idx < 0 || idx >= (int) slots[s].size()) return nullptr;
		return slots[s][(size_t) idx].get();
	}

	const std::vector<std::unique_ptr<FxProcessor>>& getProcessors(FxCategory cat) const noexcept
	{
		return slots[(int) cat];
	}

	/// Find the position of an FxId within a category's processor list.
	/// Returns 0 (the None entry) if not found.
	int indexOf(FxCategory cat, FxId id) const noexcept
	{
		const auto& list = slots[(int) cat];
		for (int i = 0; i < (int) list.size(); ++i)
			if (list[(size_t) i]->getId() == id) return i;
		return 0;
	}

	/// Push BPM update to every processor (so beat-synced effects retrack).
	void setBpm(double bpmValue) noexcept
	{
		for (int s = 0; s < kNumSlots; ++s)
			for (auto& fx : slots[s])
				fx->setBpm(bpmValue);
	}

private:
	juce::AudioSource* input = nullptr;
	std::array<std::vector<std::unique_ptr<FxProcessor>>, kNumSlots> slots;
	std::array<std::atomic<int>, kNumSlots> activeIndex { { {0}, {0}, {0} } };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxChain)
};
