
#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "../analysis/BeatGrid.h"
#include "../effects/AudioCommandFifo.h"
#include "../effects/FxChain.h"

/**
 * Definition of a DJAudioplayer
 *
 * An AudioSource class that contains general player functionality.
 * Acts as an AudioSource interface that contains load, gain, playback
 * and filter functionality
 *
 */
class DJAudioPlayer : public juce::AudioSource {
public:

	//==============================================================================

	/**
		* Class Constructor for DJAudioPlayer, initializes member variables.
		*
		* @param juce::AudioFormatManager reference
	*/
	DJAudioPlayer(juce::AudioFormatManager& formatManager);

	/**
		* Class destructor for DJAudioPlayer
	*/
	~DJAudioPlayer();

	//==============================================================================

	/**
		* Prepares audio source members
		*
		* @param Expected samples in a block
		* @param Number of samples per second
	*/
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

	/**
		* Called repeatedly to fetch subsequent blocks of audio data.
		*
		* @param juce::AudioSourceChannelInfo&: Buffer to be filled by audio source
	*/
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

	/**
		* Release resources on audio sources.
	*/
	void releaseResources() override;

	//==============================================================================

	/**
		* Start playing the file
	*/
	void start();

	/**
	   * Stop playing the file
   */
	void stop();

	/**
	   * Returns true if the DJAudioPlayer is playing on the audio source, and false otherwise
   */
	bool isPlaying();

	/**
	   * Returns true if player is loaded with an audio file
   */
	bool isLoaded();

	/**
	   * Returns the juce::URL of the loaded audio file.
   */
	juce::URL returnURL();

	/**
		* Loads URL into the transport source.
		*
		* WARNING: this performs synchronous file I/O and format probing on the
		* calling thread. Prefer AudioEngine::requestLoad() which moves that work
		* onto a background ThreadPool. Retained for legacy callers.
		*
		* @param juce::URL of audio file to be loaded
	*/
	void loadURL(juce::URL audioURL);

	/**
		* Install a pre-built AudioFormatReaderSource. Must be invoked on the
		* message thread (the AudioEngine load pool calls this via
		* MessageManager::callAsync once the reader has been created off-thread).
		* Hands ownership of the source to this player and atomically marks the
		* deck as Loaded.
		*
		* @param newSource Heap-allocated reader source (ownership transferred).
		* @param sampleRate Source sample rate captured during reader creation.
		* @param audioURL  Original URL for state tracking.
	*/
	void installLoadedSource(std::unique_ptr<juce::AudioFormatReaderSource> newSource,
							double sampleRate,
							juce::URL audioURL);

	/**
		* Loading-state of the deck. Used by DeckGUI to disable controls and
		* show a "Loading…" overlay while a track is being prepared off-thread.
	*/
	enum class LoadingState { Idle, Loading, Loaded, Failed };

	/// Returns the current loading state (atomic, safe from any thread).
	LoadingState getLoadingState() const noexcept { return loadingState.load(std::memory_order_acquire); }

	/// Marks the deck as loading. Called by AudioEngine before queueing the load job.
	void markLoading() noexcept { loadingState.store(LoadingState::Loading, std::memory_order_release); }

	/// Marks the deck as failed (e.g. unreadable file). Called from the message thread.
	void markLoadFailed() noexcept { loadingState.store(LoadingState::Failed, std::memory_order_release); }

	//==============================================================================

	/**
	   * Returns the rms level of the audio source via the level variable member
   */
	float getRMSLevel();

	/**
	   * Get the relative position of the playhead
   */
	double getPositionRelative();

	/**
	   * Get the total length of the loaded track in seconds
	 */
	double getLengthInSeconds();

	//==============================================================================

	/**
		* Set gain of the file
		*
		*  @param Gain of audio source, between 0 to 1
		*  @param True if called from volume functionality, false otherwise.
	*/
	void setGain(double gain, bool isVol = true);

	/**
		* Set speed of file playing by setting resampling audio source ratio
		*
		* @param Ratio of the resampling source
	*/
	void setSpeed(double ratio);

	/**
		* Set position of the file playback in seconds
		*
		* @param Position of the audio source playback in seconds.
	*/
	void setPosition(double posInSecs);

	/**
		* Set relative position of the file playback, calls setPosition
		*
		* @param Relative position of the audio source playback between 0 and 1.
	*/
	void setPositionRelative(double pos);

