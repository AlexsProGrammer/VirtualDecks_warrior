
#include "DJAudioPlayer.h"
#include <chrono>
#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>

//============================================================================== 

/**
 * Implementation of a constructor for DJAudioPlayer
 *
 * Initializes juce::AudioFormatManager pointer data member
 *
 */
DJAudioPlayer::DJAudioPlayer(juce::AudioFormatManager& _formatManager)
	: formatManager(_formatManager)
{
};

/**
 * Implementation of a destructor for DJAudioPlayer
 *
 */
DJAudioPlayer::~DJAudioPlayer()
{
	const int64_t worst = worstCaseCallbackMicros.load(std::memory_order_relaxed);
	if (worst > 0)
		DBG("[DJAudioPlayer] worst-case getNextAudioBlock: " << worst << " µs");
}

//==============================================================================

/**
 * Implementation of prepareToPlay method for DJAudioPlayer
 *
 * Calls prepareToPlay methods on all AudioSource data members and saves the sample rate
 *
 */
void DJAudioPlayer::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
	transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
	resampleSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
	audioLPFilter.prepareToPlay(samplesPerBlockExpected, sampleRate);
	audioHPFilter.prepareToPlay(samplesPerBlockExpected, sampleRate);
	audioLBFilter.prepareToPlay(samplesPerBlockExpected, sampleRate);
	audioMBFilter.prepareToPlay(samplesPerBlockExpected, sampleRate);
	audioHBFilter.prepareToPlay(samplesPerBlockExpected, sampleRate);
	thisSampleRate = sampleRate;
};

/**
 * Implementation of getNextAudioBlock method for DJAudioPlayer
 *
 * Calls getNextAudioBlock methods on the main AudioSource data member and updates the root mean square value
 *
 */
void DJAudioPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
	const auto t0 = std::chrono::steady_clock::now();

	// 1) Drain UI → audio commands first. Allocation- and lock-free.
	drainCommands();

	// 2) Pull audio through the filter chain.
	audioLPFilter.getNextAudioBlock(bufferToFill);

	// 3) Loop: if active and playhead has passed the out point, jump back to in point.
	const bool   activeNow = loopActive.load(std::memory_order_acquire);
	const double inSecs    = loopInSecs.load(std::memory_order_acquire);
	const double outSecs   = loopOutSecs.load(std::memory_order_acquire);
	if (activeNow && inSecs >= 0.0 && outSecs > inSecs) {
		if (transportSource.getCurrentPosition() >= outSecs)
			transportSource.setPosition(inSecs);
	}

	// 4) Update RMS for UI meters.
	const float rmsLevelLeft  = juce::Decibels::gainToDecibels(bufferToFill.buffer->getRMSLevel(0, 0, bufferToFill.buffer->getNumSamples()));
	const float rmsLevelRight = juce::Decibels::gainToDecibels(bufferToFill.buffer->getRMSLevel(1, 0, bufferToFill.buffer->getNumSamples()));
	level.store((rmsLevelLeft + rmsLevelRight) / 2.0f, std::memory_order_release);

	// 5) Track worst-case callback duration for profiling (printed on shutdown).
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - t0).count();
	int64_t prev = worstCaseCallbackMicros.load(std::memory_order_relaxed);
	while (elapsed > prev &&
	       !worstCaseCallbackMicros.compare_exchange_weak(prev, elapsed,
	                                                       std::memory_order_relaxed))
	{}
};

//==============================================================================

/**
 * Drain queued AudioCommand entries on the audio thread. Called once per
 * getNextAudioBlock prior to any audio processing.
 */
void DJAudioPlayer::drainCommands() noexcept {
	commandFifo.drain([this](const AudioCommand& cmd) noexcept {
		applyCommand(cmd);
	});
}

/**
 * Apply a single AudioCommand on the audio thread. No allocation, no
 * UI-thread calls. JUCE-internal SpinLocks (filters / transport) are the
 * only synchronisation taken here.
 */
