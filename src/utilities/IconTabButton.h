#pragma once

#include <JuceHeader.h>
#include <cstring>

/**
 * Vertical-rail tab button: a juce::TextButton that paints an SVG icon on
 * top of its existing TextButton background. Inherits all of TextButton's
 * colour-id highlighting logic (`buttonColourId`, `buttonOnColourId`) so the
 * existing per-tab colour state machine keeps working unchanged.
 *
 * The icon is loaded once at construction from raw BinaryData SVG bytes.
 * Two tints are stored - a translucent "inactive" tint for unselected tabs
 * and an opaque white "active" tint for the currently selected tab - so
 * paint() can pick between them based on getToggleState().
 */
class IconTabButton : public juce::TextButton {
public:
	/**
	 * @param tooltip    Hover-tooltip text (also passed through to TextButton).
	 * @param svgData    Pointer to BinaryData SVG bytes (may be nullptr → text fallback).
	 * @param svgSize    Length of the SVG byte buffer.
	 */
	IconTabButton(const juce::String& tooltip, const char* svgData, int svgSize)
		: juce::TextButton(tooltip)
	{
		setTooltip(tooltip);
		setButtonText({}); // icon-only; tooltip provides the label

		if (svgData != nullptr && svgSize > 0)
		{
			iconInactive = juce::Drawable::createFromImageData(svgData, (size_t)svgSize);
			iconActive   = juce::Drawable::createFromImageData(svgData, (size_t)svgSize);

			// SVGs ship as white; replaceColour swaps for the desired tint.
			if (iconInactive)
				iconInactive->replaceColour(juce::Colours::white, juce::Colour::fromRGB(180, 180, 180));
			// Active stays white - it pops against the theme-coloured background.
		}
	}

	/**
	 * Implementation of paintButton override.
	 *
	 * Draws the parent TextButton background first (preserves all existing
	 * colour-state behaviour) then overlays the SVG icon centred and inset.
	 */
	void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
	{
		juce::TextButton::paintButton(g, isOver, isDown);

		auto* d = getToggleState() ? iconActive.get() : iconInactive.get();
		if (d != nullptr)
		{
			auto area = getLocalBounds().reduced(6).toFloat();
			d->drawWithin(g, area, juce::RectanglePlacement::centred, 1.0f);
		}
	}

private:
	std::unique_ptr<juce::Drawable> iconInactive;
	std::unique_ptr<juce::Drawable> iconActive;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconTabButton);
};