	/**
	   * Sets the IIR coefficients of audioLPFilter and audioHPFilter audio sources
	   *
	   * @param Frequency to perform low or high pass from -20000 to 20000.
   */
	void setFilter(double freq);

	/**
	   * Sets the IIR coefficients of audioLBFilter audio source
	   *
	   * @param Gain factor of the audio source in the low band.
   */
	void setLBFilter(double gain);

	/**
	   * Sets the IIR coefficients of audioMBFilter audio source
	   *
	   * @param Gain factor of the audio source in the mid band.
   */
	void setMBFilter(double gain);

	/**
	   * Sets the IIR coefficients of audioHBFilter audio source
	   *
	   * @param Gain factor of the audio source in the high band.
   */
	void setHBFilter(double gain);

	//==============================================================================

	/**
	 * Get the detected or manually set BPM of the loaded track.
	 * Returns 0.0 if no BPM has been determined.
	 */
	double getDetectedBpm() const;

	/**
	 * Get the effective BPM (detected BPM adjusted by current speed ratio).
	 */
	double getCurrentBpm() const;

	/**
	 * Get the current speed/resampling ratio.
	 */
	double getSpeedRatio() const;

	/**
	 * Jump forward or backward by a given number of beats.
	 * Uses the current beat grid BPM to calculate the distance.
	 *
	 * @param beats Number of beats to jump (negative = backward)
	 */
	void beatJump(int beats);

	//==============================================================================

	/**
	 * Set the loop-in point at the current playback position.
	 */
	void setLoopIn();

	/**
	 * Set the loop-out point at the current playback position and activate looping.
	 */
	void setLoopOut();

	/**
	 * Toggle loop on/off. If loop points are set, re-enable or disable looping.
	 */
	void toggleReloop();

	/**
	 * Halve the loop length by moving the loop-out point closer to loop-in.
	 */
	void halveLoop();

	/**
	 * Double the loop length by moving the loop-out point further from loop-in.
	 */
	void doubleLoop();

	/**
	 * Clear all loop points and deactivate looping.
	 */
	void clearLoop();

	/**
	 * @return True if a loop is currently active.
	 */
	bool isLooping() const;

	/**
	 * Get the loop-in position as a relative value (0 to 1).
	 * Returns -1.0 if not set.
	 */
	double getLoopInRelative() const;

	/**
	 * Get the loop-out position as a relative value (0 to 1).
	 * Returns -1.0 if not set.
	 */
	double getLoopOutRelative() const;

	//==============================================================================

	/**
	 * Get the current beat grid for the loaded track.
	 */
	const BeatGrid& getBeatGrid() const;

	/**
	 * Set the beat grid for the loaded track.
	 *
	 * @param grid The BeatGrid to apply
	 */
	void setBeatGrid(const BeatGrid& grid);

	//==============================================================================

	/**
	 * Producer-side handle to push an AudioCommand onto this player's FIFO.
	 * Safe to call from the UI message thread. The command is consumed at the
	 * top of the next getNextAudioBlock callback.
	 */
	bool postCommand(const AudioCommand& cmd) noexcept { return commandFifo.push(cmd); }

	//==============================================================================

	/**
	 * Direct access to this player's FX chain. The chain is built once in
	 * the constructor; UI components use it to enumerate processors, drive
	 * parameter sliders (via atomic FxParameter::set) and read selection
	 * state. Mutations that affect audio-thread state (selection / engage)
	 * MUST be posted through postCommand() rather than mutating the chain
	 * directly.
	 */
	FxChain&       getFxChain()       noexcept { return fxChain; }
	const FxChain& getFxChain() const noexcept { return fxChain; }

	//==============================================================================
	// Headphone cue tap

	/**
	 * Enable or disable copying this deck's processed output into the cue ring
	 * buffer so a separate headphone device callback can consume it.
	 * Safe to call from the message thread at any time.
	 */
	void enableCueTap(bool enable) noexcept;

	/**
	 * Mute this deck on the master output. When true, the processed audio is still
	 * copied to the cue ring buffer (headphones hear the audio) but the master output
	 * is zeroed. Used to implement the DJ monitor behavior where CUE routes a deck's
	 * audio to headphones only. Safe to call from the message thread at any time.
	 */
	void setMasterMuted(bool mute) noexcept;

	/**
	 * Read up to numSamples frames from the cue ring buffer into destChannels.
	 * Returns the number of frames actually read (may be less if buffer is
	 * starved). Remaining output frames are zeroed by the caller.
	 * Must be called from the headphone audio thread only.
	 */
	int readCueTap(float* const* destChannels, int numChannels, int numSamples) noexcept;

