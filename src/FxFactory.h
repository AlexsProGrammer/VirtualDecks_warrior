
#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "FxIds.h"
#include "FxProcessor.h"

/**
 * Factory + registry mapping FxCategory -> ordered list of FxProcessor
 * instances exposed in that category.
 *
 * The registry is built once on the message thread (DJAudioPlayer ctor)
 * and ownership of all processors moves into FxChain. After construction
 * the audio thread only needs an FxId -> index lookup.
 */
class FxFactory
{
public:
	using Owned = std::unique_ptr<FxProcessor>;

	/// Build all processors for the given category in display order.
	/// The first entry (index 0) is always the "None" passthrough stub.
	static std::vector<Owned> buildCategory(FxCategory cat);

	/// Display label for the category (used in UI).
	static juce::String categoryLabel(FxCategory cat) noexcept
	{
		switch (cat)
		{
			case FxCategory::Pad:     return "P.FX";
			case FxCategory::Beat:    return "B.FX";
			case FxCategory::Release: return "R.FX";
			default:                  return "FX";
		}
	}
};
