
#include <JuceHeader.h>

#pragma once

#include "../../utilities/UIConstants.h"

/**
 * Custom LookAndFeel for the DJDecks app.
 *
 * Provides a flat, modern look across the entire application: rounded buttons
 * with glow on toggle, pill-shaped sliders with circular thumbs, code-drawn
 * rotary knobs with a filled-arc indicator, and rounded popup menus / combo
 * boxes / scrollbars. Theme colours and corner radii come from `UIConstants.h`.
 */
class CustomLookAndFeel : public juce::LookAndFeel_V4 {
public:

	/** Configures default colour IDs across all standard JUCE components. */
	CustomLookAndFeel();

	//==============================================================================
	// Sliders
	void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
		float sliderPos, float minSliderPos, float maxSliderPos,
		const juce::Slider::SliderStyle, juce::Slider&) override;

	void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
		float sliderPos, const float rotaryStartAngle,
		const float rotaryEndAngle, juce::Slider& slider) override;

	int getSliderThumbRadius(juce::Slider&) override;

	//==============================================================================
	// Tables
	void drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header) override;
	void drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent& header,
		const juce::String& columnName, int columnId, int width, int height,
		bool isMouseOver, bool isMouseDown, int columnFlags) override;

	//==============================================================================
	// Buttons
	void drawButtonText(juce::Graphics& g, juce::TextButton& button,
		bool isMouseOverButton, bool isButtonDown) override;

	void drawButtonBackground(juce::Graphics& g, juce::Button& button,
		const juce::Colour& backgroundColour,
		bool isMouseOverButton, bool isButtonDown) override;

	/// Overrides LookAndFeel_V2::drawDrawableButton to suppress the default
	/// fillAll() background rectangle for ImageFitted DrawableButtons.
	void drawDrawableButton(juce::Graphics& g, juce::DrawableButton& button,
		bool isMouseOverButton, bool isButtonDown) override;

	juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
	juce::Font getLabelFont(juce::Label&) override;
	juce::Font getComboBoxFont(juce::ComboBox&) override;
	juce::Font getPopupMenuFont() override;

	//==============================================================================
	// Combo / Popup
	void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
		int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;

	void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

	//==============================================================================
	// Scrollbar
	void drawScrollbar(juce::Graphics& g, juce::ScrollBar& bar, int x, int y,
		int width, int height, bool isScrollbarVertical, int thumbStartPosition,
		int thumbSize, bool isMouseOver, bool isMouseDown) override;

	//==============================================================================
	// TextEditor
	void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& te) override;
	void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& te) override;

	//==============================================================================

	/**
	 * Paint a generic panel background: vertical gradient, rounded rect, optional
	 * drop shadow. Used by deck panels, sidebars, mixer card and the settings panel.
	 */
	static void paintPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds,
		bool elevated = false, float radius = UI::kPanelRadius);

	/** Paint an inner card background (no gradient, lighter than panel). */
	static void paintCardBackground(juce::Graphics& g, juce::Rectangle<float> bounds,
		float radius = UI::kCardRadius);

	/** Centralised SVG → Drawable parser with optional colour replacement. */
	static std::unique_ptr<juce::Drawable> loadIcon(const char* svgData,
		juce::Colour tint = juce::Colours::transparentBlack);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeel)
};

