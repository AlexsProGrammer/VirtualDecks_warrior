#include "BeatSyncManager.h"
#include "../../ui/decks/DeckGUI.h"

//==============================================================================

BeatSyncManager::BeatSyncManager() {
	startTimer(kTimerIntervalMs);
}

BeatSyncManager::~BeatSyncManager() {
	stopTimer();
}

//==============================================================================

void BeatSyncManager::setDeck(int idx, DJAudioPlayer* player, DeckGUI* deckGUI) {
	if (idx < 0 || idx > 1) return;
	players[idx]  = player;
	deckGUIs[idx] = deckGUI;
}

//==============================================================================

BeatSyncManager::MasterDeck BeatSyncManager::getMaster() const {
	return currentMaster;
}

bool BeatSyncManager::isMaster(int deckIdx) const {
	if (deckIdx == 0) return currentMaster == MasterDeck::Deck1;
	if (deckIdx == 1) return currentMaster == MasterDeck::Deck2;
	return false;
}

void BeatSyncManager::setMaster(MasterDeck which) {
	if (currentMaster == which) return;

	// If the new master was previously a synced slave, disengage that first.
	int newMasterIdx = -1;
	if (which == MasterDeck::Deck1) newMasterIdx = 0;
	else if (which == MasterDeck::Deck2) newMasterIdx = 1;

	if (newMasterIdx >= 0 && syncEngaged[newMasterIdx])
		disengageSync(newMasterIdx);

	currentMaster = which;

	// Re-target any currently engaged slaves to the new master.
	for (int i = 0; i < 2; ++i) {
		if (syncEngaged[i] && i != newMasterIdx)
			applySlaveSpeed(i);
	}

	refreshDeckUIs();
}

//==============================================================================

bool BeatSyncManager::isSynced(int deckIdx) const {
	if (deckIdx < 0 || deckIdx > 1) return false;
	return syncEngaged[deckIdx];
}

bool BeatSyncManager::engageSync(int slaveDeckIdx) {
	if (slaveDeckIdx < 0 || slaveDeckIdx > 1) return false;
	if (currentMaster == MasterDeck::None)    { statusText[slaveDeckIdx] = "NO MASTER"; refreshDeckUIs(); return false; }
	if (isMaster(slaveDeckIdx))               { statusText[slaveDeckIdx] = "IS MASTER"; refreshDeckUIs(); return false; }

	int mIdx = masterIndex();
	if (mIdx < 0 || players[mIdx] == nullptr || players[slaveDeckIdx] == nullptr) {
		statusText[slaveDeckIdx] = "ERROR";
		refreshDeckUIs();
		return false;
	}

	const auto& mGrid = players[mIdx]->getBeatGrid();
	const auto& sGrid = players[slaveDeckIdx]->getBeatGrid();

	if (!players[mIdx]->isLoaded() || !players[slaveDeckIdx]->isLoaded()) {
		statusText[slaveDeckIdx] = "NOT LOADED";
		refreshDeckUIs();
		return false;
	}
	if (mGrid.bpm <= 0.0 || sGrid.bpm <= 0.0) {
		statusText[slaveDeckIdx] = "NO BPM";
		refreshDeckUIs();
		return false;
	}

	// Auto-resolve multiplier so ratio falls in the wider sync range.
	slaveMultiplier[slaveDeckIdx] = autoResolveMultiplier(mGrid.bpm, sGrid.bpm);

	double ratio = computeSpeedRatio(slaveDeckIdx);
	if (ratio < kSyncMinSpeed - 1e-9 || ratio > kSyncMaxSpeed + 1e-9) {
		DBG("BeatSyncManager::engageSync: ratio out of sync range = " << ratio);
		statusText[slaveDeckIdx] = "OUT OF RANGE";
		refreshDeckUIs();
		return false;
	}

	syncEngaged[slaveDeckIdx] = true;
	statusText[slaveDeckIdx]  = "SYNCED";

	applySlaveSpeed(slaveDeckIdx);
	phaseAlignSlave(slaveDeckIdx);
	// Re-align once on the next timer tick to compensate for any latency in
	// the resampler picking up the new ratio.
	pendingRealign[slaveDeckIdx] = true;
	refreshDeckUIs();
	return true;
}

void BeatSyncManager::disengageSync(int slaveDeckIdx) {
	if (slaveDeckIdx < 0 || slaveDeckIdx > 1) return;
	if (!syncEngaged[slaveDeckIdx]) return;

	syncEngaged[slaveDeckIdx]      = false;
	lastAppliedSpeed[slaveDeckIdx] = 0.0;
	pendingRealign[slaveDeckIdx]   = false;
	statusText[slaveDeckIdx]       = "";
	refreshDeckUIs();
}

//==============================================================================

double BeatSyncManager::getSlaveMultiplier(int deckIdx) const {
	if (deckIdx < 0 || deckIdx > 1) return 1.0;
	return slaveMultiplier[deckIdx];
}

