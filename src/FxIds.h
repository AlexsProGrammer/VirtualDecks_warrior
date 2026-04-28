
#pragma once
#include <JuceHeader.h>

/**
 * Identifiers for the FX engine.
 *
 * Three FX categories live in three slot positions inside FxChain.
 * Each category exposes a fixed set of FxId values; FxFactory maps
 * an id to a concrete FxProcessor instance.
 */
enum class FxCategory : int
{
	Pad     = 0,
	Beat    = 1,
	Release = 2,
	Count   = 3
};

/**
 * Stable identifiers for every effect across all categories.
 * Order does not matter; values are persisted in the FX settings file
 * and indexed by FxFactory's per-category arrays.
 */
enum class FxId : int
{
	None = 0,

	// Pad FX
	PadRoll,
	PadSweep,
	PadFlanger,
	PadVinylBrake,
	PadEcho,
	PadReverb,
	PadRecordEcho,

	// Beat FX
	BeatDelay,
	BeatEcho,
	BeatSpiral,
	BeatReverb,
	BeatTrans,
	BeatFilter,
	BeatFlanger,
	BeatPhaser,
	BeatSlipLoop,
	BeatRoll,
	BeatPitch,
	BeatLowCutEcho,
	BeatHelix,
	BeatMobiusSaw,
	BeatMobiusTri,

	// Release FX
	ReleaseVinylBrake,
	ReleaseRecordEcho,
	ReleaseBackSpin
};
