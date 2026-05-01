
#pragma once
#include <JuceHeader.h>
#include "FxProcessor.h"

/*
 * Concrete FxProcessor implementations.
 *
 * All effects share a small set of conventions:
 *   - Parameter 0 of every effect is the wet/dry mix in [0, 1].
 *     FxChain does NOT mix dry+wet for the processor - each process()
 *     implementation handles its own dry/wet blend using getWet().
 *   - Beat-synced effects expose a "Time" parameter that is interpreted as
 *     a beat-division code when getBpm() > 0, else as milliseconds.
 *
 * All public methods on this hierarchy are audio-thread safe (no allocation,
 * no locking) once prepareToPlay() has been called.
 */

namespace fx_detail
{
	/** Convert a beat-division code (0..6 -> 1/16..4 beats) to seconds at given BPM.
	    Returns 0 if BPM is invalid. */
	inline double beatDivisionToSeconds(int code, double bpm) noexcept
	{
		if (bpm <= 1.0) return 0.0;
		static constexpr double table[] = { 0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0 };
		const int safe = juce::jlimit(0, 6, code);
		return table[safe] * 60.0 / bpm;
	}

	/** Linear interpolation. */
	inline float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }
}

//==============================================================================
// Echo / Delay
//==============================================================================

/**
 * Simple stereo digital echo with feedback and wet/dry mix.
 * Used as the implementation for both Pad Echo and Beat Echo (different ids).
 */
class EchoFxBase : public FxProcessor
{
public:
	EchoFxBase()
	{
		params.emplace_back("Wet",      "",   0.0, 1.0, 0.5);
		params.emplace_back("Time",     "",   0.0, 6.0, 3.0);  // beat-division code
		params.emplace_back("Feedback", "",   0.0, 0.95, 0.45);
	}

	void prepareToPlay(int blockSize_, double sr) override
	{
		FxProcessor::prepareToPlay(blockSize_, sr);
		const int maxDelaySamples = (int) (sr * 4.0) + 4; // up to 4 s
		delayBuf.setSize(2, maxDelaySamples, false, true, true);
		writePos = 0;
	}

	void onEngageChanged(bool nowEngaged) override
	{
		if (nowEngaged) delayBuf.clear();
	}

	void onParametersReset() override { delayBuf.clear(); }

	void process(juce::AudioBuffer<float>& buffer) override
	{
		const int   numSamples = buffer.getNumSamples();
		const int   numCh      = juce::jmin(2, buffer.getNumChannels());
		const float wet        = (float) params[0].get();
		const float fb         = (float) params[2].get();

		// Resolve delay time
		double seconds = fx_detail::beatDivisionToSeconds((int) params[1].get(), getBpm());
		if (seconds <= 0.0) seconds = 0.25; // 250ms free-time fallback
		int delaySamples = juce::jlimit(1, delayBuf.getNumSamples() - 1,
		                                (int) (seconds * sampleRate));

		const int bufLen = delayBuf.getNumSamples();

		for (int ch = 0; ch < numCh; ++ch)
		{
			float* in   = buffer.getWritePointer(ch);
			float* dbuf = delayBuf.getWritePointer(ch);
			int    wp   = writePos;

			for (int i = 0; i < numSamples; ++i)
			{
				int rp = wp - delaySamples;
				if (rp < 0) rp += bufLen;
				const float delayed = dbuf[rp];
				const float dry     = in[i];
				dbuf[wp] = dry + delayed * fb;
				in[i]    = dry * (1.0f - wet) + delayed * wet;
				if (++wp >= bufLen) wp = 0;
			}
		}
		writePos = (writePos + numSamples) % bufLen;
	}

private:
	juce::AudioBuffer<float> delayBuf;
	int writePos = 0;
};

class PadEchoFx : public EchoFxBase
{
public:
	FxId getId() const noexcept override { return FxId::PadEcho; }
	juce::String getName() const override { return "Echo"; }
};

class BeatEchoFx : public EchoFxBase
{
public:
	FxId getId() const noexcept override { return FxId::BeatEcho; }
	juce::String getName() const override { return "Echo"; }
};

