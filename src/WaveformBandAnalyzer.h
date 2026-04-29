#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

/**
 * One frame of 3-band waveform analysis data.
 *
 * Each waveform pixel column maps to one (or more) BandFrame; the renderer
 * tints the column using `juce::Colour::fromRGB(low, mid, high)` and draws
 * the column height from `amp`.
 *
 * Storage is 4 bytes/frame so a typical 5-minute track at 50 ms/frame
 * (~6000 frames) costs ~24 KB of RAM and disk.
 */
struct BandFrame {
	juce::uint8 low  = 0;   ///< Low-band  (R) energy 0..255
	juce::uint8 mid  = 0;   ///< Mid-band  (G) energy 0..255
	juce::uint8 high = 0;   ///< High-band (B) energy 0..255
	juce::uint8 amp  = 0;   ///< Peak amplitude 0..255 (for column height)
};

/// Shared, immutable container handed from the analyzer worker to UI components.
using BandData = std::vector<BandFrame>;
using BandDataPtr = std::shared_ptr<const BandData>;

/**
 * Offline 3-band RMS/peak analyzer for waveform colouring.
 *
 * Reads an audio file off-thread, runs three IIR-filter chains
 * (low <500 Hz, mid 500-5000 Hz, high >5000 Hz — matching DJAudioPlayer's
 * EQ crossovers), accumulates per-frame RMS energies + peak amplitude,
 * normalises to 0..255, and delivers the result to the message thread.
 *
 * Results are cached by file-content hash via TrackDataCache::saveBands /
 * loadBands so repeat loads are instantaneous.
 */
class WaveformBandAnalyzer {
public:
	/// Output frame rate. ~50 ms per frame keeps detail without bloating storage.
	static constexpr double kSecondsPerFrame = 0.05;

	/// Hard cap on frame count so very long tracks can't blow memory.
	static constexpr int kMaxFrames = 12000;

	/**
	 * Look the result up in the cache; if missing, schedule a background
	 * analysis. The callback is invoked on the message thread once data is
	 * ready (either from cache or a freshly-completed analysis).
	 *
	 * @param file     Audio file to analyse
	 * @param fileHash Content hash for cache keying (may be empty to skip caching)
	 * @param formatManager Audio format manager (used to create a reader)
	 * @param onReady  Invoked on the message thread; receives the band data
	 *                 (never null — empty vector on failure).
	 */
	static void analyzeAsync(juce::File file,
	                         juce::String fileHash,
	                         juce::AudioFormatManager& formatManager,
	                         std::function<void(BandDataPtr)> onReady);

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformBandAnalyzer)
};
