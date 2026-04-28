#include "DeckLibrarySidebar.h"

//==============================================================================

DeckLibrarySidebar::DeckLibrarySidebar(Library& libraryRef,
                                       juce::Colour themeColour,
                                       int deckIndexIn,
                                       TrackCallback onLoadIn,
                                       TrackCallback onAddToQueueIn,
                                       VoidCallback onCloseIn)
	: library(libraryRef),
	  theme(themeColour),
	  deckIndex(deckIndexIn),
	  onLoad(std::move(onLoadIn)),
	  onAddToQueue(std::move(onAddToQueueIn)),
	  onClose(std::move(onCloseIn)),
	  titleLabel("title", "Deck " + juce::String(deckIndexIn + 1) + " — Library")
{
	setOpaque(false);

	// Title + close
	titleLabel.setJustificationType(juce::Justification::centredLeft);
	titleLabel.setColour(juce::Label::textColourId, theme);
	titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
	addAndMakeVisible(titleLabel);

	closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(50, 50, 50, 255));
	closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	closeBtn.addListener(this);
	addAndMakeVisible(closeBtn);

	// Folder list (top half of left pane on a vertical layout)
	folderList.setModel(&folderListModel);
	folderList.setRowHeight(22);
	folderList.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGBA(20, 20, 20, 255));
	folderList.setColour(juce::ListBox::outlineColourId,    juce::Colour::fromRGBA(50, 50, 50, 255));
	folderList.setOutlineThickness(1);
	addAndMakeVisible(folderList);

	for (auto* b : { &addFolderBtn, &renameFolderBtn, &removeFolderBtn, &importFolderBtn,
	                 &addFilesBtn, &removeTrackBtn, &loadBtn, &queueBtn })
	{
		b->addListener(this);
		b->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(50, 50, 50, 255));
		b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		addAndMakeVisible(*b);
	}

	loadBtn.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.9f));
	loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
	queueBtn.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.45f));

	// Search editor
	searchEditor.setTextToShowWhenEmpty("Search...", juce::Colours::grey);
	searchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGBA(20, 20, 20, 255));
	searchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGBA(50, 50, 50, 255));
	searchEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
	searchEditor.addListener(this);
	addAndMakeVisible(searchEditor);

	// Track list
	trackList.setModel(this);
	trackList.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGBA(20, 20, 20, 255));
	trackList.setColour(juce::ListBox::outlineColourId,    juce::Colour::fromRGBA(50, 50, 50, 255));
	trackList.setOutlineThickness(1);
	trackList.setRowHeight(22);
	trackList.getHeader().addColumn("Title",  1, 220, 60, -1, juce::TableHeaderComponent::defaultFlags);
	trackList.getHeader().addColumn("Length", 2,  60, 40, 100, juce::TableHeaderComponent::defaultFlags);
	trackList.getHeader().addColumn("BPM",    3,  50, 30, 80,  juce::TableHeaderComponent::defaultFlags);
	addAndMakeVisible(trackList);

	library.addListener(this);

	// Initial state: open first folder if any.
	if (library.getNumFolders() > 0)
		selectedFolder = 0;
	rebuildFilter();
	refreshLists();
}

DeckLibrarySidebar::~DeckLibrarySidebar()
{
	library.removeListener(this);
}

//==============================================================================

void DeckLibrarySidebar::paint(juce::Graphics& g)
{
	auto r = getLocalBounds().toFloat();
	g.setColour(juce::Colour::fromRGBA(28, 28, 28, 240));
	g.fillRect(r);
	g.setColour(theme.withAlpha(0.7f));
	g.drawRect(r, 1.5f);
}