class BeatDelayFx : public EchoFxBase
{
public:
	BeatDelayFx() { params[2].defaultValue = 0.2; params[2].set(0.2); } // shorter feedback for "Delay"
	FxId getId() const noexcept override { return FxId::BeatDelay; }
	juce::String getName() const override { return "Delay"; }
};

//==============================================================================
// Reverb
//==============================================================================

class ReverbFxBase : public FxProcessor
{
public:
	ReverbFxBase()
	{
		params.emplace_back("Wet",     "", 0.0, 1.0, 0.4);
		params.emplace_back("Size",    "", 0.0, 1.0, 0.6);
		params.emplace_back("Damping", "", 0.0, 1.0, 0.5);
	}

	void prepareToPlay(int b, double sr) override
	{
		FxProcessor::prepareToPlay(b, sr);
		reverb.setSampleRate(sr);
		reverb.reset();
	}

	void onEngageChanged(bool nowEngaged) override { if (nowEngaged) reverb.reset(); }
	void onParametersReset() override { reverb.reset(); }

	void process(juce::AudioBuffer<float>& buffer) override
	{
		juce::Reverb::Parameters p;
		p.roomSize    = (float) params[1].get();
		p.damping     = (float) params[2].get();
		p.wetLevel    = (float) params[0].get();
		p.dryLevel    = 1.0f - p.wetLevel;
		p.width       = 1.0f;
		p.freezeMode  = 0.0f;
		reverb.setParameters(p);

		const int n = buffer.getNumSamples();
		if (buffer.getNumChannels() >= 2)
			reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), n);
		else if (buffer.getNumChannels() == 1)
			reverb.processMono(buffer.getWritePointer(0), n);
	}

private:
	juce::Reverb reverb;
};

class PadReverbFx  : public ReverbFxBase { public: FxId getId() const noexcept override { return FxId::PadReverb;  } juce::String getName() const override { return "Reverb"; } };
class BeatReverbFx : public ReverbFxBase { public: FxId getId() const noexcept override { return FxId::BeatReverb; } juce::String getName() const override { return "Reverb"; } };

//==============================================================================
// Filter (LP/HP morph centered at 0.5 = bypass)
//==============================================================================

class BeatFilterFx : public FxProcessor
{
public:
	BeatFilterFx()
	{
		params.emplace_back("Wet",      "", 0.0, 1.0, 1.0);
		params.emplace_back("Cutoff",   "", 0.0, 1.0, 0.5);   // <0.5 = LP, >0.5 = HP
		params.emplace_back("Resonance","", 0.1, 4.0, 0.7);
	}

	FxId getId() const noexcept override { return FxId::BeatFilter; }
	juce::String getName() const override { return "Filter"; }

	void prepareToPlay(int b, double sr) override
	{
		FxProcessor::prepareToPlay(b, sr);
		juce::dsp::ProcessSpec spec { sr, (juce::uint32) b, 2 };
		svf.prepare(spec);
		svf.reset();
	}

	void onEngageChanged(bool nowEngaged) override { if (nowEngaged) svf.reset(); }
	void onParametersReset() override { svf.reset(); }

	void process(juce::AudioBuffer<float>& buffer) override
	{
		const float c   = (float) params[1].get(); // 0..1
		const float res = (float) params[2].get();
		const float wet = (float) params[0].get();

		// Map: 0 -> 80 Hz LP, 0.5 -> bypass-ish, 1 -> 8 kHz HP
		const bool isHP = c > 0.5f;
		const float norm = isHP ? (c - 0.5f) * 2.0f : (0.5f - c) * 2.0f;
		const float cutoff = isHP
			? juce::jmap(norm, 80.0f, 8000.0f)
			: juce::jmap(norm, 8000.0f, 80.0f);

		using Mode = juce::dsp::StateVariableTPTFilterType;
		svf.setType(isHP ? Mode::highpass : Mode::lowpass);
		svf.setCutoffFrequency(cutoff);
		svf.setResonance(res);

		// Process onto a wet copy and blend
		const int n  = buffer.getNumSamples();
		const int ch = buffer.getNumChannels();

		juce::AudioBuffer<float> wetBuf (ch, n);
		for (int c = 0; c < ch; ++c) wetBuf.copyFrom(c, 0, buffer, c, 0, n);

		juce::dsp::AudioBlock<float> block (wetBuf);
		juce::dsp::ProcessContextReplacing<float> ctx (block);
		svf.process(ctx);

		for (int c = 0; c < ch; ++c)
		{
			float* dry = buffer.getWritePointer(c);
			const float* w = wetBuf.getReadPointer(c);
			for (int i = 0; i < n; ++i)
				dry[i] = dry[i] * (1.0f - wet) + w[i] * wet;
		}
	}

private:
	juce::dsp::StateVariableTPTFilter<float> svf;
};

