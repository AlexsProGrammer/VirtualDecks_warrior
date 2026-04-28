#include "FileHasher.h"

//==============================================================================

namespace {
	/**
	 * Single-worker background ThreadPool used by computeHashAsync.
	 * Hashing reads ~128 KB of disk; one worker is plenty and prevents
	 * concurrent disk seeks on spinning media.
	 */
	juce::ThreadPool& fileHashPool()
	{
		static juce::ThreadPool pool { 1 };
		return pool;
	}
}

//==============================================================================

/**
 * Implementation of computeHash method for FileHasher.
 *
 * FNV-1a 64-bit over first+last 64 KB chunks plus the file size.
 */
juce::String FileHasher::computeHash(const juce::File& file)
{
	if (! file.existsAsFile())
		return {};

	auto fileSize = file.getSize();
	if (fileSize <= 0)
		return {};

	juce::MemoryBlock data;
	constexpr juce::int64 chunkSize = 65536; // 64 KB

	juce::FileInputStream stream(file);
	if (! stream.openedOk())
		return {};

	if (fileSize <= chunkSize * 2)
	{
		data.setSize(static_cast<size_t>(fileSize));
		stream.read(data.getData(), static_cast<int>(fileSize));
	}
	else
	{
		juce::MemoryBlock head(static_cast<size_t>(chunkSize));
		stream.read(head.getData(), static_cast<int>(chunkSize));

		juce::MemoryBlock tail(static_cast<size_t>(chunkSize));
		stream.setPosition(fileSize - chunkSize);
		stream.read(tail.getData(), static_cast<int>(chunkSize));

		data.append(head.getData(), head.getSize());
		data.append(tail.getData(), tail.getSize());
	}

	// Append file size for extra uniqueness
	data.append(&fileSize, sizeof(fileSize));

	// FNV-1a 64-bit hash
	const auto* bytes = static_cast<const uint8_t*>(data.getData());
	uint64_t h = 14695981039346656037ULL;
	for (size_t i = 0; i < data.getSize(); ++i)
	{
		h ^= static_cast<uint64_t>(bytes[i]);
		h *= 1099511628211ULL;
	}

	return juce::String::toHexString(static_cast<juce::int64>(h));
}

//==============================================================================

/**
 * Implementation of computeHashAsync method for FileHasher.
 *
 * Schedules the hash computation on the background worker pool and posts the
 * result back to the message thread.
 */
void FileHasher::computeHashAsync(juce::File file,
                                  std::function<void(juce::String)> onDone)
{
	if (! onDone)
		return;

	fileHashPool().addJob([f = std::move(file), cb = std::move(onDone)]() mutable {
		juce::String hash = FileHasher::computeHash(f);
		juce::MessageManager::callAsync([cb = std::move(cb), hash = std::move(hash)]() mutable {
			cb(std::move(hash));
		});
	});
}
