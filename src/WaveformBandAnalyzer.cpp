#include "WaveformBandAnalyzer.h"
#include "TrackDataCache.h"

#include <algorithm>
#include <cmath>

namespace {
	/// Dedicated background pool for offline waveform analysis. 2 workers so the
	/// two decks can be processed in parallel without starving BPM detection.
	juce::ThreadPool& bandAnalyzerPool()
	{
		static juce::ThreadPool pool { 2 };
		return pool;
	}

	/// Crossover frequencies (Hz). Match DJAudioPlayer EQ-band centres.
	constexpr double kLowCutoffHz  = 500.0;
	constexpr double kHighCutoffHz = 5000.0;

	/// Block size for streaming reads.
	constexpr int kReadBlockSize = 4096;

	/**
	 * Run a freshly-built BandData for `file` on the calling thread.
	 * Returns an empty vector on failure.
	 */
	BandData computeBands(const juce::File& file, juce::AudioFormatManager& formatManager)
	{
		BandData out;

		std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
		if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
			return out;

		const double sr = reader->sampleRate;
		const juce::int64 totalSamples = reader->lengthInSamples;
		const int numChannels = juce::jmin<int>((int)reader->numChannels, 2);

		// Output frame size = N audio samples per output frame.
		const int samplesPerFrame = juce::jmax(1, (int)std::round(sr * WaveformBandAnalyzer::kSecondsPerFrame));

		// Estimate frame count and clamp to the hard cap.
		juce::int64 estFrames = (totalSamples + samplesPerFrame - 1) / samplesPerFrame;
		int frameCount = (estFrames > (juce::int64)WaveformBandAnalyzer::kMaxFrames)
			? WaveformBandAnalyzer::kMaxFrames : (int)estFrames;
		if (frameCount <= 0)
			return out;

		// If we hit the cap, scale samplesPerFrame up so the whole track fits.
		const int effectiveSamplesPerFrame = (estFrames > WaveformBandAnalyzer::kMaxFrames)
			? (int)((totalSamples + frameCount - 1) / frameCount)
			: samplesPerFrame;

		// One IIR filter per channel per band (4 chains total since mid uses HP+LP).
		// Mid band = high-pass at low cutoff, then low-pass at high cutoff (band-pass).
		auto lowCoeffs    = juce::IIRCoefficients::makeLowPass (sr, kLowCutoffHz);
		auto highCoeffs   = juce::IIRCoefficients::makeHighPass(sr, kHighCutoffHz);
		auto midHpCoeffs  = juce::IIRCoefficients::makeHighPass(sr, kLowCutoffHz);
		auto midLpCoeffs  = juce::IIRCoefficients::makeLowPass (sr, kHighCutoffHz);

		struct ChannelFilters {
			juce::IIRFilter low;
			juce::IIRFilter high;
			juce::IIRFilter midHp;
			juce::IIRFilter midLp;
		};
		std::array<ChannelFilters, 2> filters;
		for (int c = 0; c < numChannels; ++c)
		{
			filters[c].low  .setCoefficients(lowCoeffs);
			filters[c].high .setCoefficients(highCoeffs);
			filters[c].midHp.setCoefficients(midHpCoeffs);
			filters[c].midLp.setCoefficients(midLpCoeffs);
		}

		// Working buffers - one for the source read, three for filtered output per band.
		juce::AudioBuffer<float> sourceBuf(numChannels, kReadBlockSize);
		juce::AudioBuffer<float> lowBuf   (numChannels, kReadBlockSize);
		juce::AudioBuffer<float> highBuf  (numChannels, kReadBlockSize);
		juce::AudioBuffer<float> midBuf   (numChannels, kReadBlockSize);

		out.reserve((size_t)frameCount);

		// Per-frame energy accumulators.
		double accLow = 0.0, accMid = 0.0, accHigh = 0.0;
		float  accPeak = 0.0f;
		int    accSamples = 0;

		// Tracking maxes for normalisation pass.
		double maxLow = 1e-9, maxMid = 1e-9, maxHigh = 1e-9;
		float  maxPeak = 1e-6f;

		// Scratch per-frame RMS values stored as raw floats; we normalise after the read pass.
		struct RawFrame { double low, mid, high; float amp; };
		std::vector<RawFrame> raw;
		raw.reserve((size_t)frameCount);

		juce::int64 readPos = 0;
		while (readPos < totalSamples)
		{
			const juce::int64 remaining = totalSamples - readPos;
			const int toRead = (remaining > (juce::int64)kReadBlockSize) ? kReadBlockSize : (int)remaining;
			sourceBuf.clear();
			if (! reader->read(&sourceBuf, 0, toRead, readPos, true, numChannels > 1))
				break;

			// Copy into per-band buffers, then filter in place.
			for (int c = 0; c < numChannels; ++c)
			{
				lowBuf .copyFrom(c, 0, sourceBuf, c, 0, toRead);
				highBuf.copyFrom(c, 0, sourceBuf, c, 0, toRead);
				midBuf .copyFrom(c, 0, sourceBuf, c, 0, toRead);

				filters[c].low .processSamples(lowBuf .getWritePointer(c), toRead);
				filters[c].high.processSamples(highBuf.getWritePointer(c), toRead);
				// Mid: HP then LP cascade.
				filters[c].midHp.processSamples(midBuf.getWritePointer(c), toRead);
				filters[c].midLp.processSamples(midBuf.getWritePointer(c), toRead);
			}

			// Accumulate per-sample.
			for (int i = 0; i < toRead; ++i)
			{
				double sLow = 0.0, sMid = 0.0, sHigh = 0.0;
				float  sPeak = 0.0f;
				for (int c = 0; c < numChannels; ++c)
				{
					const float lv = lowBuf .getSample(c, i);
					const float mv = midBuf .getSample(c, i);
					const float hv = highBuf.getSample(c, i);
					const float rv = sourceBuf.getSample(c, i);
					sLow  += (double)lv * lv;
					sMid  += (double)mv * mv;
					sHigh += (double)hv * hv;
					sPeak  = juce::jmax(sPeak, std::abs(rv));
				}
				accLow  += sLow;
				accMid  += sMid;
				accHigh += sHigh;
				accPeak  = juce::jmax(accPeak, sPeak);
				++accSamples;

				if (accSamples >= effectiveSamplesPerFrame && (int)raw.size() < frameCount)
				{
					const double inv = 1.0 / juce::jmax(1, accSamples * numChannels);
					RawFrame rf;
					rf.low  = std::sqrt(accLow  * inv);
					rf.mid  = std::sqrt(accMid  * inv);
					rf.high = std::sqrt(accHigh * inv);
					rf.amp  = accPeak;
					raw.push_back(rf);

					maxLow  = juce::jmax(maxLow,  rf.low);
					maxMid  = juce::jmax(maxMid,  rf.mid);
					maxHigh = juce::jmax(maxHigh, rf.high);
					maxPeak = juce::jmax(maxPeak, rf.amp);

					accLow = accMid = accHigh = 0.0;
					accPeak = 0.0f;
					accSamples = 0;
				}
			}

			readPos += toRead;
		}

		// Tail: flush any remaining partial frame.
		if (accSamples > 0 && (int)raw.size() < frameCount)
		{
			const double inv = 1.0 / juce::jmax(1, accSamples * numChannels);
			RawFrame rf;
			rf.low  = std::sqrt(accLow  * inv);
			rf.mid  = std::sqrt(accMid  * inv);
			rf.high = std::sqrt(accHigh * inv);
			rf.amp  = accPeak;
			raw.push_back(rf);
			maxLow  = juce::jmax(maxLow,  rf.low);
			maxMid  = juce::jmax(maxMid,  rf.mid);
			maxHigh = juce::jmax(maxHigh, rf.high);
			maxPeak = juce::jmax(maxPeak, rf.amp);
		}

		// Normalise. Per-band normalisation keeps mid/high visible even when
		// bass dominates; gamma 0.5 (sqrt) lifts quieter sections so the
		// waveform stays readable across loudness ranges.
		const double invLow  = 1.0 / maxLow;
		const double invMid  = 1.0 / maxMid;
		const double invHigh = 1.0 / maxHigh;
		const float  invAmp  = 1.0f / maxPeak;

		out.resize(raw.size());
		for (size_t i = 0; i < raw.size(); ++i)
		{
			auto gammaByte = [](double x) -> juce::uint8 {
				const double g = std::sqrt(juce::jlimit(0.0, 1.0, x));
				return (juce::uint8)juce::jlimit(0, 255, (int)std::round(g * 255.0));
			};
			BandFrame f;
			f.low  = gammaByte(raw[i].low  * invLow);
			f.mid  = gammaByte(raw[i].mid  * invMid);
			f.high = gammaByte(raw[i].high * invHigh);
			f.amp  = (juce::uint8)juce::jlimit(0, 255,
			          (int)std::round(std::sqrt(juce::jlimit(0.0f, 1.0f, raw[i].amp * invAmp)) * 255.0));
			out[i] = f;
		}

		return out;
	}
}

//==============================================================================

void WaveformBandAnalyzer::analyzeAsync(juce::File file,
                                        juce::String fileHash,
                                        juce::AudioFormatManager& formatManager,
                                        std::function<void(BandDataPtr)> onReady)
{
	if (! onReady)
		return;

	auto deliver = [cb = std::move(onReady)](BandDataPtr data) mutable {
		juce::MessageManager::callAsync([cb = std::move(cb), data = std::move(data)]() mutable {
			cb(std::move(data));
		});
	};

	bandAnalyzerPool().addJob([file, fileHash, &formatManager, deliver = std::move(deliver)]() mutable {
		// 1. Cache hit?
		if (fileHash.isNotEmpty())
		{
			if (auto cached = TrackDataCache::loadBands(fileHash))
			{
				deliver(std::move(cached));
				return;
			}
		}

		// 2. Compute fresh.
		BandData computed = computeBands(file, formatManager);
		auto shared = std::make_shared<const BandData>(std::move(computed));

		// 3. Persist to cache for next time (skip if hash unknown or empty result).
		if (fileHash.isNotEmpty() && ! shared->empty())
			TrackDataCache::saveBands(fileHash, *shared);

		deliver(std::move(shared));
	});
}
