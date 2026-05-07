#pragma once

#include <JuceHeader.h>
#include "../../core/analysis/BeatGrid.h"

/**
 * Utility class for loading and saving per-track beat grid configuration.
 *
 * Beat grid data is stored as JSON files in ~/.otodecks/grids/ using
 * the track's identity hash as the filename.
 */
class BeatGridConfig {
public:
	/**
	 * Load beat grid configuration for a track.
	 *
	 * @param trackIdentity The identity hash string of the track
	 * @return BeatGrid with saved values, or default BeatGrid if no config exists
	 */
	static BeatGrid load(const juce::String& trackIdentity);

	/**
	 * Save beat grid configuration for a track.
	 *
	 * @param trackIdentity The identity hash string of the track
	 * @param grid The BeatGrid data to persist
	 */
	static void save(const juce::String& trackIdentity, const BeatGrid& grid);

private:
	/** @return The directory path for grid config files */
	static juce::File getGridDirectory();

	/** @return The file path for a specific track's grid config */
	static juce::File getGridFile(const juce::String& trackIdentity);
};
