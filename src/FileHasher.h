#pragma once

#include <JuceHeader.h>
#include <functional>

/**
 * Utility for computing a content-based hash of an audio file.
 *
 * Reads the first and last 64 KB of the file plus the file size
 * to produce a hex string. This allows the same file to be
 * recognised across re-imports without reading the entire file.
 */
class FileHasher {
public:
	/**
	 * Compute a partial-content hash of the given file (synchronous).
	 *
	 * Uses FNV-1a (64-bit) over the first+last 64 KB and file size.
	 * Performs blocking disk I/O - only call from a background thread.
	 *
	 * @param file The audio file to hash
	 * @return Hex string of the hash, or empty string on failure
	 */
	static juce::String computeHash(const juce::File& file);

	/**
	 * Asynchronously compute a partial-content hash on a background worker.
	 * The callback is invoked on the message thread once the hash is ready.
	 *
	 * @param file   The audio file to hash
	 * @param onDone Callback invoked on the message thread with the hash string
	 */
	static void computeHashAsync(juce::File file,
	                             std::function<void(juce::String)> onDone);
};
