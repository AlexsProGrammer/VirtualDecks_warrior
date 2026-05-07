#include <JuceHeader.h>
#include "AppSettings.h"

static constexpr const char* kHeadphoneStateTag = "HeadphoneDeviceState";

juce::File AppSettings::getSettingsFile()
{
	return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
	           .getChildFile(".otodecks/AppSettings.xml");
}

std::unique_ptr<juce::XmlElement> AppSettings::loadHeadphoneDeviceState()
{
	const juce::File f = getSettingsFile();
	if (!f.existsAsFile())
		return nullptr;

	auto root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		return nullptr;

	if (auto* child = root->getChildByName(kHeadphoneStateTag))
		return std::make_unique<juce::XmlElement>(*child);

	return nullptr;
}

void AppSettings::saveHeadphoneDeviceState(const juce::XmlElement* state)
{
	const juce::File f = getSettingsFile();
	f.getParentDirectory().createDirectory();

	// Load or create root document.
	std::unique_ptr<juce::XmlElement> root;
	if (f.existsAsFile())
		root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		root = std::make_unique<juce::XmlElement>("OtoDecksSettings");

	// Replace the headphone state child.
	root->deleteAllChildElementsWithTagName(kHeadphoneStateTag);

	if (state != nullptr)
	{
		auto* copy = new juce::XmlElement(*state);
		copy->setTagName(kHeadphoneStateTag);
		root->addChildElement(copy);
	}

	root->writeTo(f, juce::XmlElement::TextFormat{});
}
