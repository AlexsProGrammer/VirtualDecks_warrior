#include <JuceHeader.h>
#include "AppSettings.h"

static constexpr const char* kHeadphoneStateTag = "HeadphoneDeviceState";
static constexpr const char* kMasterStateTag = "MasterDeviceState";
static constexpr const char* kMasterGainAttr = "masterGain";
static constexpr const char* kHeadphoneGainAttr = "headphoneGain";
static constexpr const char* kStartAtFirstHotCueAttr = "startAtFirstHotCue";

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

std::unique_ptr<juce::XmlElement> AppSettings::loadMasterDeviceState()
{
	const juce::File f = getSettingsFile();
	if (!f.existsAsFile())
		return nullptr;

	auto root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		return nullptr;

	if (auto* child = root->getChildByName(kMasterStateTag))
		return std::make_unique<juce::XmlElement>(*child);

	return nullptr;
}

void AppSettings::saveMasterDeviceState(const juce::XmlElement* state)
{
	const juce::File f = getSettingsFile();
	f.getParentDirectory().createDirectory();

	std::unique_ptr<juce::XmlElement> root;
	if (f.existsAsFile())
		root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		root = std::make_unique<juce::XmlElement>("OtoDecksSettings");

	root->deleteAllChildElementsWithTagName(kMasterStateTag);

	if (state != nullptr)
	{
		auto* copy = new juce::XmlElement(*state);
		copy->setTagName(kMasterStateTag);
		root->addChildElement(copy);
	}

	root->writeTo(f, juce::XmlElement::TextFormat{});
}

bool AppSettings::loadStartAtFirstHotCue()
{
	const juce::File f = getSettingsFile();
	if (!f.existsAsFile())
		return false;

	auto root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		return false;

	return root->getBoolAttribute(kStartAtFirstHotCueAttr, false);
}

void AppSettings::saveStartAtFirstHotCue(bool value)
{
	const juce::File f = getSettingsFile();
	f.getParentDirectory().createDirectory();

	std::unique_ptr<juce::XmlElement> root;
	if (f.existsAsFile())
		root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		root = std::make_unique<juce::XmlElement>("OtoDecksSettings");

	root->setAttribute(kStartAtFirstHotCueAttr, value);
	root->writeTo(f, juce::XmlElement::TextFormat{});
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

float AppSettings::loadMasterGain()
{
	const juce::File f = getSettingsFile();
	if (!f.existsAsFile())
		return 1.0f;

	auto root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		return 1.0f;

	if (root->hasAttribute(kMasterGainAttr))
	{
		const float gain = root->getDoubleAttribute(kMasterGainAttr, 1.0);
		return juce::jlimit(0.0f, 1.0f, static_cast<float>(gain));
	}

	return 1.0f;
}

void AppSettings::saveMasterGain(float gain)
{
	const juce::File f = getSettingsFile();
	f.getParentDirectory().createDirectory();

	std::unique_ptr<juce::XmlElement> root;
	if (f.existsAsFile())
		root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		root = std::make_unique<juce::XmlElement>("OtoDecksSettings");

	root->setAttribute(kMasterGainAttr, static_cast<double>(gain));
	root->writeTo(f, juce::XmlElement::TextFormat{});
}

float AppSettings::loadHeadphoneGain()
{
	const juce::File f = getSettingsFile();
	if (!f.existsAsFile())
		return 1.0f;

	auto root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		return 1.0f;

	if (root->hasAttribute(kHeadphoneGainAttr))
	{
		const float gain = root->getDoubleAttribute(kHeadphoneGainAttr, 1.0);
		return juce::jlimit(0.0f, 1.0f, static_cast<float>(gain));
	}

	return 1.0f;
}

void AppSettings::saveHeadphoneGain(float gain)
{
	const juce::File f = getSettingsFile();
	f.getParentDirectory().createDirectory();

	std::unique_ptr<juce::XmlElement> root;
	if (f.existsAsFile())
		root = juce::XmlDocument::parse(f);
	if (root == nullptr)
		root = std::make_unique<juce::XmlElement>("OtoDecksSettings");

	root->setAttribute(kHeadphoneGainAttr, static_cast<double>(gain));
	root->writeTo(f, juce::XmlElement::TextFormat{});
}
