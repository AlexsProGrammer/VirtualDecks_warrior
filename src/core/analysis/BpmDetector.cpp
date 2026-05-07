#include "BpmDetector.h"
#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

//==============================================================================

/**
 * Correct a BPM into the 70-200 range via octave halving/doubling.
 */
double BpmDetector::octaveCorrect(double bpm)
{
	if (bpm <= 0.0) return 0.0;
	while (bpm > 200.0) bpm *= 0.5;
	while (bpm < 70.0)  bpm *= 2.0;
	return bpm;
}

//==============================================================================
// Algorithm 1: Bass energy onset + IOI histogram
//==============================================================================

double BpmDetector::detectBassOnset(const float* samples, int numSamples, double sampleRate)
{
	if (numSamples < static_cast<int>(sampleRate * 2.0))
		return 0.0;

	// Simple 2nd-order bandpass filter for 60-200 Hz (bass range)
	double centreFreq = 120.0;
	double bandwidth = 140.0; // Hz
	double Q = centreFreq / bandwidth;
	double w0 = 2.0 * juce::MathConstants<double>::pi * centreFreq / sampleRate;
	double alpha = std::sin(w0) / (2.0 * Q);

	double b0 = alpha;
	double b1 = 0.0;
	double b2 = -alpha;
	double a0 = 1.0 + alpha;
	double a1 = -2.0 * std::cos(w0);
	double a2 = 1.0 - alpha;

	// Normalise
	b0 /= a0; b1 /= a0; b2 /= a0;
	a1 /= a0; a2 /= a0;

	// Filter the audio
	std::vector<float> filtered(static_cast<size_t>(numSamples));
	double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	for (int i = 0; i < numSamples; ++i)
	{
		double x = static_cast<double>(samples[i]);
		double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
		filtered[static_cast<size_t>(i)] = static_cast<float>(y);
		x2 = x1; x1 = x; y2 = y1; y1 = y;
	}

	// Compute energy per ~10ms frame
	int frameSize = static_cast<int>(sampleRate * 0.01);
	if (frameSize < 1) frameSize = 1;
	int numFrames = numSamples / frameSize;
	if (numFrames < 10) return 0.0;

	std::vector<double> energy(static_cast<size_t>(numFrames));
	for (int f = 0; f < numFrames; ++f)
	{
		double sum = 0.0;
		int start = f * frameSize;
		for (int s = 0; s < frameSize; ++s)
		{
			double val = static_cast<double>(filtered[static_cast<size_t>(start + s)]);
			sum += val * val;
		}
		energy[static_cast<size_t>(f)] = sum / frameSize;
	}

	// Spectral flux (positive energy differences)
	std::vector<double> flux(static_cast<size_t>(numFrames), 0.0);
	for (int f = 1; f < numFrames; ++f)
	{
		double diff = energy[static_cast<size_t>(f)] - energy[static_cast<size_t>(f - 1)];
		if (diff > 0.0) flux[static_cast<size_t>(f)] = diff;
	}

	// Adaptive threshold: running mean over ~0.5s window, scaled by 1.5x
	int windowSize = static_cast<int>(0.5 / 0.01);
	if (windowSize < 1) windowSize = 1;
	std::vector<double> threshold(static_cast<size_t>(numFrames), 0.0);
	double runningSum = 0.0;
	for (int f = 0; f < numFrames; ++f)
	{
		runningSum += flux[static_cast<size_t>(f)];
		if (f >= windowSize)
			runningSum -= flux[static_cast<size_t>(f - windowSize)];
		int count = std::min(f + 1, windowSize);
		threshold[static_cast<size_t>(f)] = 1.5 * runningSum / count;
	}

	// Peak picking: flux > threshold AND local maximum
	std::vector<double> onsetPositions;
	for (int f = 2; f < numFrames - 1; ++f)
	{
		if (flux[static_cast<size_t>(f)] > threshold[static_cast<size_t>(f)] &&
			flux[static_cast<size_t>(f)] > flux[static_cast<size_t>(f - 1)] &&
			flux[static_cast<size_t>(f)] > flux[static_cast<size_t>(f + 1)])
		{
			onsetPositions.push_back(static_cast<double>(f) * 0.01);
		}
	}

	if (onsetPositions.size() < 4)
		return 0.0;

	// IOI histogram (1ms bins, 200-2000ms range = 30-300 BPM)
	std::vector<int> histogram(1801, 0); // bins for 200ms to 2000ms
	for (size_t i = 1; i < onsetPositions.size(); ++i)
	{
		double ioi = onsetPositions[i] - onsetPositions[i - 1];
		int bin = static_cast<int>(std::round(ioi * 1000.0)) - 200;
		if (bin >= 0 && bin < static_cast<int>(histogram.size()))
			histogram[static_cast<size_t>(bin)]++;
	}

	// Find peak bin
	int peakBin = 0;
	int peakCount = 0;
	for (int i = 0; i < static_cast<int>(histogram.size()); ++i)
	{
		if (histogram[static_cast<size_t>(i)] > peakCount)
		{
			peakCount = histogram[static_cast<size_t>(i)];
			peakBin = i;
		}
	}

	if (peakCount < 2)
		return 0.0;

	double dominantIOI = (static_cast<double>(peakBin) + 200.0) / 1000.0;
	double bpm = 60.0 / dominantIOI;
	return std::round(bpm * 10.0) / 10.0;
}

