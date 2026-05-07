#pragma once

#include <JuceHeader.h>
#include "../../core/data/Track.h"
#include <deque>

//==============================================================================

/**
 * Compact per-deck queue widget. Shows a scrollable list of upcoming tracks
 * with a tiny row layout (idx | title | M:SS). Click a row to load it into
 * the owning deck (jump-to). Right-click a row for a context menu (remove,
 * move up/down, clear). Accepts drag-and-drop payloads from
 * DeckLibrarySidebar (the sidebar publishes a string descriptor).
 *
 * Auto-advance is implemented by the owning DeckGUI: when a track ends it
 * calls popFront() and loads the result.
 */
class DeckQueue : public juce::Component,
                  public juce::ListBoxModel,
                  public juce::DragAndDropTarget
{
public:
	using TrackCallback = std::function<void(const track&)>;

	/// theme is the deck's accent colour (aqua / hotpink). onJump is called
	/// when the user clicks a queue row to load that track into the deck.
	DeckQueue(juce::Colour theme, TrackCallback onJumpIn);
	~DeckQueue() override = default;

	//==============================================================================

	/// Append a track to the end of the queue.
	void pushBack(const track& t);

	/// Pop and return the front track, or empty if the queue is empty.
	track popFront();

	/// True if the queue is empty.
	bool isEmpty() const { return queue.empty(); }

	/// Number of tracks queued.
	int  size() const { return (int) queue.size(); }

	//==============================================================================
	// juce::Component
	void paint(juce::Graphics& g) override;
	void paintOverChildren(juce::Graphics& g) override;
	void resized() override;

	//==============================================================================
	// juce::ListBoxModel
	int  getNumRows() override;
	void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
	                      bool rowIsSelected) override;
	void listBoxItemClicked(int row, const juce::MouseEvent& e) override;

	//==============================================================================
	// juce::DragAndDropTarget - accepts drops from DeckLibrarySidebar.
	bool isInterestedInDragSource(const SourceDetails& details) override;
	void itemDropped(const SourceDetails& details) override;

	/// Hook from sidebar drag protocol: a callback that resolves a drag
	/// description string (from DeckLibrarySidebar::getDragSourceDescription)
	/// into a fully-populated track. Set by the host (MainComponent).
	std::function<track(const juce::var&)> resolveDragSource;

private:
	juce::Colour theme;
	TrackCallback onJump;
	std::deque<track> queue;
	juce::ListBox list;
	juce::Label headerLabel { "queueHdr", "QUEUE" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckQueue)
};