void DJAudioPlayer::applyCommand(const AudioCommand& cmd) noexcept {
	switch (cmd.tag)
	{
		case AudioCommand::Tag::Start:                transportSource.start(); break;
		case AudioCommand::Tag::Stop:                 transportSource.stop();  break;
		case AudioCommand::Tag::SetSpeed:
			if (cmd.doublePayload >= 0 && cmd.doublePayload <= 100.0) {
				resampleSource.setResamplingRatio(cmd.doublePayload);
				currentSpeedRatio.store(cmd.doublePayload, std::memory_order_release);
			}
			break;
		case AudioCommand::Tag::SetGainPlayer:
			playerVol = cmd.doublePayload;
			transportSource.setGain(playerVol * crossFadeVol);
			break;
		case AudioCommand::Tag::SetGainCrossfade:
			crossFadeVol = cmd.doublePayload;
			transportSource.setGain(playerVol * crossFadeVol);
			break;
		case AudioCommand::Tag::SetPosition:           transportSource.setPosition(cmd.doublePayload); break;
		case AudioCommand::Tag::SetPositionRelative:
			if (cmd.doublePayload >= 0 && cmd.doublePayload <= 1.0)
				transportSource.setPosition(transportSource.getLengthInSeconds() * cmd.doublePayload);
			break;
		case AudioCommand::Tag::SetLoopIn: {
			const double pos = transportSource.getCurrentPosition();
			loopInSecs.store(pos, std::memory_order_release);
			const double out = loopOutSecs.load(std::memory_order_acquire);
			if (out >= 0.0 && pos >= out) {
				loopOutSecs.store(-1.0, std::memory_order_release);
				loopActive.store(false, std::memory_order_release);
			}
			break;
		}
		case AudioCommand::Tag::SetLoopOut: {
			const double pos = transportSource.getCurrentPosition();
			const double in  = loopInSecs.load(std::memory_order_acquire);
			if (in >= 0.0 && pos > in) {
				loopOutSecs.store(pos, std::memory_order_release);
				loopActive.store(true, std::memory_order_release);
			}
			break;
		}
		case AudioCommand::Tag::ToggleReloop: {
			const double in  = loopInSecs.load(std::memory_order_acquire);
			const double out = loopOutSecs.load(std::memory_order_acquire);
			if (in < 0.0 || out < 0.0) break;
			const bool nowActive = ! loopActive.load(std::memory_order_acquire);
			loopActive.store(nowActive, std::memory_order_release);
			if (nowActive) transportSource.setPosition(in);
			break;
		}
		case AudioCommand::Tag::HalveLoop: {
			const double in  = loopInSecs.load(std::memory_order_acquire);
			const double out = loopOutSecs.load(std::memory_order_acquire);
			if (in < 0.0 || out <= in) break;
			const double newOut = in + (out - in) / 2.0;
			loopOutSecs.store(newOut, std::memory_order_release);
			if (loopActive.load(std::memory_order_acquire) && transportSource.getCurrentPosition() >= newOut)
				transportSource.setPosition(in);
			break;
		}
		case AudioCommand::Tag::DoubleLoop: {
			const double in  = loopInSecs.load(std::memory_order_acquire);
			const double out = loopOutSecs.load(std::memory_order_acquire);
			if (in < 0.0 || out <= in) break;
			double newOut = in + (out - in) * 2.0;
			const double trackLen = transportSource.getLengthInSeconds();
			if (newOut > trackLen) newOut = trackLen;
			loopOutSecs.store(newOut, std::memory_order_release);
			break;
		}
		case AudioCommand::Tag::ClearLoop:
			loopInSecs.store(-1.0,  std::memory_order_release);
			loopOutSecs.store(-1.0, std::memory_order_release);
			loopActive.store(false, std::memory_order_release);
			break;
		case AudioCommand::Tag::SetFilter: {
			const double freq = cmd.doublePayload;
			if (freq > 0 && freq < 20000) {
				audioLPFilter.makeInactive();
				audioHPFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(thisSampleRate, freq));
			}
			else if (freq < 0 && freq > -20000) {
				audioHPFilter.makeInactive();
				audioLPFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(thisSampleRate, 20000 + freq));
			}
			else {
				audioHPFilter.makeInactive();
				audioLPFilter.makeInactive();
			}
			break;
		}
		case AudioCommand::Tag::SetLBFilter:
			audioLBFilter.setCoefficients(juce::IIRCoefficients::makeLowShelf(thisSampleRate, 500, 1.0 / juce::MathConstants<double>::sqrt2, cmd.doublePayload));
			break;
		case AudioCommand::Tag::SetMBFilter:
			audioMBFilter.setCoefficients(juce::IIRCoefficients::makePeakFilter(thisSampleRate, 3250, 1.0 / juce::MathConstants<double>::sqrt2, cmd.doublePayload));
			break;
		case AudioCommand::Tag::SetHBFilter:
			audioHBFilter.setCoefficients(juce::IIRCoefficients::makeHighShelf(thisSampleRate, 5000, 1.0 / juce::MathConstants<double>::sqrt2, cmd.doublePayload));
			break;
		case AudioCommand::Tag::BeatJump: {
			if (! loaded.load(std::memory_order_acquire)) break;
			// beatGrid.bpm is UI-thread only; reading here is racy in principle.
			// BeatJump is reserved for Phase 3 when the grid is shared atomically.
			// For now this branch is unused (BeatJump never enqueued yet).
			break;
		}
		case AudioCommand::Tag::None:
		default:
			break;
	}
}

