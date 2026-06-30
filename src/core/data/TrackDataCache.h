#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>
#include "../analysis/BeatGrid.h"

struct BandFrame; // forward-declared to avoid pulling WaveformBandAnalyzer.h here

/**
 * Data returned from the track data cache.
 *
 * Consolidates detected BPM, detection confidence, and the
 * beat grid (which may include manual overrides) into a single
 * structure keyed by file content hash.
 */
struct CuePoint {
	/// Relative track position for the cue (0.0 .. 1.0). -1.0 means unset.
	double relativePos = -1.0;

	/// Colour hue for the cue marker.
	float hue = 0.0f;
};

struct TrackData {
	/// Auto-detected BPM (0.0 if not yet analysed)
	double detectedBpm = 0.0;

	/// Confidence of the detection (0.0 to 1.0)
	double confidence = 0.0;

	/// Beat grid (may contain manual overrides)
	BeatGrid beatGrid;

	/// Per-track hot cue metadata.
	std::array<CuePoint, 6> hotCues {};

	/// Saved loop start/end as relative positions.
	double loopInRelative = -1.0;
	double loopOutRelative = -1.0;

	/// Whether the loop was active when saved.
	bool loopActive = false;
};

/**
 * Persistent cache for per-track analysis data, keyed by file content hash.
 *
 * Data is stored as JSON files in ~/.otodecks/trackdata/<fileHash>.json.
 * This replaces the old identity-based BeatGridConfig for new data.
 */
class TrackDataCache {
public:
	/**
	 * Load cached track data for a given file hash.
	 *
	 * @param fileHash The content hash of the audio file
	 * @return TrackData with saved values, or defaults if no cache exists
	 */
	static TrackData load(const juce::String& fileHash);

	/**
	 * Save track data for a given file hash.
	 *
	 * @param fileHash The content hash of the audio file
	 * @param data The TrackData to persist
	 */
	static void save(const juce::String& fileHash, const TrackData& data);

	/**
	 * Check whether cached data exists for a given file hash.
	 *
	 * @param fileHash The content hash of the audio file
	 * @return true if a cache file exists on disk
	 */
	static bool exists(const juce::String& fileHash);

	//==============================================================================
	// Async API - use these from the message thread to avoid blocking on disk I/O.

	/**
	 * Asynchronously load track data on a background worker. The callback is
	 * invoked on the message thread once the JSON has been read and parsed.
	 *
	 * @param fileHash The content hash of the audio file
	 * @param onLoaded Callback invoked on the message thread with the loaded data
	 */
	static void loadAsync(const juce::String& fileHash,
	                      std::function<void(TrackData)> onLoaded);

	/**
	 * Asynchronously read-modify-write track data on a background worker.
	 * The mutator runs on the worker thread (after the existing data has been
	 * loaded) and may freely mutate the TrackData; the worker writes the
	 * result back to disk. Fire-and-forget - no completion callback.
	 *
	 * Sequential calls for the same hash are serialized by the single-worker
	 * pool, so concurrent updates from rapid slider movement are safe.
	 *
	 * @param fileHash The content hash of the audio file
	 * @param mutator  Function applied to the TrackData on the worker thread
	 */
	static void updateAsync(const juce::String& fileHash,
	                        std::function<void(TrackData&)> mutator);

	//==============================================================================
	// Waveform band data (3-band RGB per-frame) - stored as a separate binary
	// blob next to the JSON track data, keyed by the same content hash.

	/**
	 * Load cached waveform band data for a track. Returns nullptr if the cache
	 * file does not exist or fails to parse. Safe to call from any thread.
	 *
	 * @param fileHash Content hash of the audio file
	 * @return Shared pointer to immutable band data, or nullptr on miss/failure
	 */
	static std::shared_ptr<const std::vector<BandFrame>> loadBands(const juce::String& fileHash);

	/**
	 * Persist waveform band data for a track. Overwrites any existing cache
	 * file. Safe to call from any thread.
	 *
	 * @param fileHash Content hash of the audio file
	 * @param bands    Frames to persist (typically produced by WaveformBandAnalyzer)
	 */
	static void saveBands(const juce::String& fileHash, const std::vector<BandFrame>& bands);

private:
	static juce::File getCacheDirectory();
	static juce::File getCacheFile(const juce::String& fileHash);
	static juce::File getBandsFile(const juce::String& fileHash);
};
