#pragma once

#include <JuceHeader.h>

/**
 * Data structure representing a beat grid for a track.
 *
 * Stores BPM, the time offset of the first beat, and flags indicating
 * whether the values were manually overridden by the user.
 */
struct BeatGrid {
	/// Beats per minute (0.0 if unknown)
	double bpm = 0.0;

	/// Time position of the first beat in seconds
	double gridOffsetSecs = 0.0;

	/// True if the user manually set the BPM
	bool isManualBpm = false;

	/// True if the user manually adjusted the grid offset
	bool isManualOffset = false;
};
