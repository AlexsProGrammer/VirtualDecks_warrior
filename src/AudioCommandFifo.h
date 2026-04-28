#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

/**
 * A POD command sent from the UI message thread to the audio thread via
 * AudioCommandFifo. Drained inside DJAudioPlayer::getNextAudioBlock before any
 * audio processing occurs. All payload fields are unioned by tag semantics:
 * a tag implies which payload member is meaningful.
 *
 * NOTE: AudioCommand is intentionally trivially copyable so it can live in a
 * lock-free ring buffer without per-slot construction.
 */
struct AudioCommand
{
	enum class Tag : juce::uint8
	{
		None = 0,

		// Transport
		Start,
		Stop,

		// Speed / gain
		SetSpeed,             // doublePayload = ratio
		SetGainPlayer,        // doublePayload = 0..1
		SetGainCrossfade,     // doublePayload = 0..1

		// Position
		SetPosition,          // doublePayload = posInSecs
		SetPositionRelative,  // doublePayload = 0..1

		// Loop
		SetLoopIn,
		SetLoopOut,
		ToggleReloop,
		HalveLoop,
		DoubleLoop,
		ClearLoop,

		// Filters
		SetFilter,            // doublePayload = freq
		SetLBFilter,          // doublePayload = gain
		SetMBFilter,          // doublePayload = gain
		SetHBFilter,          // doublePayload = gain

		// Beat ops
		BeatJump              // intPayload    = beats
	};

	Tag    tag           = Tag::None;
	double doublePayload = 0.0;
	int    intPayload    = 0;
};

//==============================================================================

/**
 * Single-producer / single-consumer lock-free ring buffer of AudioCommand.
 *
 * Producer: UI message thread (slider listeners, buttons, sync timer).
 * Consumer: audio callback (DJAudioPlayer::getNextAudioBlock).
 *
 * Capacity is fixed at construction. Overflowing commands are dropped (the
 * audio thread always sees the latest user intent on its next callback;
 * coalescing slider spam is acceptable because applying a few stale values is
 * cheap and the freshest value is always at the head).
 */
template <int Capacity = 256>
class AudioCommandFifo
{
public:
	AudioCommandFifo() noexcept : fifo(Capacity) {}

	/**
	 * Push a command from the producer (UI) thread. Returns false if the
	 * FIFO was full and the command was dropped.
	 */
	bool push(const AudioCommand& cmd) noexcept
	{
		int start1, size1, start2, size2;
		fifo.prepareToWrite(1, start1, size1, start2, size2);
		if (size1 == 0 && size2 == 0)
			return false;
		if (size1 > 0)
			storage[(size_t) start1] = cmd;
		else
			storage[(size_t) start2] = cmd;
		fifo.finishedWrite(1);
		return true;
	}

	/**
	 * Drain all pending commands on the consumer (audio) thread, invoking
	 * the visitor for each. The visitor must not allocate or block.
	 */
	template <typename Visitor>
	void drain(Visitor&& visit) noexcept
	{
		int start1, size1, start2, size2;
		fifo.prepareToRead(fifo.getNumReady(), start1, size1, start2, size2);
		for (int i = 0; i < size1; ++i)
			visit(storage[(size_t) (start1 + i)]);
		for (int i = 0; i < size2; ++i)
			visit(storage[(size_t) (start2 + i)]);
		fifo.finishedRead(size1 + size2);
	}

private:
	juce::AbstractFifo               fifo;
	std::array<AudioCommand, Capacity> storage {};

	JUCE_DECLARE_NON_COPYABLE(AudioCommandFifo)
};
