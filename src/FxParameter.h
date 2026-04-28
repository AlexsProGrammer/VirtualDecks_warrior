
#pragma once
#include <JuceHeader.h>
#include <atomic>

/**
 * Parameter descriptor for an FxProcessor.
 *
 * Defines a single named, ranged parameter. Values are stored as a
 * std::atomic<double> so they can be updated from the UI thread (via the
 * AudioCommand FIFO drain on the audio thread, which writes here) and read
 * lock-free by the audio-thread process() implementation.
 */
struct FxParameter
{
	juce::String     name;          ///< UI label, e.g. "Time", "Feedback"
	juce::String     unit;          ///< UI suffix, e.g. "ms", "%", "" (none)
	double           minValue = 0.0;
	double           maxValue = 1.0;
	double           defaultValue = 0.0;
	std::atomic<double> value { 0.0 };

	FxParameter() = default;

	FxParameter(juce::String n, juce::String u, double mn, double mx, double dv)
		: name(std::move(n)), unit(std::move(u)),
		  minValue(mn), maxValue(mx), defaultValue(dv), value(dv) {}

	// std::atomic<double> is non-copyable; provide explicit copy/move.
	FxParameter(const FxParameter& o)
		: name(o.name), unit(o.unit),
		  minValue(o.minValue), maxValue(o.maxValue),
		  defaultValue(o.defaultValue), value(o.value.load()) {}

	FxParameter& operator=(const FxParameter& o)
	{
		name = o.name; unit = o.unit;
		minValue = o.minValue; maxValue = o.maxValue;
		defaultValue = o.defaultValue;
		value.store(o.value.load());
		return *this;
	}

	double get() const noexcept { return value.load(std::memory_order_relaxed); }
	void   set(double v) noexcept
	{
		v = juce::jlimit(minValue, maxValue, v);
		value.store(v, std::memory_order_relaxed);
	}
	void resetToDefault() noexcept { value.store(defaultValue, std::memory_order_relaxed); }
};