//==============================================================================
// Algorithm 2: Autocorrelation of onset envelope
//==============================================================================

double BpmDetector::detectAutocorrelation(const float* samples, int numSamples, double sampleRate)
{
	if (numSamples < static_cast<int>(sampleRate * 2.0))
		return 0.0;

	// Compute RMS envelope at ~100 Hz (hop ~= sampleRate/100)
	int hopSize = static_cast<int>(sampleRate / 100.0);
	if (hopSize < 1) hopSize = 1;
	int envLength = numSamples / hopSize;
	if (envLength < 100) return 0.0;

	std::vector<double> envelope(static_cast<size_t>(envLength));
	for (int i = 0; i < envLength; ++i)
	{
		double sum = 0.0;
		int start = i * hopSize;
		int end = std::min(start + hopSize, numSamples);
		for (int s = start; s < end; ++s)
		{
			double val = static_cast<double>(samples[s]);
			sum += val * val;
		}
		envelope[static_cast<size_t>(i)] = std::sqrt(sum / (end - start));
	}

	// Subtract running mean to centre the signal
	double envMean = 0.0;
	for (auto v : envelope) envMean += v;
	envMean /= envelope.size();
	for (auto& v : envelope) v -= envMean;

	// Autocorrelation for lags corresponding to 60-200 BPM
	double envelopeRate = 100.0; // samples per second in envelope domain
	int minLag = static_cast<int>(envelopeRate * 60.0 / 200.0); // 200 BPM
	int maxLag = static_cast<int>(envelopeRate * 60.0 / 60.0);  // 60 BPM

	if (maxLag >= envLength) maxLag = envLength - 1;
	if (minLag < 1) minLag = 1;
	if (minLag >= maxLag) return 0.0;

	std::vector<double> acf(static_cast<size_t>(maxLag - minLag + 1), 0.0);
	for (int lag = minLag; lag <= maxLag; ++lag)
	{
		double sum = 0.0;
		int count = envLength - lag;
		for (int i = 0; i < count; ++i)
			sum += envelope[static_cast<size_t>(i)] * envelope[static_cast<size_t>(i + lag)];
		acf[static_cast<size_t>(lag - minLag)] = sum / count;
	}

	// Find peak
	int peakIdx = 0;
	double peakVal = acf[0];
	for (int i = 1; i < static_cast<int>(acf.size()); ++i)
	{
		if (acf[static_cast<size_t>(i)] > peakVal)
		{
			peakVal = acf[static_cast<size_t>(i)];
			peakIdx = i;
		}
	}

	double peakLag = static_cast<double>(peakIdx + minLag);
	double bpm = 60.0 * envelopeRate / peakLag;
	return std::round(bpm * 10.0) / 10.0;
}