void DeckLibrarySidebar::resized()
{
	auto r = getLocalBounds().reduced(8);

	auto header = r.removeFromTop(28);
	closeBtn.setBounds(header.removeFromRight(28));
	titleLabel.setBounds(header);

	r.removeFromTop(6);

	// Bottom action bar
	auto actionBar = r.removeFromBottom(36);
	loadBtn .setBounds(actionBar.removeFromLeft(actionBar.getWidth() / 2).reduced(2));
	queueBtn.setBounds(actionBar.reduced(2));

	r.removeFromBottom(6);

	// Folder section: top ~40% of the remaining area
	int folderHeight = juce::jmax(120, r.getHeight() * 4 / 10);
	auto folderArea = r.removeFromTop(folderHeight);

	auto folderBtns = folderArea.removeFromBottom(28);
	addFolderBtn   .setBounds(folderBtns.removeFromLeft(folderBtns.getWidth() / 4).reduced(2));
	renameFolderBtn.setBounds(folderBtns.removeFromLeft(folderBtns.getWidth() / 3).reduced(2));
	removeFolderBtn.setBounds(folderBtns.removeFromLeft(folderBtns.getWidth() / 2).reduced(2));
	importFolderBtn.setBounds(folderBtns.reduced(2));
	folderList.setBounds(folderArea);

	r.removeFromTop(6);

	// Search bar above track list
	searchEditor.setBounds(r.removeFromTop(24));
	r.removeFromTop(4);

	// Track buttons just below the table
	auto trackBtns = r.removeFromBottom(28);
	addFilesBtn   .setBounds(trackBtns.removeFromLeft(trackBtns.getWidth() / 2).reduced(2));
	removeTrackBtn.setBounds(trackBtns.reduced(2));

	trackList.setBounds(r);
}

//==============================================================================
// Folder list

int DeckLibrarySidebar::FolderListModel::getNumRows()
{
	return owner.library.getNumFolders();
}

void DeckLibrarySidebar::FolderListModel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                                            int width, int height, bool rowIsSelected)
{
	auto& lib = owner.library;
	auto  th  = owner.theme;
	if (rowNumber < 0 || rowNumber >= lib.getNumFolders())
		return;

	if (rowIsSelected)
		g.fillAll(th.withAlpha(0.35f));
	else if (rowNumber == owner.selectedFolder)
		g.fillAll(th.withAlpha(0.18f));

	auto name  = lib.getFolderName(rowNumber);
	auto count = lib.getNumTracksInFolder(rowNumber);

	g.setColour(juce::Colours::white);
	g.setFont(13.0f);
	g.drawText(name, 8, 0, width - 50, height, juce::Justification::centredLeft, true);
	g.setColour(juce::Colours::lightgrey);
	g.setFont(11.0f);
	g.drawText(juce::String(count), width - 40, 0, 32, height, juce::Justification::centredRight);
}

void DeckLibrarySidebar::FolderListModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
	if (row < 0 || row >= owner.library.getNumFolders())
		return;
	owner.selectedFolder = row;
	owner.selectedTrackRow = -1;
	owner.library.setActiveFolder(row);
	owner.rebuildFilter();
	owner.refreshLists();
}

//==============================================================================
// Track list

int DeckLibrarySidebar::getNumRows()
{
	return (int) filteredIndices.size();
}

void DeckLibrarySidebar::paintRowBackground(juce::Graphics& g, int rowNumber,
                                             int width, int height, bool rowIsSelected)
{
	if (rowIsSelected)
		g.fillAll(theme.withAlpha(0.35f));
	else if ((rowNumber & 1) == 0)
		g.fillAll(juce::Colour::fromRGBA(30, 30, 30, 255));
	else
		g.fillAll(juce::Colour::fromRGBA(25, 25, 25, 255));
}

void DeckLibrarySidebar::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                    int width, int height, bool /*rowIsSelected*/)
{
	auto t = trackForRow(rowNumber);
	g.setColour(juce::Colours::white);
	g.setFont(12.0f);

	juce::String text;
	if (columnId == 1)
		text = t.title;
	else if (columnId == 2)
		text = juce::String(track::getLengthString(t.lengthInSeconds));
	else if (columnId == 3)
		text = t.bpm > 0.0 ? juce::String(t.bpm, 1) : juce::String("—");

	g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
}

void DeckLibrarySidebar::cellClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& e)
{
	if (rowNumber < 0 || rowNumber >= getNumRows())
		return;
	selectedTrackRow = rowNumber;

	if (e.mods.isPopupMenu())
	{
		juce::PopupMenu menu;
		menu.addItem(1, "Load to deck");
		menu.addItem(2, "Add to queue");
		menu.addSeparator();
		menu.addItem(3, "Remove track");
		auto trk = trackForRow(rowNumber);
		auto absIdx = absoluteTrackIndex(rowNumber);
		auto folder = selectedFolder;
		menu.showMenuAsync(juce::PopupMenu::Options(),
			[this, trk, absIdx, folder](int result)
			{
				if (result == 1) { if (onLoad)        onLoad(trk); }
				else if (result == 2) { if (onAddToQueue) onAddToQueue(trk); }
				else if (result == 3) { library.removeTrackAt(folder, absIdx); }
			});
	}
}

