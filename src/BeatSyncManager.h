#pragma once

#include <JuceHeader.h>
#include "DJAudioPlayer.h"

class DeckGUI;

//==============================================================================

/**
 * Definition of BeatSyncManager
 *
 * Coordinates beat-sync between the two DJ decks. Holds the master/slave
 * state, computes BPM-matching speed ratios (with half/double aware
 * resolution), aligns phase on engage, and continuously tracks master
 * speed changes via a juce::Timer that pushes updated speed to engaged
 * slaves.
 *
 * Owned by MainComponent. Each DeckGUI receives a pointer plus its
 * deck index (0 = Deck 1, 1 = Deck 2).
 *
 * All state mutates only on the message thread (timer callback runs there).
 * Audio-thread interactions go strictly through DJAudioPlayer's existing
 * thread-safe setters, so no additional locking is required.
 */
class BeatSyncManager : public juce::Timer {
public:
	//==============================================================================

	/// Identifies which deck is currently the sync master, or None.
	enum class MasterDeck { None, Deck1, Deck2 };

	//==============================================================================

	/// Constructor. Decks must be wired in via setDeck() before use.
	BeatSyncManager();

	/// Destructor stops the timer.
	~BeatSyncManager() override;

	//==============================================================================

	/**
	 * Wire a deck into the manager. Must be called for both decks before any
	 * sync operations are attempted.
	 *
	 * @param idx       0 for Deck 1, 1 for Deck 2
	 * @param player    DJAudioPlayer pointer for this deck
	 * @param deckGUI   DeckGUI pointer for this deck (for refresh callbacks)
	 */
	void setDeck(int idx, DJAudioPlayer* player, DeckGUI* deckGUI);

	//==============================================================================

	/// Returns the currently configured master deck (or None).
	MasterDeck getMaster() const;

	/// Returns true if the given deck index is the master.
	bool isMaster(int deckIdx) const;

	/**
	 * Set the master deck. Pass MasterDeck::None to clear. Setting a master
	 * disengages any previously active sync from the new master deck (a deck
	 * cannot be both master and synced slave). Slaves remain engaged but
	 * re-target their speed to the new master.
	 */
	void setMaster(MasterDeck which);

	//==============================================================================

	/// Returns true if the given deck is currently engaged as a synced slave.
	bool isSynced(int deckIdx) const;

	/**
	 * Engage sync on the given deck (treating it as slave). Returns false if:
	 *   - no master is set, or
	 *   - this deck is the master, or
	 *   - either deck is missing BPM / not loaded, or
	 *   - resulting speed ratio would be out of slider range [0.8, 1.2]
	 *     even after auto-resolving half/double multiplier.
	 * On success, slave's playhead is phase-aligned with the master's beat phase
	 * and slave's playback speed is set so its effective BPM matches the master.
	 */
	bool engageSync(int slaveDeckIdx);

	/// Disengage sync on the given deck. Slave's speed is left at its current value.
	void disengageSync(int slaveDeckIdx);

	//==============================================================================

	/// Get the current half/double multiplier for a slave deck (1.0 default).
	double getSlaveMultiplier(int deckIdx) const;

	/**
	 * Set the half/double multiplier (0.5, 1.0, or 2.0) for a deck. If the deck
	 * is currently synced, the speed ratio is re-resolved and applied immediately.
	 * If the resulting ratio is out of range, sync remains engaged but status
	 * is updated to "OUT OF RANGE".
	 */
	void setSlaveMultiplier(int deckIdx, double mult);

	//==============================================================================

	/// Returns the master BPM the slave is targeted at, or 0.0 if not synced.
	double getTargetBpm(int deckIdx) const;

	/// Returns a short human-readable status string for UI display ("", "NO BPM", "OUT OF RANGE", "SYNCED").
	juce::String getStatus(int deckIdx) const;

	//==============================================================================

	/// Notify the manager that a deck just loaded a new track. Auto-disengages
	/// any active sync involving this deck (slave or master).
	void onTrackLoaded(int deckIdx);

	/// Notify the manager that a deck's detected BPM changed (e.g. analysis
	/// completed). If a sync was previously out-of-range, this attempts to retry.
	void onBpmUpdated(int deckIdx);

private:
	//==============================================================================

	/// Timer callback runs on the message thread; tracks master speed/bpm
	/// changes and pushes new speed to engaged slaves.
	void timerCallback() override;

	/// Apply the currently-resolved speed ratio to the given slave.
	void applySlaveSpeed(int slaveDeckIdx);

	/// Snap the given slave's playhead to phase-align with the master.
	void phaseAlignSlave(int slaveDeckIdx);

	/// Compute the speed ratio required for the slave to match the master,
	/// using the slave's current multiplier. Returns 0.0 on invalid state.
	double computeSpeedRatio(int slaveDeckIdx) const;

	/// Pick a multiplier (0.5, 1.0, 2.0) so the resulting ratio lands closest
	/// to 1.0 within the [0.8, 1.2] slider range. Returns 1.0 if none fit.
	static double autoResolveMultiplier(double masterBpm, double slaveBpm);

	/// Refresh both deck GUIs (no-op if pointers null).
	void refreshDeckUIs();

	/// Returns 0 or 1 for the master, or -1 if no master.
	int masterIndex() const;

	/// Returns the other deck's index (0->1, 1->0). Undefined for invalid input.
	static int otherIndex(int idx);

	//==============================================================================

	/// Speed slider range constants matching DeckGUI's speedSlider.
	static constexpr double kMinSpeed = 0.8;
	static constexpr double kMaxSpeed = 1.2;

	/// Polling interval for master speed changes (ms).
	static constexpr int    kTimerIntervalMs = 50;

	/// Speed-change epsilon to avoid redundant setSpeed calls.
	static constexpr double kSpeedEpsilon = 1e-4;

	//==============================================================================

	DJAudioPlayer*  players[2]      = { nullptr, nullptr };
	DeckGUI*        deckGUIs[2]     = { nullptr, nullptr };
	MasterDeck      currentMaster   = MasterDeck::None;
	bool            syncEngaged[2]  = { false, false };
	double          slaveMultiplier[2] = { 1.0, 1.0 };
	double          lastAppliedSpeed[2] = { 0.0, 0.0 };
	juce::String    statusText[2];

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatSyncManager)
};
