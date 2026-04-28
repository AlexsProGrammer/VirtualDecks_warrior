#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include "DJAudioPlayer.h"

/**
 * AudioEngine — facade that owns the two DJAudioPlayers and the MixerAudioSource,
 * coordinates audio rendering on the audio thread, and dispatches off-thread
 * track loading on a dedicated ThreadPool.
 *
 * Responsibilities:
 *   - Owns and prepares both DJAudioPlayers + the mixer.
 *   - Provides getNextAudioBlock for MainComponent (audio thread only).
 *   - Provides AudioEngine::requestLoad(deck, url) which performs file I/O and
 *     AudioFormatReader creation on a background thread and hands the finished
 *     reader source back to the player on the message thread.
 *   - Broadcasts loading-state transitions to UI listeners.
 *
 * Threading rules:
 *   - All public mutating methods are called on the message thread.
 *   - getNextAudioBlock / prepareToPlay / releaseResources run on the audio thread.
 *   - Load jobs run on loadPool worker threads; results are marshalled back via
 *     juce::MessageManager::callAsync.
 */
class AudioEngine : public juce::AudioSource
{
public:
	//==============================================================================

	/**
	 * Listener interface for asynchronous load completion / state change.
	 * All callbacks fire on the message thread.
	 */
	class Listener
	{
	public:
		virtual ~Listener() = default;

		/**
		 * Called whenever the loading state of a deck changes
		 * (Idle → Loading → Loaded / Failed).
		 *
		 * @param deckIndex 0 or 1
		 * @param newState  the new loading state
		 */
		virtual void deckLoadingStateChanged(int deckIndex, DJAudioPlayer::LoadingState newState) = 0;
	};

	//==============================================================================

	/**
	 * Constructs the engine. The shared AudioFormatManager passed in is used by
	 * UI-side consumers (thumbnails, library); the engine maintains its own
	 * separate AudioFormatManager for the load pool to avoid cross-thread
	 * contention on the shared one.
	 */
	explicit AudioEngine(juce::AudioFormatManager& sharedFormatManager);

	~AudioEngine() override;

	//==============================================================================
	// AudioSource

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
	void releaseResources() override;

	//==============================================================================
	// Player access (read-only handles for UI)

	/// Returns a reference to the requested deck's player.
	DJAudioPlayer&       getPlayer(int deckIndex)       noexcept;
	const DJAudioPlayer& getPlayer(int deckIndex) const noexcept;

	//==============================================================================
	// Asynchronous track loading

	/**
	 * Request asynchronous load of a track into the given deck. The function
	 * returns immediately; the deck's loading state transitions to Loading,
	 * then to Loaded (or Failed) once the background job completes.
	 *
	 * @param deckIndex 0 or 1
	 * @param audioURL  source URL
	 * @param onComplete optional message-thread callback; bool == success
	 */
	void requestLoad(int deckIndex,
	                 juce::URL audioURL,
	                 std::function<void(bool)> onComplete = {});

	//==============================================================================
	// Listener registration

	void addListener   (Listener* l);
	void removeListener(Listener* l);

	//==============================================================================
	// Headphone cue routing

	/**
	 * Route the processed output of deck deckIndex to the cue ring buffer.
	 * Pass -1 to disable cue monitoring for all decks.
	 * If the same deckIndex is already active this is a no-op (caller handles
	 * the toggle logic).
	 */
	void setCueDeck(int deckIndex) noexcept;

	/// Returns the index of the deck currently sending to the cue output,
	/// or -1 if none.
	int getCuedDeckIndex() const noexcept;

	/// Returns a pointer to the cue-active player, or nullptr if none.
	DJAudioPlayer* getCuedPlayer() noexcept;

	//==============================================================================

	/// Background ThreadPool job that creates a reader source off the message thread.
	class LoadJob;

	/// Called on the message thread by a LoadJob once the reader is ready.
	void completeLoad(int deckIndex,
	                  std::unique_ptr<juce::AudioFormatReaderSource> source,
	                  double sampleRate,
	                  juce::URL audioURL,
	                  std::function<void(bool)> onComplete);

	/// Called on the message thread by a LoadJob when the reader could not be created.
	void completeLoadFailed(int deckIndex,
	                        juce::URL audioURL,
	                        std::function<void(bool)> onComplete);

	/// Notifies listeners on the message thread of a state change.
	void notifyLoadingStateChanged(int deckIndex, DJAudioPlayer::LoadingState newState);

	//==============================================================================

	/// Shared format manager (UI side: thumbnails, library probing).
	juce::AudioFormatManager& sharedFormatManager;

	/// Dedicated format manager used only by the load pool (avoids concurrent
	/// access to the shared instance, which is not thread-safe for createReaderFor).
	juce::AudioFormatManager loadFormatManager;

	/// Two players (deck 0 and deck 1).
	DJAudioPlayer player1;
	DJAudioPlayer player2;

	/// Mixes both players.
	juce::MixerAudioSource mixerSource;

	/// Load pool — single worker so reader creation is serialised through one
	/// AudioFormatManager. Two decks loading in rapid succession will queue.
	juce::ThreadPool loadPool { 1 };

	/// Listeners notified on the message thread.
	juce::ListenerList<Listener> listeners;

	/// Generation counter per deck to invalidate in-flight loads if a newer
	/// requestLoad is issued before the previous one completes.
	std::atomic<int> loadGeneration[2] { {0}, {0} };

	/// Index of the deck currently routed to the cue (headphone) output.
	/// -1 means no deck is cued. Written on message thread, read on headphone thread.
	std::atomic<int> cueDeckIndex { -1 };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