/**
 * Implementation of releaseResources method for DJAudioPlayer
 *
 * Calls releaseResources methods on the main AudioSource data member
 *
 */
void DJAudioPlayer::releaseResources() {
	audioLPFilter.releaseResources();
};

//============================================================================== 

/**
 * Implementation of start method for DJAudioPlayer
 *
 *  Calls the start method on the AudioTransportSource data member
 *
 */
void DJAudioPlayer::start() {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::Start;
	commandFifo.push(cmd);
};

/**
 * Implementation of stop method for DJAudioPlayer
 *
 *  Calls the stop method on the AudioTransportSource data member
 *
 */
void DJAudioPlayer::stop() {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::Stop;
	commandFifo.push(cmd);
};

/**
 * Implementation of isPlaying method for DJAudioPlayer
 *
 * Returns if the AudioTransportSource data member is playing
 *
 */
bool DJAudioPlayer::isPlaying() {
	return transportSource.isPlaying();
}

/**
 * Implementation of isLoaded method for DJAudioPlayer
 *
 * Returns the loaded data member
 *
 */
bool DJAudioPlayer::isLoaded() {
	return loaded.load(std::memory_order_acquire);
}

/**
 * Implementation of returnURL method for DJAudioPlayer
 *
 * Returns the currentAudioURL data member
 *
 */
juce::URL DJAudioPlayer::returnURL() {
	return currentAudioURL;
}

/**
 * Implementation of loadURL method for DJAudioPlayer
 *
 * Creates a reader for the juce::URL and parses it into a juce::AudioFormatReaderSource
 * The AudioTransportSource data member sets it source using the juce::AudioFormatReaderSource
 *
 */
void DJAudioPlayer::loadURL(juce::URL audioURL) {
	loadingState.store(LoadingState::Loading, std::memory_order_release);
	auto* reader = formatManager.createReaderFor(audioURL.createInputStream(false));
	if (reader != nullptr) {
		std::unique_ptr<juce::AudioFormatReaderSource> newSource(new juce::AudioFormatReaderSource(reader, true));
		installLoadedSource(std::move(newSource), reader->sampleRate, audioURL);
	}
	else
	{
		DBG("Something went wrong loading the file ");
		loaded.store(false, std::memory_order_release);
		loadingState.store(LoadingState::Failed, std::memory_order_release);
	}
};

/**
 * Implementation of installLoadedSource method for DJAudioPlayer
 *
 * Hands ownership of a pre-built reader source to this player. Called on the
 * message thread by AudioEngine after off-thread reader creation.
 */
void DJAudioPlayer::installLoadedSource(std::unique_ptr<juce::AudioFormatReaderSource> newSource,
										double sampleRate,
										juce::URL audioURL)
{
	// Replace transport source. AudioTransportSource::setSource takes its
	// internal CriticalSection; the audio callback also takes that lock,
	// but the contention window is microseconds.
	transportSource.setSource(newSource.get(), 0, nullptr, sampleRate);
	readerSource = std::move(newSource);
	loadedFileName = audioURL.getFileName();
	currentAudioURL = audioURL;

	// Reset BPM + loop state on every track change.
	detectedBpm = 0.0;
	beatGrid = BeatGrid();
	loopInSecs.store(-1.0, std::memory_order_release);
	loopOutSecs.store(-1.0, std::memory_order_release);
	loopActive.store(false, std::memory_order_release);

	loaded.store(true, std::memory_order_release);
	loadingState.store(LoadingState::Loaded, std::memory_order_release);
}

//==============================================================================

/**
 * Implementation of getRMSLevel method for DJAudioPlayer
 *
 * Returns the level data member
 *
 */
float DJAudioPlayer::getRMSLevel() {
	return level.load(std::memory_order_acquire);
};

/**
 * Implementation of getPositionRelative method for DJAudioPlayer
 *
 * Returns the relative position of the AudioTransportSource data member.
 * Value returned is between 0 and 1.
 *
 */
double DJAudioPlayer::getPositionRelative() {
	return (transportSource.getLengthInSeconds() == 0 ? 0 : transportSource.getCurrentPosition() / transportSource.getLengthInSeconds());
}

/**
 * Implementation of getLengthInSeconds method for DJAudioPlayer
 *
 * Returns the total length in seconds of the loaded audio source.
 */
