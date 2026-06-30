#include <JuceHeader.h>
#include "SettingsPanel.h"
#include "../../core/data/AppSettings.h"
#include "../../utilities/UIConstants.h"
#include "CustomLookAndFeel.h"

SettingsPanel::SettingsPanel(juce::AudioDeviceManager& masterManager,
                             juce::AudioDeviceManager& headphoneManager,
                             Midi::MidiMapper&         midiMapper,
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
	  startAtFirstHotCueCallback(std::move(startAtFirstHotCueSetter)),
	  masterManager(masterManager),
	  headphoneManager(headphoneManager),
	  masterSelector   (masterManager,    0, 0, 2, 2, false, false, true, false),
	  headphoneSelector(headphoneManager, 0, 0, 2, 2, false, false, true, false),
	  midiPanel(midiMapper)
{
	addAndMakeVisible(generalTabBtn);
	addAndMakeVisible(audioTabBtn);
	addAndMakeVisible(midiTabBtn);
	addAndMakeVisible(closeButton);
	addAndMakeVisible(generalPanel);
	addAndMakeVisible(audioPanel);
	addAndMakeVisible(midiPanel);

	generalPanel.addAndMakeVisible(generalHeaderLabel);
	generalPanel.addAndMakeVisible(startAtFirstHotCueToggle);

	generalHeaderLabel.setFont(juce::Font(juce::FontOptions{ 13.0f }).boldened());
	generalHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	generalHeaderLabel.setJustificationType(juce::Justification::centredLeft);

	audioPanel.addAndMakeVisible(masterLabel);
	audioPanel.addAndMakeVisible(headphoneLabel);
	audioPanel.addAndMakeVisible(masterSelector);
	audioPanel.addAndMakeVisible(headphoneSelector);
	audioPanel.addAndMakeVisible(masterVolLabel);
	audioPanel.addAndMakeVisible(masterVolDisplayLabel);
	audioPanel.addAndMakeVisible(masterVolSlider);
	audioPanel.addAndMakeVisible(headphoneVolLabel);
	audioPanel.addAndMakeVisible(headphoneVolDisplayLabel);
	audioPanel.addAndMakeVisible(headphoneVolSlider);

	masterLabel.setText("Master Output", juce::dontSendNotification);
	masterLabel.setFont(juce::Font(juce::FontOptions{ 13.0f }).boldened());
	masterLabel.setColour(juce::Label::textColourId, juce::Colours::white);

	headphoneLabel.setText("Headphone / CUE Output  (use JACK type for per-sink routing)", juce::dontSendNotification);
	headphoneLabel.setFont(juce::Font(juce::FontOptions{ 13.0f }).boldened());
	headphoneLabel.setColour(juce::Label::textColourId, UI::deck2Accent);

	masterVolLabel.setFont(juce::Font(juce::FontOptions{ 11.0f }));
	masterVolLabel.setColour(juce::Label::textColourId, juce::Colours::white);

	masterVolDisplayLabel.setFont(juce::Font(juce::FontOptions{ 10.0f }));
	masterVolDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	masterVolDisplayLabel.setJustificationType(juce::Justification::centredRight);
	masterVolDisplayLabel.setText(juce::String::formatted("%.2f", initialMasterGain), juce::dontSendNotification);

	masterVolSlider.setRange(0.0, 1.0, 0.0);  // continuous - no step snapping
	masterVolSlider.setValue(initialMasterGain, juce::dontSendNotification);
	masterVolSlider.setColour(juce::Slider::trackColourId, juce::Colours::grey);
	masterVolSlider.setColour(juce::Slider::thumbColourId, juce::Colours::aqua);
	masterVolSlider.onValueChange = [this]
	{
		const float value = static_cast<float>(masterVolSlider.getValue());
		if (masterGainCallback)
			masterGainCallback(value);
		masterVolDisplayLabel.setText(juce::String::formatted("%.2f", value), juce::dontSendNotification);
	};
	masterVolSlider.onDragEnd = [this]
	{
		AppSettings::saveMasterGain(static_cast<float>(masterVolSlider.getValue()));
	};

	headphoneVolLabel.setFont(juce::Font(juce::FontOptions{ 11.0f }));
	headphoneVolLabel.setColour(juce::Label::textColourId, UI::deck2Accent);

	headphoneVolDisplayLabel.setFont(juce::Font(juce::FontOptions{ 10.0f }));
	headphoneVolDisplayLabel.setColour(juce::Label::textColourId, UI::deck2Accent);
	headphoneVolDisplayLabel.setJustificationType(juce::Justification::centredRight);
	headphoneVolDisplayLabel.setText(juce::String::formatted("%.2f", initialHeadphoneGain), juce::dontSendNotification);

	headphoneVolSlider.setRange(0.0, 1.0, 0.0);  // continuous - no step snapping
	headphoneVolSlider.setValue(initialHeadphoneGain, juce::dontSendNotification);
	headphoneVolSlider.setColour(juce::Slider::trackColourId, juce::Colours::grey);
	headphoneVolSlider.setColour(juce::Slider::thumbColourId, UI::deck2Accent);
	headphoneVolSlider.onValueChange = [this]
	{
		const float value = static_cast<float>(headphoneVolSlider.getValue());
		if (headphoneGainCallback)
			headphoneGainCallback(value);
		headphoneVolDisplayLabel.setText(juce::String::formatted("%.2f", value), juce::dontSendNotification);
	};
	headphoneVolSlider.onDragEnd = [this]
	{
		AppSettings::saveHeadphoneGain(static_cast<float>(headphoneVolSlider.getValue()));
	};

	startAtFirstHotCueToggle.setToggleState(initialStartAtFirstHotCue, juce::dontSendNotification);
	startAtFirstHotCueToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
	startAtFirstHotCueToggle.onClick = [this]
	{
		AppSettings::saveStartAtFirstHotCue(startAtFirstHotCueToggle.getToggleState());
		if (startAtFirstHotCueCallback)
			startAtFirstHotCueCallback(startAtFirstHotCueToggle.getToggleState());
	};

	closeButton.setColour(juce::TextButton::buttonColourId,
	                      juce::Colour::fromRGBA(80, 80, 80, 200));
	closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	closeButton.onClick = [this]
	{
		AppSettings::saveMasterGain(static_cast<float>(masterVolSlider.getValue()));
		AppSettings::saveHeadphoneGain(static_cast<float>(headphoneVolSlider.getValue()));

		if (auto xml = this->masterManager.createStateXml())
			AppSettings::saveMasterDeviceState(xml.get());
		if (auto xml = this->headphoneManager.createStateXml())
			AppSettings::saveHeadphoneDeviceState(xml.get());

		if (closeCallback)
			closeCallback();
	};

	generalTabBtn.onClick = [this] { showTab(0); };
	audioTabBtn.onClick = [this] { showTab(1); };
	midiTabBtn.onClick = [this] { showTab(2); };

	showTab(1);
}

SettingsPanel::~SettingsPanel() = default;

void SettingsPanel::paint(juce::Graphics& g)
{
	CustomLookAndFeel::paintPanelBackground(g, getLocalBounds().toFloat(), true, UI::kPanelRadius);
	g.setColour(UI::borderSubtle);
	g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), UI::kPanelRadius, 1.0f);
}

