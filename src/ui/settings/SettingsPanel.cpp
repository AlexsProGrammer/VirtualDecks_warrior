#include <JuceHeader.h>
#include "SettingsPanel.h"
#include "../../core/data/AppSettings.h"
#include "../../utilities/UIConstants.h"
#include "CustomLookAndFeel.h"

SettingsPanel::SettingsPanel(juce::AudioDeviceManager& masterManager,
                             juce::AudioDeviceManager& headphoneManager,
                             std::function<void()>     onClose,
                             std::function<void(float)> masterGainSetter,
                             std::function<void(float)> headphoneGainSetter,
                             std::function<void(bool)>  startAtFirstHotCueSetter,
                             float                      initialMasterGain,
                             float                      initialHeadphoneGain,
                             bool                       initialStartAtFirstHotCue)
	: closeCallback(std::move(onClose)),
	  masterGainCallback(std::move(masterGainSetter)),
	  headphoneGainCallback(std::move(headphoneGainSetter)),
	  headphoneManager(headphoneManager),
	  startAtFirstHotCueCallback(std::move(startAtFirstHotCueSetter)),
	  masterSelector   (masterManager,    0, 0, 2, 2, false, false, true, false),
	  headphoneSelector(headphoneManager, 0, 0, 2, 2, false, false, true, false)
{
	masterLabel.setText("Master Output", juce::dontSendNotification);
	masterLabel.setFont(juce::Font(juce::FontOptions{ 13.0f }).boldened());
	masterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	addAndMakeVisible(masterLabel);

	headphoneLabel.setText("Headphone / CUE Output  (use JACK type for per-sink routing)", juce::dontSendNotification);
	headphoneLabel.setFont(juce::Font(juce::FontOptions{ 13.0f }).boldened());
	headphoneLabel.setColour(juce::Label::textColourId, UI::deck2Accent);
	addAndMakeVisible(headphoneLabel);

	addAndMakeVisible(masterSelector);
	addAndMakeVisible(headphoneSelector);

	// Configure master volume slider (linear, continuous range)
	masterVolLabel.setFont(juce::Font(juce::FontOptions{ 11.0f }));
	masterVolLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	addAndMakeVisible(masterVolLabel);

	// Display label for master volume value (updated in real-time, 2 decimals)
	masterVolDisplayLabel.setFont(juce::Font(juce::FontOptions{ 10.0f }));
	masterVolDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	masterVolDisplayLabel.setJustificationType(juce::Justification::centredRight);
	masterVolDisplayLabel.setText(juce::String::formatted("%.2f", initialMasterGain), juce::dontSendNotification);
	addAndMakeVisible(masterVolDisplayLabel);

	masterVolSlider.setRange(0.0, 1.0, 0.0);  // continuous - no step snapping
	masterVolSlider.setValue(initialMasterGain, juce::dontSendNotification);
	masterVolSlider.setColour(juce::Slider::trackColourId, juce::Colours::grey);
	masterVolSlider.setColour(juce::Slider::thumbColourId, juce::Colours::aqua);
	masterVolSlider.onValueChange = [this]
	{
		// Audio gain update: atomic store — instant, no I/O.
		const float value = static_cast<float>(masterVolSlider.getValue());
		if (masterGainCallback)
			masterGainCallback(value);
		// Update display label with 2 decimal places.
		masterVolDisplayLabel.setText(juce::String::formatted("%.2f", value), juce::dontSendNotification);
	};
	masterVolSlider.onDragEnd = [this]
	{
		// Persist only once per drag gesture (not on every pixel).
		AppSettings::saveMasterGain(static_cast<float>(masterVolSlider.getValue()));
	};
	addAndMakeVisible(masterVolSlider);

	// Configure headphone volume slider (linear, continuous range)
	headphoneVolLabel.setFont(juce::Font(juce::FontOptions{ 11.0f }));
	headphoneVolLabel.setColour(juce::Label::textColourId, UI::deck2Accent);
	addAndMakeVisible(headphoneVolLabel);

	// Display label for headphone volume value (updated in real-time, 2 decimals)
	headphoneVolDisplayLabel.setFont(juce::Font(juce::FontOptions{ 10.0f }));
	headphoneVolDisplayLabel.setColour(juce::Label::textColourId, UI::deck2Accent);
	headphoneVolDisplayLabel.setJustificationType(juce::Justification::centredRight);
	headphoneVolDisplayLabel.setText(juce::String::formatted("%.2f", initialHeadphoneGain), juce::dontSendNotification);
	addAndMakeVisible(headphoneVolDisplayLabel);

	headphoneVolSlider.setRange(0.0, 1.0, 0.0);  // continuous - no step snapping
	headphoneVolSlider.setValue(initialHeadphoneGain, juce::dontSendNotification);
	headphoneVolSlider.setColour(juce::Slider::trackColourId, juce::Colours::grey);
	headphoneVolSlider.setColour(juce::Slider::thumbColourId, UI::deck2Accent);
	headphoneVolSlider.onValueChange = [this]
	{
		// Audio gain update: atomic store — instant, no I/O.
		const float value = static_cast<float>(headphoneVolSlider.getValue());
		if (headphoneGainCallback)
			headphoneGainCallback(value);
		// Update display label with 2 decimal places.
		headphoneVolDisplayLabel.setText(juce::String::formatted("%.2f", value), juce::dontSendNotification);
	};
	headphoneVolSlider.onDragEnd = [this]
	{
		// Persist only once per drag gesture (not on every pixel).
		AppSettings::saveHeadphoneGain(static_cast<float>(headphoneVolSlider.getValue()));
	};
	startAtFirstHotCueToggle.setToggleState(initialStartAtFirstHotCue, juce::dontSendNotification);
	startAtFirstHotCueToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
	startAtFirstHotCueToggle.onClick = [this]
	{
		if (startAtFirstHotCueCallback)
			startAtFirstHotCueCallback(startAtFirstHotCueToggle.getToggleState());
	};
	addAndMakeVisible(startAtFirstHotCueToggle);

	closeButton.setColour(juce::TextButton::buttonColourId,
	                      juce::Colour::fromRGBA(80, 80, 80, 200));
	closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	closeButton.onClick = [this]
	{
		// Persist gains (covers case where panel closes without a completed drag).
		AppSettings::saveMasterGain(static_cast<float>(masterVolSlider.getValue()));
		AppSettings::saveHeadphoneGain(static_cast<float>(headphoneVolSlider.getValue()));

		// Persist the headphone device state before hiding.
		if (auto xml = this->headphoneManager.createStateXml())
			AppSettings::saveHeadphoneDeviceState(xml.get());

		if (closeCallback)
			closeCallback();
	};
	addAndMakeVisible(closeButton);
}