void BeatSyncManager::setSlaveMultiplier(int deckIdx, double mult) {
	if (deckIdx < 0 || deckIdx > 1) return;
	if (mult != 0.5 && mult != 1.0 && mult != 2.0) return;

	slaveMultiplier[deckIdx] = mult;

	if (syncEngaged[deckIdx]) {
		double ratio = computeSpeedRatio(deckIdx);
		if (ratio < kSyncMinSpeed - 1e-9 || ratio > kSyncMaxSpeed + 1e-9) {
			statusText[deckIdx] = "OUT OF RANGE";
		}
		else {
			statusText[deckIdx] = "SYNCED";
			applySlaveSpeed(deckIdx);
			phaseAlignSlave(deckIdx);
		}
	}
	refreshDeckUIs();
}

//==============================================================================

double BeatSyncManager::getTargetBpm(int deckIdx) const {
	if (deckIdx < 0 || deckIdx > 1) return 0.0;
	if (!syncEngaged[deckIdx])      return 0.0;
	int mIdx = masterIndex();
	if (mIdx < 0 || players[mIdx] == nullptr) return 0.0;
	return players[mIdx]->getCurrentBpm();
}

juce::String BeatSyncManager::getStatus(int deckIdx) const {
	if (deckIdx < 0 || deckIdx > 1) return {};
	return statusText[deckIdx];
}

//==============================================================================

void BeatSyncManager::onTrackLoaded(int deckIdx) {
	if (deckIdx < 0 || deckIdx > 1) return;

	// New track on this deck: any sync involving it is invalidated.
	if (syncEngaged[deckIdx])
		disengageSync(deckIdx);

	// If this deck is the master, the other deck's sync target changed -
	// disengage to avoid stale lock at the wrong BPM.
	if (isMaster(deckIdx)) {
		int slave = otherIndex(deckIdx);
		if (syncEngaged[slave])
			disengageSync(slave);
	}

	slaveMultiplier[deckIdx]  = 1.0;
	lastAppliedSpeed[deckIdx] = 0.0;
	pendingRealign[deckIdx]   = false;
	statusText[deckIdx]       = "";
	refreshDeckUIs();
}

void BeatSyncManager::onBpmUpdated(int deckIdx) {
	if (deckIdx < 0 || deckIdx > 1) return;

	// If the slave was out-of-range, retry with new BPM.
	if (syncEngaged[deckIdx] && statusText[deckIdx] == "OUT OF RANGE") {
		int mIdx = masterIndex();
		if (mIdx >= 0 && players[mIdx] != nullptr && players[deckIdx] != nullptr) {
			double mb = players[mIdx]->getBeatGrid().bpm;
			double sb = players[deckIdx]->getBeatGrid().bpm;
			if (mb > 0.0 && sb > 0.0) {
				slaveMultiplier[deckIdx] = autoResolveMultiplier(mb, sb);
				double ratio = computeSpeedRatio(deckIdx);
				if (ratio >= kSyncMinSpeed - 1e-9 && ratio <= kSyncMaxSpeed + 1e-9) {
					statusText[deckIdx] = "SYNCED";
					applySlaveSpeed(deckIdx);
					phaseAlignSlave(deckIdx);
				}
			}
		}
	}
	refreshDeckUIs();
}

//==============================================================================

void BeatSyncManager::timerCallback() {
	int mIdx = masterIndex();
	if (mIdx < 0) return;

	// Push speed updates to engaged slaves whenever master's effective BPM
	// (i.e. master's speed ratio) changes.
	for (int i = 0; i < 2; ++i) {
		if (i == mIdx)         continue;
		if (!syncEngaged[i])   continue;
		applySlaveSpeed(i);
		if (pendingRealign[i]) {
			pendingRealign[i] = false;
			phaseAlignSlave(i);
		}
	}
}

//==============================================================================

void BeatSyncManager::applySlaveSpeed(int slaveDeckIdx) {
	if (slaveDeckIdx < 0 || slaveDeckIdx > 1) return;
	if (players[slaveDeckIdx] == nullptr)     return;

	double ratio = computeSpeedRatio(slaveDeckIdx);
	if (ratio <= 0.0) return;

	if (ratio < kSyncMinSpeed - 1e-9 || ratio > kSyncMaxSpeed + 1e-9) {
		// Stay engaged but flag - don't push out-of-range ratio to the audio engine.
		if (statusText[slaveDeckIdx] != "OUT OF RANGE") {
			statusText[slaveDeckIdx] = "OUT OF RANGE";
			refreshDeckUIs();
		}
		return;
	}

	if (std::abs(ratio - lastAppliedSpeed[slaveDeckIdx]) < kSpeedEpsilon)
		return;

	players[slaveDeckIdx]->setSpeed(ratio);
	lastAppliedSpeed[slaveDeckIdx] = ratio;

	if (statusText[slaveDeckIdx] != "SYNCED") {
		statusText[slaveDeckIdx] = "SYNCED";
		refreshDeckUIs();
	}
}

//==============================================================================

