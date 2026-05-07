#pragma once

#include <JuceHeader.h>

/**
 * Result of a BPM analysis run.
 */
struct BpmResult {
	/// Detected BPM (0.0 if detection failed)
	double bpm = 0.0;

	/// Confidence of the detection (0.0 to 1.0)
	double confidence = 0.0;
};

/**
 * Multi-algorithm BPM detector.
 *
 * Runs three complementary algorithms on multiple random segments of a track,
 * then uses clustering consensus to determine the most likely BPM.
 *
 * Algorithm 1: Bass energy onset + IOI histogram (kick-drum focused)
 * Algorithm 2: Autocorrelation of onset envelope (rhythmic pattern)
 * Algorithm 3: Differential rectified energy autocorrelation (full-band)
 */
class BpmDetector {
public:
	/**
	 * Analyse a full track and return the consensus BPM.
	 *
	 * Picks multiple random segments, runs all three algorithms on each,
	 * and clusters the results to find the dominant tempo.
	 *
	 * @param reader AudioFormatReader for the track (caller owns it)
	 * @return BpmResult with detected BPM and confidence
	 */
	static BpmResult analyze(juce::AudioFormatReader* reader);

	/**
	 * Read BPM from audio file metadata via TagLib.
	 *
	 * @param filePath Absolute path to the audio file
	 * @return BPM value, or 0.0 if not found in metadata
	 */
	static double readBpmFromMetadata(const juce::String& filePath);

private:
	/**
	 * Bass energy onset detection with IOI histogram.
	 * Bandpass-filters to 60-200 Hz, detects onsets, builds IOI histogram.
	 *
	 * @param samples Mono audio buffer
	 * @param numSamples Number of samples
	 * @param sampleRate Sample rate in Hz
	 * @return Detected BPM or 0.0
	 */
	static double detectBassOnset(const float* samples, int numSamples, double sampleRate);

	/**
	 * Autocorrelation of onset envelope.
	 * Computes RMS envelope then autocorrelates for tempo-range lags.
	 *
	 * @param samples Mono audio buffer
	 * @param numSamples Number of samples
	 * @param sampleRate Sample rate in Hz
	 * @return Detected BPM or 0.0
	 */
	static double detectAutocorrelation(const float* samples, int numSamples, double sampleRate);

	/**
	 * Differential rectified energy with autocorrelation.
	 * Full-band energy differences, half-wave rectified, then autocorrelated.
	 *
	 * @param samples Mono audio buffer
	 * @param numSamples Number of samples
	 * @param sampleRate Sample rate in Hz
	 * @return Detected BPM or 0.0
	 */
	static double detectDifferentialEnergy(const float* samples, int numSamples, double sampleRate);

	/**
	 * Correct a BPM value into the 70-170 range by octave doubling/halving.
	 */
	static double octaveCorrect(double bpm);
};