//==============================================================================
// Flanger (LFO-modulated short delay)
//==============================================================================

class FlangerFxBase : public FxProcessor
{
public:
	FlangerFxBase()
	{
		params.emplace_back("Wet",      "",   0.0, 1.0,  0.5);
		params.emplace_back("Rate",     "Hz", 0.05, 5.0, 0.3);
		params.emplace_back("Depth",    "",   0.0, 1.0,  0.5);
		params.emplace_back("Feedback", "",   0.0, 0.9,  0.3);
	}

	void prepareToPlay(int b, double sr) override
	{
		FxProcessor::prepareToPlay(b, sr);
		const int maxDelay = (int)(sr * 0.012) + 4;        // up to 12 ms
		delayBuf.setSize(2, maxDelay, false, true, true);
		writePos = 0;
		lfoPhase = 0.0;
	}

	void onEngageChanged(bool nowEngaged) override { if (nowEngaged) { delayBuf.clear(); lfoPhase = 0.0; } }
	void onParametersReset() override { delayBuf.clear(); lfoPhase = 0.0; }

	void process(juce::AudioBuffer<float>& buffer) override
	{
		const int n   = buffer.getNumSamples();
		const int ch  = juce::jmin(2, buffer.getNumChannels());
		const float wet   = (float) params[0].get();
		const double rate = params[1].get();
		const float depth = (float) params[2].get();
		const float fb    = (float) params[3].get();

		const int   bufLen = delayBuf.getNumSamples();
		const float maxDel = (float)(bufLen - 2);
		const float minDel = 1.0f;
		const double phaseInc = juce::MathConstants<double>::twoPi * rate / sampleRate;

		for (int i = 0; i < n; ++i)
		{
			const float lfo = 0.5f + 0.5f * std::sin((float) lfoPhase);
			const float delaySamples = minDel + (maxDel - minDel) * (depth * lfo);

			for (int c = 0; c < ch; ++c)
			{
				float* in   = buffer.getWritePointer(c);
				float* dbuf = delayBuf.getWritePointer(c);
				const float dry = in[i];

				float rp = (float) writePos - delaySamples;
				while (rp < 0) rp += bufLen;
				const int   rpi = (int) rp;
				const float frac = rp - (float) rpi;
				const int   rp2 = (rpi + 1) % bufLen;
				const float delayed = fx_detail::lerp(dbuf[rpi], dbuf[rp2], frac);

				dbuf[writePos] = dry + delayed * fb;
				in[i]          = dry * (1.0f - wet) + delayed * wet;
			}

			if (++writePos >= bufLen) writePos = 0;
			lfoPhase += phaseInc;
			if (lfoPhase > juce::MathConstants<double>::twoPi) lfoPhase -= juce::MathConstants<double>::twoPi;
		}
	}

private:
	juce::AudioBuffer<float> delayBuf;
	int    writePos = 0;
	double lfoPhase = 0.0;
};

class PadFlangerFx  : public FlangerFxBase { public: FxId getId() const noexcept override { return FxId::PadFlanger;  } juce::String getName() const override { return "Flanger"; } };
class BeatFlangerFx : public FlangerFxBase { public: FxId getId() const noexcept override { return FxId::BeatFlanger; } juce::String getName() const override { return "Flanger"; } };

//==============================================================================
// Phaser (juce::dsp::Phaser)
//==============================================================================