double DJAudioPlayer::getLengthInSeconds() {
	return transportSource.getLengthInSeconds();
}

//==============================================================================

/**
 * Implementation of setGain method for DJAudioPlayer
 *
 * Checks if gain value passed in is called from a volume
 * functionality before setting the playerVolume.
 * Non volume functionality would impact the cross fader
 * volume.
 * Calls the setGain method on the AudioTransportSource data member,
 * passing in the multiplication of the player volume and cross fader volume.
 *
 */
void DJAudioPlayer::setGain(double gain, bool isVol) {
	if (gain < 0 || gain > 1.0) {
		DBG("DJAudioPlayer:: setGain Gain should be between 0 and 1");
		return;
	}
	AudioCommand cmd;
	cmd.tag = isVol ? AudioCommand::Tag::SetGainPlayer
	                : AudioCommand::Tag::SetGainCrossfade;
	cmd.doublePayload = gain;
	commandFifo.push(cmd);
};

/**
 * Implementation of setSpeed method for DJAudioPlayer
 *
 * If conditional acting as guard clause, ensuring resampling
 * ratio isnt set below 0 or above 100.
 * Sets ResamplingAudioSource data member's resampling ratio with
 * passed in value
 *
 */
void DJAudioPlayer::setSpeed(double ratio) {
	if (ratio < 0 || ratio > 100.0) {
		DBG("DJAudioPlayer:: setSpeed Ratio should be between 0 and 100");
		return;
	}
	// Update the UI-visible cached ratio immediately so getSpeedRatio() and
	// any beat-grid display logic see the latest value before the audio
	// thread drains the command.
	currentSpeedRatio.store(ratio, std::memory_order_release);
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetSpeed; cmd.doublePayload = ratio;
	commandFifo.push(cmd);
};

/**
 * Implementation of setPosition method for DJAudioPlayer
 *
 * Sets the playback position by calling setPosition method
 * in the AudioTransportSource data member
 *
 */
void DJAudioPlayer::setPosition(double posInSecs) {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetPosition; cmd.doublePayload = posInSecs;
	commandFifo.push(cmd);
};

/**
 * Implementation of setPositionRelative method for DJAudioPlayer
 *
 * If conditional guard clause ensuring passed in value is between 0 and 1.
 * Converts value into a length in seconds and calls setPosition with converted value.
 *
 */
void DJAudioPlayer::setPositionRelative(double pos) {
	if (pos < 0 || pos > 1) {
		DBG("DJAudioPlayer:: setPositionRelative pos should be between 0 and 1");
		return;
	}
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetPositionRelative; cmd.doublePayload = pos;
	commandFifo.push(cmd);
}

/**
 * Implementation of setFilter method for DJAudioPlayer
 *
 * Sets the high pass IIRCoefficients or low pass IIRCoefficients
 * on the IIRFilterAudioSource data members depending on the freq
 * value passed in
 *
 */
void DJAudioPlayer::setFilter(double freq) {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetFilter; cmd.doublePayload = freq;
	commandFifo.push(cmd);
}

/**
 * Implementation of setLBFilter method for DJAudioPlayer
 *
 * Sets the low shelf IIRCoefficients on the IIRFilterAudioSource
 * data member depending on the gain value passed in
 *
 */
void DJAudioPlayer::setLBFilter(double gain) {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetLBFilter; cmd.doublePayload = gain;
	commandFifo.push(cmd);
};

/**
 * Implementation of setMBFilter method for DJAudioPlayer
 *
 * Sets the peak filter IIRCoefficients on the IIRFilterAudioSource
 * data member depending on the gain value passed in
 *
 */
void DJAudioPlayer::setMBFilter(double gain) {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetMBFilter; cmd.doublePayload = gain;
	commandFifo.push(cmd);
};

/**
 * Implementation of setHBFilter method for DJAudioPlayer
 *
 * Sets the high shelf IIRCoefficients on the IIRFilterAudioSource
 * data member depending on the gain value passed in
 *
 */
void DJAudioPlayer::setHBFilter(double gain) {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetHBFilter; cmd.doublePayload = gain;
	commandFifo.push(cmd);
};

//==============================================================================

/**
 * Implementation of getDetectedBpm method for DJAudioPlayer
 *
 * Returns the BPM from the beat grid (may be manual override or detected).
 */
double DJAudioPlayer::getDetectedBpm() const {
	return detectedBpm;
}

/**
 * Implementation of getCurrentBpm method for DJAudioPlayer
 *
 * Returns the effective BPM adjusted by the current speed ratio.
 */