void SettingsPanel::showTab(int tabIndex)
{
	currentTab = tabIndex;
	generalPanel.setVisible(tabIndex == 0);
	audioPanel.setVisible(tabIndex == 1);
	midiPanel.setVisible(tabIndex == 2);

	if (tabIndex == 0)
		generalPanel.toFront(false);
	else if (tabIndex == 1)
		audioPanel.toFront(false);
	else if (tabIndex == 2)
		midiPanel.toFront(false);

	resized();

	generalTabBtn.setColour(juce::TextButton::buttonColourId,
		(tabIndex == 0) ? UI::deck1Accent.withAlpha(0.8f) : UI::bgCard);
	audioTabBtn.setColour(juce::TextButton::buttonColourId,
		(tabIndex == 1) ? UI::deck1Accent.withAlpha(0.8f) : UI::bgCard);
	midiTabBtn.setColour(juce::TextButton::buttonColourId,
		(tabIndex == 2) ? UI::deck1Accent.withAlpha(0.8f) : UI::bgCard);
}

void SettingsPanel::resized()
{
	auto area = getLocalBounds().reduced(8);

	// Tab bar and close button
	auto tabsArea = area.removeFromTop(32);
	auto closeArea = tabsArea.removeFromRight(32);
	closeButton.setBounds(closeArea);

	const int tabW = tabsArea.getWidth() / 3;
	generalTabBtn.setBounds(tabsArea.removeFromLeft(tabW));
	audioTabBtn.setBounds(tabsArea.removeFromLeft(tabW));
	midiTabBtn.setBounds(tabsArea);

	generalPanel.setBounds(area);
	audioPanel.setBounds(area);
	midiPanel.setBounds(area);

	if (generalPanel.isVisible()) {
		auto generalArea = generalPanel.getLocalBounds().reduced(16);
		generalHeaderLabel.setBounds(16, 16, generalArea.getWidth() - 32, 24);
		startAtFirstHotCueToggle.setBounds(16, 50, generalArea.getWidth() - 32, 28);
		generalHeaderLabel.setVisible(true);
		startAtFirstHotCueToggle.setVisible(true);
	}

	if (audioPanel.isVisible()) {
		auto audioArea = audioPanel.getLocalBounds().reduced(8);
		const int labelH = 22;
		const int sliderH = 80;

		auto leftCol = audioArea.removeFromLeft(audioArea.getWidth() / 2);
		auto rightCol = audioArea.withTrimmedLeft(8);

		masterLabel.setBounds(leftCol.removeFromTop(labelH));
		auto masterSelectorArea = leftCol;
		masterSelectorArea.removeFromBottom(sliderH);
		masterSelector.setBounds(masterSelectorArea);

		auto masterVolArea = leftCol.removeFromBottom(sliderH).reduced(0, 4);
		auto masterLabelRow = masterVolArea.removeFromTop(16);
		masterVolLabel.setBounds(masterLabelRow.removeFromLeft(masterLabelRow.getWidth() - 40));
		masterVolDisplayLabel.setBounds(masterLabelRow);
		masterVolSlider.setBounds(masterVolArea);

		headphoneLabel.setBounds(rightCol.removeFromTop(labelH));
		auto headphoneSelectorArea = rightCol;
		headphoneSelectorArea.removeFromBottom(sliderH);
		headphoneSelector.setBounds(headphoneSelectorArea);

		auto headphoneVolArea = rightCol.removeFromBottom(sliderH).reduced(0, 4);
		auto headphoneLabelRow = headphoneVolArea.removeFromTop(16);
		headphoneVolLabel.setBounds(headphoneLabelRow.removeFromLeft(headphoneLabelRow.getWidth() - 40));
		headphoneVolDisplayLabel.setBounds(headphoneLabelRow);
		headphoneVolSlider.setBounds(headphoneVolArea);
	}

	if (midiPanel.isVisible()) {
		midiPanel.setBounds(area);
	}
}
