#include "TrackDataCache.h"
#include "../analysis/WaveformBandAnalyzer.h"

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

		juce::var hotCuesVar = parsed["hotCues"];
		if (hotCuesVar.isArray())
		{
			auto& array = *hotCuesVar.getArray();
			const int maxItems = juce::jmin((int)array.size(), 6);
			for (int i = 0; i < maxItems; ++i)
			{
				if (array[i].isObject())
				{
					auto* obj = array[i].getDynamicObject();
					if (obj != nullptr)
					{
						if (obj->hasProperty("relativePos"))
							data.hotCues[i].relativePos = static_cast<double>(obj->getProperty("relativePos"));
						if (obj->hasProperty("hue"))
							data.hotCues[i].hue = static_cast<float>(obj->getProperty("hue"));
					}
				}
			}
		}

		if (parsed.hasProperty("loopInRelative"))
			data.loopInRelative = static_cast<double>(parsed["loopInRelative"]);
		if (parsed.hasProperty("loopOutRelative"))
			data.loopOutRelative = static_cast<double>(parsed["loopOutRelative"]);
		if (parsed.hasProperty("loopActive"))
			data.loopActive = static_cast<bool>(parsed["loopActive"]);
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

	juce::Array<juce::var> hotCueArray;
	hotCueArray.ensureStorageAllocated(6);
	for (int i = 0; i < 6; ++i)
	{
		auto* cueObj = new juce::DynamicObject();
		cueObj->setProperty("relativePos", data.hotCues[i].relativePos);
		cueObj->setProperty("hue", data.hotCues[i].hue);
		hotCueArray.add(juce::var(cueObj));
	}
	obj->setProperty("hotCues", juce::var(hotCueArray));
	obj->setProperty("loopInRelative", data.loopInRelative);
	obj->setProperty("loopOutRelative", data.loopOutRelative);
	obj->setProperty("loopActive", data.loopActive);

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

/**
 * Returns the file for a specific track's cached waveform band data.
 */
juce::File TrackDataCache::getBandsFile(const juce::String& fileHash)
{
	return getCacheDirectory().getChildFile(fileHash + ".bands");
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

//==============================================================================
// Waveform band data - binary on-disk format:
//   [4 bytes] magic 'B','N','D','1'
//   [4 bytes] little-endian uint32 frame count
//   [4*N    ] BandFrame { low, mid, high, amp } as raw uint8 quadruplets
//==============================================================================

namespace {
	constexpr char kBandMagic0 = 'B';
	constexpr char kBandMagic1 = 'N';
	constexpr char kBandMagic2 = 'D';
	constexpr char kBandMagic3 = '1';
}

/**
 * Implementation of loadBands for TrackDataCache.
 */
std::shared_ptr<const std::vector<BandFrame>>
TrackDataCache::loadBands(const juce::String& fileHash)
{
	if (fileHash.isEmpty())
		return nullptr;

	juce::File file = getBandsFile(fileHash);
	if (! file.existsAsFile())
		return nullptr;

	juce::FileInputStream in(file);
	if (! in.openedOk())
		return nullptr;

	char magic[4] = {};
	if (in.read(magic, 4) != 4
	    || magic[0] != kBandMagic0 || magic[1] != kBandMagic1
	    || magic[2] != kBandMagic2 || magic[3] != kBandMagic3)
		return nullptr;

	const int frameCount = (int)in.readInt();
	if (frameCount <= 0 || frameCount > 1000000)
		return nullptr;

	auto bands = std::make_shared<std::vector<BandFrame>>();
	bands->resize((size_t)frameCount);
	const int bytesExpected = frameCount * (int)sizeof(BandFrame);
	const int bytesRead = in.read(bands->data(), bytesExpected);
	if (bytesRead != bytesExpected)
		return nullptr;

	return std::shared_ptr<const std::vector<BandFrame>>(std::move(bands));
}

/**
 * Implementation of saveBands for TrackDataCache.
 */
void TrackDataCache::saveBands(const juce::String& fileHash,
                               const std::vector<BandFrame>& bands)
{
	if (fileHash.isEmpty() || bands.empty())
		return;

	juce::File dir = getCacheDirectory();
	if (! dir.isDirectory())
		dir.createDirectory();

	juce::File file = getBandsFile(fileHash);
	juce::TemporaryFile tmp(file);

	{
		juce::FileOutputStream out(tmp.getFile());
		if (! out.openedOk())
			return;
		out.writeByte(kBandMagic0);
		out.writeByte(kBandMagic1);
		out.writeByte(kBandMagic2);
		out.writeByte(kBandMagic3);
		out.writeInt((int)bands.size());
		out.write(bands.data(), bands.size() * sizeof(BandFrame));
		out.flush();
	}

	tmp.overwriteTargetFileWithTemporary();
}