SettingsPanel::~SettingsPanel() = default;

void SettingsPanel::paint(juce::Graphics& g)
{
	CustomLookAndFeel::paintPanelBackground(g, getLocalBounds().toFloat(), true, UI::kPanelRadius);
	g.setColour(UI::borderSubtle);
	g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), UI::kPanelRadius, 1.0f);
}

void SettingsPanel::resized()
{
	auto area = getLocalBounds().reduced(8);

	// Close button – top-right corner
	closeButton.setBounds(area.removeFromRight(28).removeFromTop(28));
	area.removeFromTop(4);

	const int half       = (area.getWidth() - 8) / 2;
	const int labelH     = 22;
	const int sliderH    = 80;

	auto leftCol  = area.removeFromLeft(half);
	auto rightCol = area.withTrimmedLeft(8);

	// Master side: device selector → volume slider
	masterLabel.setBounds(leftCol.removeFromTop(labelH));
	auto masterSelectorArea = leftCol;
	masterSelectorArea.removeFromBottom(sliderH);
	masterSelector.setBounds(masterSelectorArea);

	auto masterVolArea = leftCol.removeFromBottom(sliderH).reduced(0, 4);
	auto masterLabelRow = masterVolArea.removeFromTop(16);
	masterVolLabel.setBounds(masterLabelRow.removeFromLeft(masterLabelRow.getWidth() - 40));
	masterVolDisplayLabel.setBounds(masterLabelRow);  // Right side for value display
	masterVolSlider.setBounds(masterVolArea);

	// Headphone side: device selector → volume slider with display
	headphoneLabel.setBounds(rightCol.removeFromTop(labelH));
	auto headphoneSelectorArea = rightCol;
	headphoneSelectorArea.removeFromBottom(sliderH + 28);
	headphoneSelector.setBounds(headphoneSelectorArea);

	auto toggleArea = rightCol.removeFromBottom(28).reduced(0, 4);
	startAtFirstHotCueToggle.setBounds(toggleArea);

	auto headphoneVolArea = rightCol.removeFromBottom(sliderH).reduced(0, 4);
	auto headphoneLabelRow = headphoneVolArea.removeFromTop(16);
	headphoneVolLabel.setBounds(headphoneLabelRow.removeFromLeft(headphoneLabelRow.getWidth() - 40));
	headphoneVolDisplayLabel.setBounds(headphoneLabelRow);  // Right side for value display
	headphoneVolSlider.setBounds(headphoneVolArea);
}
