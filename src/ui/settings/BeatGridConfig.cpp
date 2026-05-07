#include "BeatGridConfig.h"

//==============================================================================

/**
 * Implementation of load method for BeatGridConfig
 *
 * Reads a JSON file from ~/.otodecks/grids/<trackIdentity>.json
 * and populates a BeatGrid struct. Returns a default BeatGrid if
 * the file does not exist or cannot be parsed.
 */
BeatGrid BeatGridConfig::load(const juce::String& trackIdentity)
{
	BeatGrid grid;

	juce::File file = getGridFile(trackIdentity);
	if (!file.existsAsFile())
		return grid;

	juce::String content = file.loadFileAsString();
	juce::var parsed = juce::JSON::parse(content);

	if (parsed.isObject())
	{
		if (parsed.hasProperty("bpm"))
			grid.bpm = static_cast<double>(parsed["bpm"]);
		if (parsed.hasProperty("gridOffsetSecs"))
			grid.gridOffsetSecs = static_cast<double>(parsed["gridOffsetSecs"]);
		if (parsed.hasProperty("isManualBpm"))
			grid.isManualBpm = static_cast<bool>(parsed["isManualBpm"]);
		if (parsed.hasProperty("isManualOffset"))
			grid.isManualOffset = static_cast<bool>(parsed["isManualOffset"]);
	}

	return grid;
}

/**
 * Implementation of save method for BeatGridConfig
 *
 * Writes a BeatGrid as a JSON file to ~/.otodecks/grids/<trackIdentity>.json.
 * Creates the grids directory if it does not exist.
 */
void BeatGridConfig::save(const juce::String& trackIdentity, const BeatGrid& grid)
{
	juce::File dir = getGridDirectory();
	if (!dir.isDirectory())
		dir.createDirectory();

	auto* obj = new juce::DynamicObject();
	obj->setProperty("bpm", grid.bpm);
	obj->setProperty("gridOffsetSecs", grid.gridOffsetSecs);
	obj->setProperty("isManualBpm", grid.isManualBpm);
	obj->setProperty("isManualOffset", grid.isManualOffset);

	juce::var jsonVar(obj);
	juce::String jsonString = juce::JSON::toString(jsonVar);

	juce::File file = getGridFile(trackIdentity);
	file.replaceWithText(jsonString);
}

/**
 * Returns the directory where grid config files are stored.
 */
juce::File BeatGridConfig::getGridDirectory()
{
	return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
		.getChildFile(".otodecks")
		.getChildFile("grids");
}

/**
 * Returns the file for a specific track's grid configuration.
 */
juce::File BeatGridConfig::getGridFile(const juce::String& trackIdentity)
{
	return getGridDirectory().getChildFile(trackIdentity + ".json");
}