double DJAudioPlayer::getCurrentBpm() const {
	return beatGrid.bpm * currentSpeedRatio.load(std::memory_order_acquire);
}

/**
 * Implementation of getSpeedRatio method for DJAudioPlayer
 *
 * Returns the current resampling speed ratio.
 */
double DJAudioPlayer::getSpeedRatio() const {
	return currentSpeedRatio.load(std::memory_order_acquire);
}

/**
 * Implementation of getBeatGrid method for DJAudioPlayer
 *
 * Returns the current beat grid.
 */
const BeatGrid& DJAudioPlayer::getBeatGrid() const {
	return beatGrid;
}

/**
 * Implementation of beatJump method for DJAudioPlayer
 *
 * Jumps the playhead forward or backward by a given number of beats.
 * Requires a valid BPM in the beat grid to calculate beat duration.
 */
void DJAudioPlayer::beatJump(int beats) {
	if (! loaded.load(std::memory_order_acquire) || beatGrid.bpm <= 0.0)
		return;

	const double secondsPerBeat = 60.0 / beatGrid.bpm;
	const double jumpSecs       = beats * secondsPerBeat;
	const double currentPos     = transportSource.getCurrentPosition();
	double       newPos         = currentPos + jumpSecs;

	if (newPos < 0.0) newPos = 0.0;
	const double len = transportSource.getLengthInSeconds();
	if (newPos > len) newPos = len;

	// Route the actual seek through the FIFO so the audio thread is the
	// only thing that calls transportSource.setPosition.
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetPosition; cmd.doublePayload = newPos;
	commandFifo.push(cmd);
}

//==============================================================================

/**
 * Implementation of setLoopIn method for DJAudioPlayer
 *
 * Stores the current playback position as the loop-in point.
 */
void DJAudioPlayer::setLoopIn() {
	if (! loaded.load(std::memory_order_acquire)) return;
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetLoopIn;
	commandFifo.push(cmd);
}

/**
 * Implementation of setLoopOut method for DJAudioPlayer
 *
 * Stores the current playback position as the loop-out point
 * and activates looping if the in point is already set.
 */
void DJAudioPlayer::setLoopOut() {
	if (! loaded.load(std::memory_order_acquire)) return;
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::SetLoopOut;
	commandFifo.push(cmd);
}

/**
 * Implementation of toggleReloop method for DJAudioPlayer
 *
 * Toggles loop on/off. When re-enabling, jumps back to the loop-in point.
 */
void DJAudioPlayer::toggleReloop() {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::ToggleReloop;
	commandFifo.push(cmd);
}

/**
 * Implementation of halveLoop method for DJAudioPlayer
 *
 * Halves the loop length by moving the out point halfway between in and current out.
 */
void DJAudioPlayer::halveLoop() {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::HalveLoop;
	commandFifo.push(cmd);
}

/**
 * Implementation of doubleLoop method for DJAudioPlayer
 *
 * Doubles the loop length by extending the out point, clamped to track length.
 */
void DJAudioPlayer::doubleLoop() {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::DoubleLoop;
	commandFifo.push(cmd);
}

/**
 * Implementation of clearLoop method for DJAudioPlayer
 *
 * Clears all loop points and deactivates looping.
 */
void DJAudioPlayer::clearLoop() {
	AudioCommand cmd; cmd.tag = AudioCommand::Tag::ClearLoop;
	commandFifo.push(cmd);
}

/**
 * Implementation of isLooping method for DJAudioPlayer
 */
bool DJAudioPlayer::isLooping() const {
	return loopActive.load(std::memory_order_acquire);
}

/**
 * Implementation of getLoopInRelative method for DJAudioPlayer
 */
double DJAudioPlayer::getLoopInRelative() const {
	const double in = loopInSecs.load(std::memory_order_acquire);
	if (in < 0.0 || transportSource.getLengthInSeconds() <= 0.0)
		return -1.0;
	return in / transportSource.getLengthInSeconds();
}

/**
 * Implementation of getLoopOutRelative method for DJAudioPlayer
 */
double DJAudioPlayer::getLoopOutRelative() const {
	const double out = loopOutSecs.load(std::memory_order_acquire);
	if (out < 0.0 || transportSource.getLengthInSeconds() <= 0.0)
		return -1.0;
	return out / transportSource.getLengthInSeconds();
}

//==============================================================================

/**
 * Implementation of setBeatGrid method for DJAudioPlayer
 *
 * Updates the beat grid for the loaded track.
 */
void DJAudioPlayer::setBeatGrid(const BeatGrid& grid) {
	beatGrid = grid;
	detectedBpm = grid.bpm;
}