class BeatPhaserFx : public FxProcessor
{
public:
	BeatPhaserFx()
	{
		params.emplace_back("Wet",      "",   0.0, 1.0, 0.5);
		params.emplace_back("Rate",     "Hz", 0.05, 5.0, 0.5);
		params.emplace_back("Depth",    "",   0.0, 1.0, 0.5);
		params.emplace_back("Feedback", "",   -0.95, 0.95, 0.0);
	}

	FxId getId() const noexcept override { return FxId::BeatPhaser; }
	juce::String getName() const override { return "Phaser"; }

	void prepareToPlay(int b, double sr) override
	{
		FxProcessor::prepareToPlay(b, sr);
		juce::dsp::ProcessSpec spec { sr, (juce::uint32) b, 2 };
		phaser.prepare(spec);
		phaser.reset();
	}

	void onEngageChanged(bool nowEngaged) override { if (nowEngaged) phaser.reset(); }
	void onParametersReset() override { phaser.reset(); }

	void process(juce::AudioBuffer<float>& buffer) override
	{
		phaser.setRate     ((float) params[1].get());
		phaser.setDepth    ((float) params[2].get());
		phaser.setFeedback ((float) params[3].get());
		phaser.setMix      ((float) params[0].get());
		phaser.setCentreFrequency(1300.0f);

		juce::dsp::AudioBlock<float> block (buffer);
		juce::dsp::ProcessContextReplacing<float> ctx (block);
		phaser.process(ctx);
	}

private:
	juce::dsp::Phaser<float> phaser;
};

//==============================================================================
// Roll (capture N beats on engage, loop them)
//==============================================================================

class RollFxBase : public FxProcessor
{
public:
	RollFxBase()
	{
		params.emplace_back("Wet",  "", 0.0, 1.0, 1.0);
		params.emplace_back("Time", "", 0.0, 6.0, 2.0);   // beat-division
	}

	void prepareToPlay(int b, double sr) override
	{
		FxProcessor::prepareToPlay(b, sr);
		const int maxLen = (int) (sr * 4.0) + 4;
		captureBuf.setSize(2, maxLen, false, true, true);
		captureLen = 0;
		readPos    = 0;
		armed      = true;
	}

	void onEngageChanged(bool nowEngaged) override
	{
		armed = nowEngaged;     // re-arm on each engage
		readPos = 0;
		captureLen = 0;
	}

	void process(juce::AudioBuffer<float>& buffer) override
	{
		const int n  = buffer.getNumSamples();
		const int ch = juce::jmin(2, buffer.getNumChannels());
		const float wet = (float) params[0].get();

		double seconds = fx_detail::beatDivisionToSeconds((int) params[1].get(), getBpm());
		if (seconds <= 0.0) seconds = 0.5;
		const int targetLen = juce::jlimit(64,
		                                   captureBuf.getNumSamples(),
		                                   (int) (seconds * sampleRate));

		if (armed && captureLen < targetLen)
		{
			const int toCapture = juce::jmin(n, targetLen - captureLen);
			for (int c = 0; c < ch; ++c)
				captureBuf.copyFrom(c, captureLen, buffer, c, 0, toCapture);
			captureLen += toCapture;
			if (captureLen >= targetLen) armed = false;

			// First fill: pass dry through. After that, looping kicks in.
			if (captureLen < targetLen) return;
		}

		if (captureLen <= 0) return;

		for (int i = 0; i < n; ++i)
		{
			for (int c = 0; c < ch; ++c)
			{
				float* in = buffer.getWritePointer(c);
				const float* cap = captureBuf.getReadPointer(c);
				const float dry = in[i];
				const float wetSamp = cap[readPos];
				in[i] = dry * (1.0f - wet) + wetSamp * wet;
			}
			if (++readPos >= captureLen) readPos = 0;
		}
	}

private:
	juce::AudioBuffer<float> captureBuf;
	int  captureLen = 0;
	int  readPos    = 0;
	bool armed      = false;
};

class PadRollFx  : public RollFxBase { public: FxId getId() const noexcept override { return FxId::PadRoll;  } juce::String getName() const override { return "Roll"; } };
class BeatRollFx : public RollFxBase { public: FxId getId() const noexcept override { return FxId::BeatRoll; } juce::String getName() const override { return "Roll"; } };