void DeckLibrarySidebar::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&)
{
	if (rowNumber < 0 || rowNumber >= getNumRows())
		return;
	if (onLoad)
		onLoad(trackForRow(rowNumber));
}

juce::var DeckLibrarySidebar::getDragSourceDescription(const juce::SparseSet<int>& rowsToDescribe)
{
	if (rowsToDescribe.isEmpty())
		return {};
	int row = rowsToDescribe[0];
	if (row < 0 || row >= getNumRows())
		return {};
	auto t = trackForRow(row);
	// Encode as "folderIdx|absoluteTrackIdx|identity" — sidebar always wraps
	// the source identity so receivers can re-fetch the live track from
	// Library if needed.
	return juce::String(selectedFolder) + "|" + juce::String(absoluteTrackIndex(row)) + "|" + t.identity;
}

//==============================================================================

void DeckLibrarySidebar::buttonClicked(juce::Button* button)
{
	if (button == &closeBtn)
	{
		if (onClose) onClose();
		return;
	}

	if (button == &addFolderBtn)        { library.addFolder();          return; }
	if (button == &importFolderBtn)     { library.importFolderFromDisk(); return; }

	// All folder mutators below operate on the currently-selected folder
	// inside Library. Make our local selection canonical first.
	if (selectedFolder >= 0)
		library.setActiveFolder(selectedFolder);

	if (button == &renameFolderBtn)     { library.renameFolder();       return; }
	if (button == &removeFolderBtn)
	{
		library.removeFolder();
		selectedFolder = juce::jlimit(-1, library.getNumFolders() - 1, selectedFolder);
		if (selectedFolder < 0 && library.getNumFolders() > 0)
			selectedFolder = 0;
		if (selectedFolder >= 0)
			library.setActiveFolder(selectedFolder);
		rebuildFilter();
		refreshLists();
		return;
	}
	if (button == &addFilesBtn)         { library.addFilesToFolder();   return; }

	if (button == &removeTrackBtn && selectedFolder >= 0 && selectedTrackRow >= 0)
	{
		library.removeTrackAt(selectedFolder, absoluteTrackIndex(selectedTrackRow));
		selectedTrackRow = -1;
		return;
	}

	if (button == &loadBtn && selectedTrackRow >= 0)
	{
		if (onLoad) onLoad(trackForRow(selectedTrackRow));
		return;
	}

	if (button == &queueBtn && selectedTrackRow >= 0)
	{
		if (onAddToQueue) onAddToQueue(trackForRow(selectedTrackRow));
		return;
	}
}

void DeckLibrarySidebar::textEditorTextChanged(juce::TextEditor& editor)
{
	if (&editor != &searchEditor) return;
	searchFilter = editor.getText().toLowerCase();
	rebuildFilter();
	trackList.updateContent();
	trackList.repaint();
}

//==============================================================================

void DeckLibrarySidebar::libraryChanged()
{
	// Re-clamp selection in case folders were removed.
	if (selectedFolder >= library.getNumFolders())
		selectedFolder = library.getNumFolders() - 1;
	rebuildFilter();
	refreshLists();
}

//==============================================================================

void DeckLibrarySidebar::rebuildFilter()
{
	filteredIndices.clear();
	if (selectedFolder < 0)
		return;
	int n = library.getNumTracksInFolder(selectedFolder);
	filteredIndices.reserve((size_t) n);
	for (int i = 0; i < n; ++i)
	{
		if (searchFilter.isEmpty())
			filteredIndices.push_back(i);
		else
		{
			auto t = library.getTrack(selectedFolder, i);
			if (t.title.toLowerCase().contains(searchFilter))
				filteredIndices.push_back(i);
		}
	}
	if (selectedTrackRow >= (int) filteredIndices.size())
		selectedTrackRow = -1;
}

void DeckLibrarySidebar::refreshLists()
{
	folderList.updateContent();
	folderList.repaint();
	trackList.updateContent();
	trackList.repaint();
}

track DeckLibrarySidebar::trackForRow(int row) const
{
	if (selectedFolder < 0 || row < 0 || row >= (int) filteredIndices.size())
		return {};
	return library.getTrack(selectedFolder, filteredIndices[(size_t) row]);
}

int DeckLibrarySidebar::absoluteTrackIndex(int row) const
{
	if (row < 0 || row >= (int) filteredIndices.size())
		return -1;
	return filteredIndices[(size_t) row];
}
