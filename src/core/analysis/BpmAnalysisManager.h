#pragma once

#include <JuceHeader.h>
#include "BpmDetector.h"
#include "../data/TrackDataCache.h"

/**
 * Background BPM analysis manager using a thread pool.
 *
 * Queues audio files for BPM analysis on background threads and notifies
 * listeners on the message thread when results are ready. Automatically
 * caches results via TrackDataCache to avoid re-analysis.
 */
class BpmAnalysisManager {
public:
	/**
	 * Listener interface for BPM analysis completion callbacks.
	 * Callbacks are invoked on the message thread.
	 */
	class Listener {
	public:
		virtual ~Listener() = default;

		/**
		 * Called when BPM analysis completes for a track.
		 *
		 * @param fileHash The content hash of the analysed file
		 * @param bpm The detected BPM (0.0 if detection failed)
		 */
		virtual void bpmAnalysisComplete(const juce::String& fileHash, double bpm) = 0;
	};

	/**
	 * Constructor.
	 *
	 * @param formatManager Reference to the app's AudioFormatManager
	 * @param numThreads Number of worker threads (default 2)
	 */
	BpmAnalysisManager(juce::AudioFormatManager& formatManager, int numThreads = 2);

	/** Destructor - waits for pending jobs to complete. */
	~BpmAnalysisManager();

	/**
	 * Queue a track for background BPM analysis.
	 *
	 * If cached data already exists for this hash (and BPM > 0, non-manual),
	 * the listener is notified immediately without re-analysing.
	 *
	 * @param audioFile The audio file to analyse
	 * @param fileHash The content hash of the file
	 */
	void analyzeTrack(const juce::File& audioFile, const juce::String& fileHash);

	/** Add a listener for analysis completion events. */
	void addListener(Listener* listener);

	/** Remove a previously added listener. */
	void removeListener(Listener* listener);

	/** Notify listeners on the calling thread (should be message thread). */
	void notifyListeners(const juce::String& fileHash, double bpm);

private:
	juce::AudioFormatManager& formatManager;
	juce::ThreadPool threadPool;
	juce::ListenerList<Listener> listeners;

	/**
	 * ThreadPoolJob that performs the actual BPM analysis.
	 */
	class AnalysisJob : public juce::ThreadPoolJob {
	public:
		AnalysisJob(BpmAnalysisManager& owner, const juce::File& file, const juce::String& hash);
		JobStatus runJob() override;

	private:
		BpmAnalysisManager& owner;
		juce::File audioFile;
		juce::String fileHash;
	};
};