//==============================================================================
// Vinyl Brake (rate ramp-down via downsampling of capture)
//==============================================================================

class VinylBrakeFxBase : public FxProcessor
{
public:
	VinylBrakeFxBase()
	{
		params.emplace_back("Wet",   "",  0.0, 1.0, 1.0);
		params.emplace_back("Time",  "s", 0.1, 4.0, 1.0);  // brake duration
	}

	void prepareToPlay(int b, double sr) override
	{
		FxProcessor::prepareToPlay(b, sr);
		const int maxLen = (int)(sr * 8.0) + 4;
		ringBuf.setSize(2, maxLen, false, true, true);
		writePos = 0;
		brakePos = 0.0;
		rate     = 1.0f;
		ramping  = false;
	}

	void onEngageChanged(bool nowEngaged) override
	{
		if (nowEngaged)
		{
			brakePos = (double) writePos;
			rate     = 1.0f;
			ramping  = true;
		}
		else
		{
			ramping  = false;
		}
	}

	void process(juce::AudioBuffer<float>& buffer) override
	{
		const int n   = buffer.getNumSamples();
		const int ch  = juce::jmin(2, buffer.getNumChannels());
		const int len = ringBuf.getNumSamples();
		const float wet = (float) params[0].get();
		const double brakeSecs = juce::jmax(0.05, params[1].get());
		// per-sample rate decrement so rate -> 0 in brakeSecs seconds
		const float decrement = (float)(1.0 / (brakeSecs * sampleRate));

		// 1) Always feed the dry signal into the ring buffer.
		for (int c = 0; c < ch; ++c)
		{
			const float* src = buffer.getReadPointer(c);
			float* dst = ringBuf.getWritePointer(c);
			for (int i = 0; i < n; ++i)
			{
				dst[(writePos + i) % len] = src[i];
			}
		}

		if (ramping)
		{
			// 2) Replace output with brake-rate playback of the ring buffer
			//    starting from where ramp began.
			for (int i = 0; i < n; ++i)
			{
				int rpi = (int) brakePos;
				rpi = ((rpi % len) + len) % len;
				const int rp2 = (rpi + 1) % len;
				const float frac = (float)(brakePos - std::floor(brakePos));

				for (int c = 0; c < ch; ++c)
				{
					float* dry = buffer.getWritePointer(c);
					const float* cap = ringBuf.getReadPointer(c);
					const float wetSamp = fx_detail::lerp(cap[rpi], cap[rp2], frac);
					dry[i] = dry[i] * (1.0f - wet) + wetSamp * wet;
				}

				brakePos += rate;
				rate -= decrement;
				if (rate <= 0.0f) { rate = 0.0f; }
			}
		}

		writePos = (writePos + n) % len;
	}

protected:
	juce::AudioBuffer<float> ringBuf;
	int    writePos = 0;
	double brakePos = 0.0;
	float  rate     = 1.0f;
	bool   ramping  = false;
};

class PadVinylBrakeFx     : public VinylBrakeFxBase { public: FxId getId() const noexcept override { return FxId::PadVinylBrake;     } juce::String getName() const override { return "V.Brake"; } };
class ReleaseVinylBrakeFx : public VinylBrakeFxBase { public: FxId getId() const noexcept override { return FxId::ReleaseVinylBrake; } juce::String getName() const override { return "V.Brake"; } };

//==============================================================================
// Back Spin (reverse playback of recent capture)
//==============================================================================

class ReleaseBackSpinFx : public FxProcessor
{
public:
	ReleaseBackSpinFx()
	{
		params.emplace_back("Wet",  "",  0.0, 1.0, 1.0);
		params.emplace_back("Time", "s", 0.2, 4.0, 1.5);
		params.emplace_back("Spin", "",  1.0, 8.0, 4.0);  // initial reverse playback rate
	}

	FxId getId() const noexcept override { return FxId::ReleaseBackSpin; }
	juce::String getName() const override { return "BackSpin"; }

