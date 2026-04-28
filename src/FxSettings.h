
#pragma once
#include <JuceHeader.h>
#include "FxChain.h"

/**
 * Save / load FX engine state to a small XML file at:
 *   ~/.otodecks/FxSettings.xml
 *
 * One <Deck> per deck. Each <Deck> holds three <Slot> nodes (Pad / Beat /
 * Release) with the active processor id and a <Param> list for every
 * processor in that category. The "Save" button in the parameter modal
 * triggers saveAll(); the application boot sequence calls loadInto() once
 * each deck's FxChain has been built and prepared.
 */
namespace FxSettings
{
	juce::File getSettingsFile();

	/// Persist the state of two FxChains (one per deck) to disk.
	bool saveAll(const FxChain& deck0, const FxChain& deck1);

	/// Load persisted state into both chains. Missing entries fall back to
	/// each processor's default values. Returns false if the settings file
	/// is absent or unreadable (still considered a valid first-run).
	bool loadInto(FxChain& deck0, FxChain& deck1);
}