//==============================================================================
// Algorithm 3: Differential rectified energy autocorrelation
//==============================================================================

double BpmDetector::detectDifferentialEnergy(const float* samples, int numSamples, double sampleRate)
{
	if (numSamples < static_cast<int>(sampleRate * 2.0))
		return 0.0;

	// Compute energy per ~10ms frame (full-band)
	int frameSize = static_cast<int>(sampleRate * 0.01);
	if (frameSize < 1) frameSize = 1;
	int numFrames = numSamples / frameSize;
	if (numFrames < 50) return 0.0;

	std::vector<double> energy(static_cast<size_t>(numFrames));
	for (int f = 0; f < numFrames; ++f)
	{
		double sum = 0.0;
		int start = f * frameSize;
		for (int s = 0; s < frameSize; ++s)
		{
			double val = static_cast<double>(samples[start + s]);
			sum += val * val;
		}
		energy[static_cast<size_t>(f)] = sum / frameSize;
	}

	// Half-wave rectified differences
	std::vector<double> diffRect(static_cast<size_t>(numFrames), 0.0);
	for (int f = 1; f < numFrames; ++f)
	{
		double diff = energy[static_cast<size_t>(f)] - energy[static_cast<size_t>(f - 1)];
		if (diff > 0.0) diffRect[static_cast<size_t>(f)] = diff;
	}

	// Autocorrelation for BPM range 60-200
	double frameRate = 1.0 / 0.01; // 100 frames/s
	int minLag = static_cast<int>(frameRate * 60.0 / 200.0);
	int maxLag = static_cast<int>(frameRate * 60.0 / 60.0);

	if (maxLag >= numFrames) maxLag = numFrames - 1;
	if (minLag < 1) minLag = 1;
	if (minLag >= maxLag) return 0.0;

	std::vector<double> acf(static_cast<size_t>(maxLag - minLag + 1), 0.0);
	for (int lag = minLag; lag <= maxLag; ++lag)
	{
		double sum = 0.0;
		int count = numFrames - lag;
		for (int i = 0; i < count; ++i)
			sum += diffRect[static_cast<size_t>(i)] * diffRect[static_cast<size_t>(i + lag)];
		acf[static_cast<size_t>(lag - minLag)] = sum / count;
	}

	// Find peak
	int peakIdx = 0;
	double peakVal = acf[0];
	for (int i = 1; i < static_cast<int>(acf.size()); ++i)
	{
		if (acf[static_cast<size_t>(i)] > peakVal)
		{
			peakVal = acf[static_cast<size_t>(i)];
			peakIdx = i;
		}
	}

	double peakLag = static_cast<double>(peakIdx + minLag);
	double bpm = 60.0 * frameRate / peakLag;
	return std::round(bpm * 10.0) / 10.0;
}

//==============================================================================
// Read BPM from metadata via TagLib
//==============================================================================

double BpmDetector::readBpmFromMetadata(const juce::String& filePath)
{
	TagLib::FileRef fileRef(filePath.toRawUTF8());
	if (fileRef.isNull() || fileRef.tag() == nullptr)
		return 0.0;

	TagLib::PropertyMap props = fileRef.tag()->properties();
	if (props.contains("BPM"))
	{
		TagLib::StringList bpmList = props["BPM"];
		if (!bpmList.isEmpty())
		{
			// Use toDouble via std conversion to preserve fractional BPM
			juce::String bpmStr(bpmList.front().toCString());
			double bpm = bpmStr.getDoubleValue();
			if (bpm > 0.0) return bpm;
		}
	}
	return 0.0;
}

//==============================================================================
// Multi-sample consensus analysis
//==============================================================================