void BeatSyncManager::phaseAlignSlave(int slaveDeckIdx) {
	if (slaveDeckIdx < 0 || slaveDeckIdx > 1) return;
	int mIdx = masterIndex();
	if (mIdx < 0)                              return;
	if (players[mIdx] == nullptr)              return;
	if (players[slaveDeckIdx] == nullptr)      return;

	const auto& mGrid = players[mIdx]->getBeatGrid();
	const auto& sGrid = players[slaveDeckIdx]->getBeatGrid();
	if (mGrid.bpm <= 0.0 || sGrid.bpm <= 0.0) return;

	double mLen = players[mIdx]->getLengthInSeconds();
	double sLen = players[slaveDeckIdx]->getLengthInSeconds();
	if (mLen <= 0.0 || sLen <= 0.0) return;

	double mPosSecs = players[mIdx]->getPositionRelative() * mLen;
	double sPosSecs = players[slaveDeckIdx]->getPositionRelative() * sLen;

	double N = (double)juce::jmax(1, snapBeats);

	double mBeats = (mPosSecs - mGrid.gridOffsetSecs) * mGrid.bpm / 60.0;
	// Master phase within an N-beat window, normalised to [0, N).
	double phi = std::fmod(mBeats, N);
	if (phi < 0.0) phi += N;

	double sBeats = (sPosSecs - sGrid.gridOffsetSecs) * sGrid.bpm / 60.0;
	double newSBeats = std::round((sBeats - phi) / N) * N + phi;

	double newSPosSecs = sGrid.gridOffsetSecs + newSBeats * 60.0 / sGrid.bpm;

	// If the snap landed before the start of the file, advance one window.
	if (newSPosSecs < 0.0)
		newSPosSecs += N * 60.0 / sGrid.bpm;

	// Clamp into valid range.
	if (newSPosSecs < 0.0)    newSPosSecs = 0.0;
	if (newSPosSecs > sLen)   newSPosSecs = sLen - 1e-3;

	players[slaveDeckIdx]->setPositionRelative(newSPosSecs / sLen);
}

//==============================================================================

double BeatSyncManager::computeSpeedRatio(int slaveDeckIdx) const {
	if (slaveDeckIdx < 0 || slaveDeckIdx > 1) return 0.0;
	int mIdx = masterIndex();
	if (mIdx < 0)                              return 0.0;
	if (players[mIdx] == nullptr)              return 0.0;
	if (players[slaveDeckIdx] == nullptr)      return 0.0;

	double masterEffectiveBpm = players[mIdx]->getCurrentBpm();
	double slaveRawBpm        = players[slaveDeckIdx]->getBeatGrid().bpm;
	if (masterEffectiveBpm <= 0.0 || slaveRawBpm <= 0.0) return 0.0;

	double mult = slaveMultiplier[slaveDeckIdx];
	if (mult <= 0.0) mult = 1.0;

	// Slave's raw audio at speed R has effective BPM = slaveRawBpm * R.
	// Multiplier expresses the user's interpretation of the slave's beat
	// (e.g. ×2 means "treat slave as double-time"), so the matching BPM is
	// slaveRawBpm * mult, and the required ratio is master / (slaveRawBpm * mult).
	return masterEffectiveBpm / (slaveRawBpm * mult);
}

//==============================================================================

double BeatSyncManager::autoResolveMultiplier(double masterBpm, double slaveBpm) {
	if (masterBpm <= 0.0 || slaveBpm <= 0.0) return 1.0;

	const double candidates[3] = { 0.5, 1.0, 2.0 };
	double bestMult            = 1.0;
	double bestDistance        = std::numeric_limits<double>::max();

	for (double mult : candidates) {
		double ratio = masterBpm / (slaveBpm * mult);
		if (ratio < kSyncMinSpeed - 1e-9 || ratio > kSyncMaxSpeed + 1e-9)
			continue;

		double distance = std::abs(ratio - 1.0);
		if (distance < bestDistance) {
			bestDistance = distance;
			bestMult     = mult;
		}
	}

	return bestMult;
}

//==============================================================================

void BeatSyncManager::refreshDeckUIs() {
	for (int i = 0; i < 2; ++i) {
		if (deckGUIs[i] != nullptr)
			deckGUIs[i]->syncStateChanged();
	}
}

int BeatSyncManager::masterIndex() const {
	if (currentMaster == MasterDeck::Deck1) return 0;
	if (currentMaster == MasterDeck::Deck2) return 1;
	return -1;
}

int BeatSyncManager::otherIndex(int idx) {
	return idx == 0 ? 1 : 0;
}

//==============================================================================

void BeatSyncManager::setSnapBeats(int beats) {
	if (beats != 1 && beats != 2 && beats != 4) return;
	if (snapBeats == beats) return;
	snapBeats = beats;

	// Re-align any active slave to the new snap granularity.
	for (int i = 0; i < 2; ++i) {
		if (syncEngaged[i])
			phaseAlignSlave(i);
	}
}

int BeatSyncManager::getSnapBeats() const {
	return snapBeats;
}
