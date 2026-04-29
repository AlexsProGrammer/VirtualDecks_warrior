#include <JuceHeader.h>
#include "SettingsPanel.h"
#include "AppSettings.h"
#include "UIConstants.h"
#include "CustomLookAndFeel.h"

SettingsPanel::SettingsPanel(juce::AudioDeviceManager& masterManager,
                             juce::AudioDeviceManager& headphoneManager,
                             std::function<void()>     onClose)
	: closeCallback(std::move(onClose)),
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

	auto leftCol  = area.removeFromLeft(half);
	auto rightCol = area.withTrimmedLeft(8);

	masterLabel.setBounds(leftCol.removeFromTop(labelH));
	masterSelector.setBounds(leftCol);

	headphoneLabel.setBounds(rightCol.removeFromTop(labelH));
	headphoneSelector.setBounds(rightCol);
}
