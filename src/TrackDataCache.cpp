#include "TrackDataCache.h"

//==============================================================================

namespace {
	/**
	 * Single-worker background ThreadPool used by loadAsync / updateAsync.
	 * One worker guarantees that consecutive updates for the same hash are
	 * serialized (avoiding torn JSON files during rapid BPM editing).
	 */
	juce::ThreadPool& trackDataPool()
	{
		static juce::ThreadPool pool { 1 };
		return pool;
	}
}

//==============================================================================

/**
 * Implementation of load method for TrackDataCache
 *
 * Reads a JSON file from ~/.otodecks/trackdata/<fileHash>.json
 * and populates a TrackData struct. Returns defaults if the file
 * does not exist or cannot be parsed.
 */
TrackData TrackDataCache::load(const juce::String& fileHash)
{
	TrackData data;

	juce::File file = getCacheFile(fileHash);
	if (!file.existsAsFile())
		return data;

	juce::String content = file.loadFileAsString();
	juce::var parsed = juce::JSON::parse(content);

	if (parsed.isObject())
	{
		if (parsed.hasProperty("detectedBpm"))
			data.detectedBpm = static_cast<double>(parsed["detectedBpm"]);
		if (parsed.hasProperty("confidence"))
			data.confidence = static_cast<double>(parsed["confidence"]);

		juce::var bg = parsed["beatGrid"];
		if (bg.isObject())
		{
			if (bg.hasProperty("bpm"))
				data.beatGrid.bpm = static_cast<double>(bg["bpm"]);
			if (bg.hasProperty("gridOffsetSecs"))
				data.beatGrid.gridOffsetSecs = static_cast<double>(bg["gridOffsetSecs"]);
			if (bg.hasProperty("isManualBpm"))
				data.beatGrid.isManualBpm = static_cast<bool>(bg["isManualBpm"]);
			if (bg.hasProperty("isManualOffset"))
				data.beatGrid.isManualOffset = static_cast<bool>(bg["isManualOffset"]);
		}
	}

	return data;
}

/**
 * Implementation of save method for TrackDataCache
 *
 * Writes a TrackData as a JSON file to ~/.otodecks/trackdata/<fileHash>.json.
 * Creates the trackdata directory if it does not exist.
 */
void TrackDataCache::save(const juce::String& fileHash, const TrackData& data)
{
	juce::File dir = getCacheDirectory();
	if (!dir.isDirectory())
		dir.createDirectory();

	auto* gridObj = new juce::DynamicObject();
	gridObj->setProperty("bpm", data.beatGrid.bpm);
	gridObj->setProperty("gridOffsetSecs", data.beatGrid.gridOffsetSecs);
	gridObj->setProperty("isManualBpm", data.beatGrid.isManualBpm);
	gridObj->setProperty("isManualOffset", data.beatGrid.isManualOffset);

	auto* obj = new juce::DynamicObject();
	obj->setProperty("detectedBpm", data.detectedBpm);
	obj->setProperty("confidence", data.confidence);
	obj->setProperty("beatGrid", juce::var(gridObj));

	juce::var jsonVar(obj);
	juce::String jsonString = juce::JSON::toString(jsonVar);

	juce::File file = getCacheFile(fileHash);
	file.replaceWithText(jsonString);
}

/**
 * Implementation of exists method for TrackDataCache
 */
bool TrackDataCache::exists(const juce::String& fileHash)
{
	return getCacheFile(fileHash).existsAsFile();
}

/**
 * Returns the directory where track data cache files are stored.
 */
juce::File TrackDataCache::getCacheDirectory()
{
	return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
		.getChildFile(".otodecks")
		.getChildFile("trackdata");
}

/**
 * Returns the file for a specific track's cached data.
 */
juce::File TrackDataCache::getCacheFile(const juce::String& fileHash)
{
	return getCacheDirectory().getChildFile(fileHash + ".json");
}

//==============================================================================

/**
 * Implementation of loadAsync method for TrackDataCache.
 *
 * Schedules the synchronous load on the dedicated worker pool and posts the
 * result back to the message thread once parsing is complete.
 */
void TrackDataCache::loadAsync(const juce::String& fileHash,
                               std::function<void(TrackData)> onLoaded)
{
	if (! onLoaded || fileHash.isEmpty())
		return;

	trackDataPool().addJob([hash = fileHash, cb = std::move(onLoaded)]() mutable {
		TrackData data = TrackDataCache::load(hash);
		juce::MessageManager::callAsync([cb = std::move(cb), data = std::move(data)]() mutable {
			cb(std::move(data));
		});
	});
}

/**
 * Implementation of updateAsync method for TrackDataCache.
 *
 * Read-modify-write on the worker thread. Sequential calls are serialized
 * by the single-worker pool, preventing torn JSON files during rapid edits.
 */
void TrackDataCache::updateAsync(const juce::String& fileHash,
                                 std::function<void(TrackData&)> mutator)
{
	if (! mutator || fileHash.isEmpty())
		return;

	trackDataPool().addJob([hash = fileHash, mut = std::move(mutator)]() mutable {
		TrackData data = TrackDataCache::load(hash);
		mut(data);
		TrackDataCache::save(hash, data);
	});
}