BpmResult BpmDetector::analyze(juce::AudioFormatReader* reader)
{
	BpmResult result;
	if (reader == nullptr)
		return result;

	auto totalSamples = reader->lengthInSamples;
	double sampleRate = reader->sampleRate;
	int numChannels = static_cast<int>(reader->numChannels);

	if (totalSamples < static_cast<juce::int64>(sampleRate * 2.0) || sampleRate <= 0.0)
		return result;

	// Segment parameters
	constexpr int numSegments = 5;
	double segmentDuration = 8.0; // seconds
	int segmentSamples = static_cast<int>(sampleRate * segmentDuration);

	// Avoid first/last 5% of the track
	juce::int64 safeStart = static_cast<juce::int64>(totalSamples * 0.05);
	juce::int64 safeEnd = totalSamples - static_cast<juce::int64>(totalSamples * 0.05) - segmentSamples;

	if (safeEnd <= safeStart)
	{
		// Track too short for multi-segment - use entire track
		safeStart = 0;
		safeEnd = 0;
		segmentSamples = static_cast<int>(totalSamples);
	}

	// Generate random segment positions
	std::mt19937 rng(42); // Fixed seed for reproducibility
	std::uniform_int_distribution<juce::int64> dist(safeStart, std::max(safeStart, safeEnd));

	std::vector<double> allEstimates;
	allEstimates.reserve(numSegments * 3);

	for (int seg = 0; seg < numSegments; ++seg)
	{
		juce::int64 startSample = (safeEnd > safeStart) ? dist(rng) : 0;

		// Read segment into buffer
		juce::AudioBuffer<float> buffer(numChannels, segmentSamples);
		reader->read(&buffer, 0, segmentSamples, startSample, true, true);

		// Mix to mono
		std::vector<float> mono(static_cast<size_t>(segmentSamples), 0.0f);
		for (int ch = 0; ch < numChannels; ++ch)
		{
			const float* channelData = buffer.getReadPointer(ch);
			for (int s = 0; s < segmentSamples; ++s)
				mono[static_cast<size_t>(s)] += channelData[s];
		}
		float scale = 1.0f / static_cast<float>(numChannels);
		for (auto& s : mono) s *= scale;

		// Run all three algorithms
		double bpm1 = detectBassOnset(mono.data(), segmentSamples, sampleRate);
		double bpm2 = detectAutocorrelation(mono.data(), segmentSamples, sampleRate);
		double bpm3 = detectDifferentialEnergy(mono.data(), segmentSamples, sampleRate);

		// Octave-correct all estimates to 70-170 range
		if (bpm1 > 0.0) allEstimates.push_back(octaveCorrect(bpm1));
		if (bpm2 > 0.0) allEstimates.push_back(octaveCorrect(bpm2));
		if (bpm3 > 0.0) allEstimates.push_back(octaveCorrect(bpm3));
	}

	if (allEstimates.empty())
		return result;

	// If only 1-2 estimates, return the median directly
	if (allEstimates.size() < 3)
	{
		std::sort(allEstimates.begin(), allEstimates.end());
		result.bpm = std::round(allEstimates[allEstimates.size() / 2] * 10.0) / 10.0;
		result.confidence = 0.3;
		return result;
	}

	// Cluster estimates: find largest group within ±2 BPM tolerance
	std::sort(allEstimates.begin(), allEstimates.end());

	size_t bestClusterStart = 0;
	size_t bestClusterSize = 0;

	for (size_t i = 0; i < allEstimates.size(); ++i)
	{
		size_t clusterSize = 0;
		for (size_t j = i; j < allEstimates.size(); ++j)
		{
			if (allEstimates[j] - allEstimates[i] <= 4.0) // ±2 BPM window
				clusterSize = j - i + 1;
			else
				break;
		}
		if (clusterSize > bestClusterSize)
		{
			bestClusterSize = clusterSize;
			bestClusterStart = i;
		}
	}

	// If no decent cluster, fall back to median of all estimates
	if (bestClusterSize < 2)
	{
		result.bpm = std::round(allEstimates[allEstimates.size() / 2] * 10.0) / 10.0;
		result.confidence = 0.3;
		return result;
	}

	// Median of the best cluster
	size_t medianIdx = bestClusterStart + bestClusterSize / 2;
	double consensusBpm = allEstimates[medianIdx];

	result.bpm = std::round(consensusBpm * 10.0) / 10.0;
	result.confidence = static_cast<double>(bestClusterSize) / static_cast<double>(allEstimates.size());

	return result;
}