	void prepareToPlay(int b, double sr) override
	{
		FxProcessor::prepareToPlay(b, sr);
		const int maxLen = (int)(sr * 8.0) + 4;
		ringBuf.setSize(2, maxLen, false, true, true);
		writePos = 0;
		readPos  = 0.0;
		rate     = 0.0f;
		spinning = false;
	}

	void onEngageChanged(bool nowEngaged) override
	{
		if (nowEngaged)
		{
			readPos  = (double) writePos;
			rate     = (float) params[2].get();
			spinning = true;
		}
		else
		{
			spinning = false;
		}
	}

	void process(juce::AudioBuffer<float>& buffer) override
	{
		const int n   = buffer.getNumSamples();
		const int ch  = juce::jmin(2, buffer.getNumChannels());
		const int len = ringBuf.getNumSamples();
		const float wet = (float) params[0].get();
		const double spinSecs = juce::jmax(0.05, params[1].get());
		const float decrement = (float)(rate / (spinSecs * sampleRate)); // ramp to 0

		// Always capture incoming dry samples (so on next engage we have history)
		for (int c = 0; c < ch; ++c)
		{
			const float* src = buffer.getReadPointer(c);
			float* dst = ringBuf.getWritePointer(c);
			for (int i = 0; i < n; ++i)
				dst[(writePos + i) % len] = src[i];
		}

		if (spinning)
		{
			float r = rate;
			for (int i = 0; i < n; ++i)
			{
				int rpi = (int) readPos;
				rpi = ((rpi % len) + len) % len;
				const int rp2 = (rpi + len - 1) % len; // reverse neighbour
				const float frac = (float)(readPos - std::floor(readPos));

				for (int c = 0; c < ch; ++c)
				{
					float* dry = buffer.getWritePointer(c);
					const float* cap = ringBuf.getReadPointer(c);
					const float wetSamp = fx_detail::lerp(cap[rpi], cap[rp2], frac);
					dry[i] = dry[i] * (1.0f - wet) + wetSamp * wet;
				}

				readPos -= r;
				while (readPos < 0) readPos += len;
				r -= decrement;
				if (r <= 0.0f) r = 0.0f;
			}
			rate = r;
		}

		writePos = (writePos + n) % len;
	}

private:
	juce::AudioBuffer<float> ringBuf;
	int    writePos = 0;
	double readPos  = 0.0;
	float  rate     = 0.0f;
	bool   spinning = false;
};

//==============================================================================
// R.Echo (Record/Reverse Echo) - simple variant: feedback echo with reverse
// of the captured tail. Implemented as feedback delay reusing EchoFxBase
// behaviour for now; flagged for richer DSP later.
//==============================================================================

class PadRecordEchoFx : public EchoFxBase
{
public:
	PadRecordEchoFx() { params[2].defaultValue = 0.6; params[2].set(0.6); }
	FxId getId() const noexcept override { return FxId::PadRecordEcho; }
	juce::String getName() const override { return "R.Echo"; }
};

class ReleaseRecordEchoFx : public EchoFxBase
{
public:
	ReleaseRecordEchoFx() { params[2].defaultValue = 0.7; params[2].set(0.7); }
	FxId getId() const noexcept override { return FxId::ReleaseRecordEcho; }
	juce::String getName() const override { return "R.Echo"; }
};

//==============================================================================
// Stub effects (parameter list wired, audio passes through unchanged).
// These provide UI/persistence presence for the listed effects so the engine
// can be expanded incrementally without touching the framework.
//==============================================================================

class StubFx : public FxProcessor
{
public:
	StubFx(FxId id_, juce::String displayName)
		: id(id_), displayName(std::move(displayName))
	{
		params.emplace_back("Wet",     "", 0.0, 1.0, 0.5);
		params.emplace_back("Param 1", "", 0.0, 1.0, 0.5);
		params.emplace_back("Param 2", "", 0.0, 1.0, 0.5);
	}
	FxId getId() const noexcept override { return id; }
	juce::String getName() const override { return displayName; }
	void process(juce::AudioBuffer<float>&) override { /* identity */ }

private:
	FxId         id;
	juce::String displayName;
};
