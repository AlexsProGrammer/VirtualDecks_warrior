
#pragma once
#include <JuceHeader.h>
#include <functional>
#include "FxProcessor.h"

/**
 * Small popup component shown inside a juce::CallOutBox to edit the live
 * parameters of an FxProcessor.
 *
 * The modal binds directly to a single FxProcessor instance (lifetime owned
 * by FxChain, which outlives the modal) and:
 *  - Renders one slider per parameter with name + unit + value
 *  - Exposes a Bypass toggle, a Reset-to-defaults button and a Save button
 *
 * Parameter changes are written straight into FxParameter::value (atomic);
 * the audio thread will pick them up on the next block. The Save callback
 * is supplied by the caller (DeckGUI) and is responsible for persisting the
 * full FX state across both decks.
 */
class FxParameterModal : public juce::Component
{
public:
	using SaveCallback = std::function<void()>;

	FxParameterModal(FxProcessor& processor,
	                 juce::Colour theme,
	                 SaveCallback onSave);

	void paint(juce::Graphics& g) override;
	void resized() override;

	int getPreferredHeight() const noexcept;
	int getPreferredWidth()  const noexcept { return 240; }

private:
	void buildControls();

	FxProcessor& fx;
	juce::Colour themeColour;
	SaveCallback saveCallback;

	juce::Label titleLabel;
	juce::ToggleButton bypassToggle { "Bypass" };
	juce::TextButton   resetButton  { "RESET" };
	juce::TextButton   saveButton   { "SAVE" };

	struct Row
	{
		std::unique_ptr<juce::Label>  label;
		std::unique_ptr<juce::Slider> slider;
		std::unique_ptr<juce::Label>  valueLabel;
	};
	std::vector<Row> rows;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxParameterModal)
};
