#include "AudioEngine.h"

//==============================================================================
// LoadJob — runs on a worker thread of loadPool. Reads file headers, builds an
// AudioFormatReaderSource, then marshalls the result back to the message
// thread via MessageManager::callAsync.
//==============================================================================
class AudioEngine::LoadJob : public juce::ThreadPoolJob
{
public:
	LoadJob(AudioEngine& engineToUse,
	        int deckIndex,
	        juce::URL urlToLoad,
	        int requestGeneration,
	        std::function<void(bool)> completionCallback)
		: juce::ThreadPoolJob("AudioEngine::LoadJob")
		, engine(engineToUse)
		, deck(deckIndex)
		, url(std::move(urlToLoad))
		, generation(requestGeneration)
		, onComplete(std::move(completionCallback))
	{
	}

	JobStatus runJob() override
	{
		// If a newer load was queued for this deck while we were waiting in the
		// pool, abandon early — the newer job will overwrite anyway.
		if (generation != engine.loadGeneration[deck].load(std::memory_order_acquire))
			return JobStatus::jobHasFinished;

		// Open input stream + create reader on this background thread.
		std::unique_ptr<juce::InputStream> stream(url.createInputStream(false));
		juce::AudioFormatReader* reader = (stream != nullptr)
			? engine.loadFormatManager.createReaderFor(std::move(stream))
			: nullptr;

		if (shouldExit())
			return JobStatus::jobHasFinished;

		if (reader == nullptr)
		{
			// Marshal failure to message thread.
			AudioEngine& eng = engine;
			int          d   = deck;
			juce::URL    u   = url;
			auto         cb  = std::move(onComplete);
			juce::MessageManager::callAsync([&eng, d, u, cb]() mutable
			{
				eng.completeLoadFailed(d, u, std::move(cb));
			});
			return JobStatus::jobHasFinished;
		}

		const double sampleRate = reader->sampleRate;
		auto source = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

		// Heap-stash the unique_ptr so the lambda below can transport ownership
		// across MessageManager::callAsync (lambda captures must be copyable; we
		// use a shared_ptr to a unique_ptr as a one-shot smuggler).
		auto sourceHolder = std::make_shared<std::unique_ptr<juce::AudioFormatReaderSource>>(std::move(source));

		AudioEngine& eng        = engine;
		int          d          = deck;
		int          gen        = generation;
		juce::URL    u          = url;
		auto         cb         = std::move(onComplete);

		juce::MessageManager::callAsync([&eng, d, gen, sourceHolder, sampleRate, u, cb]() mutable
		{
			// Re-check generation on the message thread: a newer requestLoad
			// may have been issued while we were posting this callback.
			if (gen != eng.loadGeneration[d].load(std::memory_order_acquire))
			{
				if (cb) cb(false);
				return;
			}
			eng.completeLoad(d, std::move(*sourceHolder), sampleRate, u, std::move(cb));
		});

		return JobStatus::jobHasFinished;
	}

private:
	AudioEngine&             engine;
	int                      deck;
	juce::URL                url;
	int                      generation;
	std::function<void(bool)> onComplete;
};

//==============================================================================
// AudioEngine
//==============================================================================

AudioEngine::AudioEngine(juce::AudioFormatManager& shared)
	: sharedFormatManager(shared)
	, player1(shared)
	, player2(shared)
{
	loadFormatManager.registerBasicFormats();
}

AudioEngine::~AudioEngine()
{
	// Block briefly for any in-flight load jobs to drain. removeAllJobs
	// signals shouldExit and waits up to the timeout.
	loadPool.removeAllJobs(true, 5000);
}

//==============================================================================

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
	mixerSource.addInputSource(&player1, false);
	mixerSource.addInputSource(&player2, false);

	player1.prepareToPlay(samplesPerBlockExpected, sampleRate);
	player2.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	mixerSource.getNextAudioBlock(bufferToFill);
}

void AudioEngine::releaseResources()
{
	mixerSource.removeAllInputs();
	mixerSource.releaseResources();
	player1.releaseResources();
	player2.releaseResources();
}

//==============================================================================

DJAudioPlayer& AudioEngine::getPlayer(int deckIndex) noexcept
{
	return deckIndex == 0 ? player1 : player2;
}

const DJAudioPlayer& AudioEngine::getPlayer(int deckIndex) const noexcept
{
	return deckIndex == 0 ? player1 : player2;
}

//==============================================================================

void AudioEngine::requestLoad(int deckIndex,
                              juce::URL audioURL,
                              std::function<void(bool)> onComplete)
{
	jassert(deckIndex == 0 || deckIndex == 1);
	jassert(juce::MessageManager::existsAndIsCurrentThread());

	// Bump generation BEFORE marking loading so any in-flight job will see the
	// mismatch and abandon.
	const int gen = loadGeneration[deckIndex].fetch_add(1, std::memory_order_acq_rel) + 1;

	getPlayer(deckIndex).markLoading();
	notifyLoadingStateChanged(deckIndex, DJAudioPlayer::LoadingState::Loading);

	loadPool.addJob(new LoadJob(*this, deckIndex, std::move(audioURL), gen, std::move(onComplete)),
	                /*deleteJobWhenFinished*/ true);
}

//==============================================================================

void AudioEngine::completeLoad(int deckIndex,
                               std::unique_ptr<juce::AudioFormatReaderSource> source,
                               double sampleRate,
                               juce::URL audioURL,
                               std::function<void(bool)> onComplete)
{
	jassert(juce::MessageManager::existsAndIsCurrentThread());

	getPlayer(deckIndex).installLoadedSource(std::move(source), sampleRate, audioURL);
	notifyLoadingStateChanged(deckIndex, DJAudioPlayer::LoadingState::Loaded);

	if (onComplete) onComplete(true);
}

void AudioEngine::completeLoadFailed(int deckIndex,
                                     juce::URL /*audioURL*/,
                                     std::function<void(bool)> onComplete)
{
	jassert(juce::MessageManager::existsAndIsCurrentThread());

	getPlayer(deckIndex).markLoadFailed();
	notifyLoadingStateChanged(deckIndex, DJAudioPlayer::LoadingState::Failed);

	if (onComplete) onComplete(false);
}

//==============================================================================

void AudioEngine::addListener(Listener* l)    { listeners.add(l); }
void AudioEngine::removeListener(Listener* l) { listeners.remove(l); }

void AudioEngine::notifyLoadingStateChanged(int deckIndex, DJAudioPlayer::LoadingState newState)
{
	listeners.call([deckIndex, newState](Listener& l)
	{
		l.deckLoadingStateChanged(deckIndex, newState);
	});
}
