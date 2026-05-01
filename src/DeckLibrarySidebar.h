#pragma once

#include <JuceHeader.h>
#include "Library.h"
#include "Track.h"

//==============================================================================

/**
 * Slide-in sidebar panel that lets a deck pick a track to load (or queue) from
 * the shared Library. One instance per deck, themed with the deck's accent
 * colour. Holds its own folder/track selection state so the two decks' sidebars
 * are independent.
 *
 * The panel renders folders (top section) and tracks of the active folder
 * (middle section), plus an action bar (LOAD / QUEUE) at the bottom and folder
 * CRUD buttons (+ Folder / Rename / − / + Files / − Track / Import Folder).
 *
 * Communicates with the host via three callbacks: onLoad (load track into deck
 * and dismiss), onAddToQueue (append to deck's queue), onClose (dismiss).
 */
class DeckLibrarySidebar : public juce::Component,
                           public juce::TableListBoxModel,
                           public juce::TextEditor::Listener,
                           public juce::Button::Listener,
                           public juce::DragAndDropContainer,
                           public Library::Listener
{
public:
	//==============================================================================

	using TrackCallback = std::function<void(const track&)>;
	using VoidCallback  = std::function<void()>;

	DeckLibrarySidebar(Library& libraryRef,
	                   juce::Colour themeColour,
	                   int deckIndex,
	                   TrackCallback onLoadIn,
	                   TrackCallback onAddToQueueIn,
	                   VoidCallback onCloseIn);

	~DeckLibrarySidebar() override;

	//==============================================================================
	// juce::Component

	void paint(juce::Graphics& g) override;
	void resized() override;

	//==============================================================================
	// Track list (juce::TableListBoxModel)

	int  getNumRows() override;
	void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
	                        bool rowIsSelected) override;
	void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width,
	               int height, bool rowIsSelected) override;
	void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
	void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;
	juce::var getDragSourceDescription(const juce::SparseSet<int>& rowsToDescribe) override;

	//==============================================================================

	void buttonClicked(juce::Button* button) override;
	void textEditorTextChanged(juce::TextEditor& editor) override;

	//==============================================================================
	// Library::Listener

	void libraryChanged() override;

private:
	//==============================================================================

	/// Backing data source.
	Library& library;

	/// Theme colour (deck accent: aqua / hotpink).
	juce::Colour theme;

	/// Owning deck index (0 = left, 1 = right).
	int deckIndex;

	/// Callbacks issued on user actions.
	TrackCallback onLoad;
	TrackCallback onAddToQueue;
	VoidCallback  onClose;

	//==============================================================================
	// UI children

	juce::Label   titleLabel;
	juce::DrawableButton closeBtn { "close", juce::DrawableButton::ImageFitted };

	//==============================================================================
	// Folder list - uses a private ListBoxModel because TableListBoxModel
	// already provides the only getNumRows() in this class.
	struct FolderListModel : public juce::ListBoxModel
	{
		DeckLibrarySidebar& owner;
		explicit FolderListModel(DeckLibrarySidebar& o) : owner(o) {}
		int  getNumRows() override;
		void paintListBoxItem(int rowNumber, juce::Graphics& g,
		                      int width, int height, bool rowIsSelected) override;
		void listBoxItemClicked(int row, const juce::MouseEvent&) override;
	};
	FolderListModel folderListModel { *this };

	juce::ListBox folderList;
	juce::TextButton addFolderBtn   { "+" };
	juce::TextButton renameFolderBtn{ juce::CharPointer_UTF8("\xe2\x9c\x8e") }; // ✎ pencil
	juce::TextButton removeFolderBtn{ juce::CharPointer_UTF8("\xe2\x88\x92") }; // − minus
	juce::TextButton importFolderBtn{ juce::CharPointer_UTF8("\xe2\xac\x87") }; // ⬇ download

	juce::TextEditor searchEditor;
	juce::TableListBox trackList;
	juce::TextButton addFilesBtn   { "+" };
	juce::TextButton removeTrackBtn{ juce::CharPointer_UTF8("\xe2\x88\x92") }; // − minus

	juce::TextButton loadBtn { "LOAD" };
	juce::TextButton queueBtn{ "ADD TO QUEUE" };

	/// Track list row index currently hovered by the mouse (-1 = none).
	int hoveredTrackRow = -1;

	//==============================================================================

	/// @return The content rectangle (full bounds minus rail-clearance on inner edge).
	juce::Rectangle<int> contentBounds() const;

	//==============================================================================

	/// Local active folder index (independent of Library's selection).
	int  selectedFolder = -1;

	/// Local selected track row in the filtered view.
	int  selectedTrackRow = -1;

	/// Lower-cased current search filter; empty = show all.
	juce::String searchFilter;

	/// Indices into the active folder's track vector that match the search.
	std::vector<int> filteredIndices;

	/// Rebuild filteredIndices from the active folder + search filter.
	void rebuildFilter();

	/// Refresh both ListBox and TableListBox visuals.
	void refreshLists();

	/// @return The track at the given row in the filtered view, or empty track.
	track trackForRow(int row) const;

	/// @return The absolute (unfiltered) track index, or -1.
	int absoluteTrackIndex(int row) const;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckLibrarySidebar)
};