	//==============================================================================

	/// Drain pending commands from the audio thread. Allocation- and lock-free.
	void drainCommands() noexcept;

	/// Apply a single command on the audio thread. Called only by drainCommands.
	void applyCommand(const AudioCommand& cmd) noexcept;


	/// Reference assigned to the AudioFormatManager passed into the constructor
	juce::AudioFormatManager& formatManager;

	/// Reader source for the audio url
	std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

	/// AudioTransportSource to manage basic gain and playback controls.
	juce::AudioTransportSource transportSource;

	/// ResamplingAudioSource to manage resampling ratio controls
	juce::ResamplingAudioSource resampleSource{ &transportSource, false, 2 };

	/// IIRFilterAudioSource to manage low band filter controls
	juce::IIRFilterAudioSource audioLBFilter{ &resampleSource , false };

	/// IIRFilterAudioSource to manage mid band filter controls
	juce::IIRFilterAudioSource audioMBFilter{ &audioLBFilter , false };

	/// IIRFilterAudioSource to manage high band filter controls
	juce::IIRFilterAudioSource audioHBFilter{ &audioMBFilter , false };

	/// IIRFilterAudioSource to manage high pass filter controls
	juce::IIRFilterAudioSource audioHPFilter{ &audioHBFilter , false };

	/// IIRFilterAudioSource to manage low pass filter controls
	juce::IIRFilterAudioSource audioLPFilter{ &audioHPFilter , false };

	/// Per-deck FX engine. Wraps audioLPFilter as its upstream and runs
	/// Pad / Beat / Release slots in order on every audio block.
	FxChain fxChain { &audioLPFilter };

	/// juce::String to store the file name of the loaded url
	juce::String loadedFileName;

	/// double to store the sample rate
	double thisSampleRate;

	/// Atomic flag: true once a track is fully loaded and addressable.
	std::atomic<bool> loaded { false };

	/// Atomic loading-state for UI feedback (Idle/Loading/Loaded/Failed).
	std::atomic<LoadingState> loadingState { LoadingState::Idle };

	/// Atomic flag: when true, the master output buffer is zeroed after fxChain
	/// processing so audio plays on headphones only (CUE monitoring). The cue tap
	/// ring buffer still receives the unzeroed audio.
	std::atomic<bool> cueMuted { false };

	/// double to store the DeckGUI player volume (UI thread only).
	double playerVol = 1;

	/// double to store the cross fader volume (UI thread only).
	double crossFadeVol = 1;

	/// juce::URL to store the current loaded audio file's URL (UI thread only).
	juce::URL currentAudioURL;

	/// SPSC command FIFO: UI → audio thread.
	AudioCommandFifo<256> commandFifo;

	/// Atomic RMS level: written by audio thread, read by UI timer.
	std::atomic<float> level { -100.0f };

	/// Detected BPM from metadata or onset analysis (UI thread only).
	double detectedBpm = 0.0;

	/// Current speed/resampling ratio. Atomic because the audio-thread
	/// command drain writes it (via SetSpeed) while UI getters read it.
	std::atomic<double> currentSpeedRatio { 1.0 };

	/// Beat grid for the loaded track (UI thread only - not read by audio thread).
	BeatGrid beatGrid;

	/// Loop-in position in seconds (-1.0 = not set). Atomic for audio-thread reads.
	std::atomic<double> loopInSecs { -1.0 };

	/// Loop-out position in seconds (-1.0 = not set). Atomic for audio-thread reads.
	std::atomic<double> loopOutSecs { -1.0 };

	/// Whether the loop is currently active. Atomic for audio-thread reads.
	std::atomic<bool> loopActive { false };

	/// Worst-case getNextAudioBlock duration in microseconds (written by audio thread,
	/// read by destructor on message thread after audio device is closed).
	std::atomic<int64_t> worstCaseCallbackMicros { 0 };

	//==============================================================================
	// Headphone cue tap ring buffer (SPSC: audio thread writes, headphone thread reads)

	static constexpr int kCueFifoSize = 65536;

	/// True when the cue tap is active. Written on message thread, read on audio thread.
	std::atomic<bool> cueTapEnabled { false };

	/// Lock-free SPSC index tracker for the cue ring buffer.
	juce::AbstractFifo cueFifo { kCueFifoSize };

	/// Ring buffer that holds the tapped stereo audio. Allocated once.
	juce::AudioBuffer<float> cueBuffer { 2, kCueFifoSize };
};
