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
                             float                      initialMasterGain,
                             float                      initialHeadphoneGain)
	: closeCallback(std::move(onClose)),
	  masterGainCallback(std::move(masterGainSetter)),
	  headphoneGainCallback(std::move(headphoneGainSetter)),
	  headphoneManager(headphoneManager),
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

	// Configure master volume slider
	masterVolLabel.setFont(juce::Font(11.0f));
	masterVolLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	addAndMakeVisible(masterVolLabel);

	masterVolSlider.setRange(0.0, 1.0, 0.01);
	masterVolSlider.setSkewFactor(0.25);
	masterVolSlider.setValue(initialMasterGain, juce::dontSendNotification);
	masterVolSlider.setColour(juce::Slider::trackColourId, juce::Colours::grey);
	masterVolSlider.setColour(juce::Slider::thumbColourId, juce::Colours::aqua);
	masterVolSlider.onValueChange = [this]
	{
		const float gain = static_cast<float>(masterVolSlider.getValue());
		if (masterGainCallback)
			masterGainCallback(gain);
		AppSettings::saveMasterGain(gain);
	};
	addAndMakeVisible(masterVolSlider);

	// Configure headphone volume slider
	headphoneVolLabel.setFont(juce::Font(11.0f));
	headphoneVolLabel.setColour(juce::Label::textColourId, UI::deck2Accent);
	addAndMakeVisible(headphoneVolLabel);

	headphoneVolSlider.setRange(0.0, 1.0, 0.01);
	headphoneVolSlider.setSkewFactor(0.25);
	headphoneVolSlider.setValue(initialHeadphoneGain, juce::dontSendNotification);
	headphoneVolSlider.setColour(juce::Slider::trackColourId, juce::Colours::grey);
	headphoneVolSlider.setColour(juce::Slider::thumbColourId, UI::deck2Accent);
	headphoneVolSlider.onValueChange = [this]
	{
		const float gain = static_cast<float>(headphoneVolSlider.getValue());
		if (headphoneGainCallback)
			headphoneGainCallback(gain);
		AppSettings::saveHeadphoneGain(gain);
	};
	addAndMakeVisible(headphoneVolSlider);

	closeButton.setColour(juce::TextButton::buttonColourId,
	                      juce::Colour::fromRGBA(80, 80, 80, 200));
	closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	closeButton.onClick = [this]
	{
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

	// Master side: device selector then volume slider
	masterLabel.setBounds(leftCol.removeFromTop(labelH));
	auto masterSelectorArea = leftCol;
	masterSelectorArea.removeFromBottom(sliderH);
	masterSelector.setBounds(masterSelectorArea);

	auto masterVolArea = leftCol.removeFromBottom(sliderH).reduced(0, 4);
	masterVolLabel.setBounds(masterVolArea.removeFromTop(16));
	masterVolSlider.setBounds(masterVolArea);

	// Headphone side: device selector then volume slider
	headphoneLabel.setBounds(rightCol.removeFromTop(labelH));
	auto headphoneSelectorArea = rightCol;
	headphoneSelectorArea.removeFromBottom(sliderH);
	headphoneSelector.setBounds(headphoneSelectorArea);

	auto headphoneVolArea = rightCol.removeFromBottom(sliderH).reduced(0, 4);
	headphoneVolLabel.setBounds(headphoneVolArea.removeFromTop(16));
	headphoneVolSlider.setBounds(headphoneVolArea);
}
