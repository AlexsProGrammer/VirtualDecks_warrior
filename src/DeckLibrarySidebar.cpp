#include "UIConstants.h"
#include "CustomLookAndFeel.h"
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
	  titleLabel("title", "Deck " + juce::String(deckIndexIn + 1) + " - Library")
{
	setOpaque(false);

	// Title + close
	titleLabel.setJustificationType(juce::Justification::centredLeft);
	titleLabel.setColour(juce::Label::textColourId, theme);
	titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
	addAndMakeVisible(titleLabel);

	closeBtn.setImages(CustomLookAndFeel::loadIcon(BinaryData::iconClose_svg,
	                                              juce::Colours::white).get());
	closeBtn.setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
	closeBtn.setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
	closeBtn.setTooltip("Close library panel");
	closeBtn.addListener(this);
	addAndMakeVisible(closeBtn);

	// Folder list (top half of left pane on a vertical layout)
	folderList.setModel(&folderListModel);
	folderList.setRowHeight(28);
	folderList.setColour(juce::ListBox::backgroundColourId, UI::bgRoot.darker(0.2f));
	folderList.setColour(juce::ListBox::outlineColourId,    UI::bgCard);
	folderList.setOutlineThickness(1);
	addAndMakeVisible(folderList);

	// Tooltips for compact tool buttons.
	addFolderBtn   .setTooltip("Add folder");
	renameFolderBtn.setTooltip("Rename folder");
	removeFolderBtn.setTooltip("Remove folder");
	importFolderBtn.setTooltip("Import folder from disk");
	addFilesBtn    .setTooltip("Add files to folder");
	removeTrackBtn .setTooltip("Remove selected track");

	for (auto* b : { &addFolderBtn, &renameFolderBtn, &removeFolderBtn, &importFolderBtn,
	                 &addFilesBtn, &removeTrackBtn, &loadBtn, &queueBtn })
	{
		b->addListener(this);
		b->setColour(juce::TextButton::buttonColourId, UI::bgCard);
		b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
		addAndMakeVisible(*b);
	}

	// Primary / secondary action styling.
	loadBtn.setColour(juce::TextButton::buttonColourId, theme.withAlpha(0.9f));
	loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
	loadBtn.setTooltip("Load selected track into the deck");
	queueBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
	queueBtn.setColour(juce::TextButton::textColourOffId, theme.brighter(0.2f));
	queueBtn.setTooltip("Add selected track to the deck's play queue");

	// Search editor
	searchEditor.setTextToShowWhenEmpty("Search...", juce::Colours::grey);
	searchEditor.setColour(juce::TextEditor::backgroundColourId, UI::bgRoot.darker(0.2f));
	searchEditor.setColour(juce::TextEditor::outlineColourId, UI::bgCard);
	searchEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
	searchEditor.addListener(this);
	addAndMakeVisible(searchEditor);

	// Track list
	trackList.setModel(this);
	trackList.setColour(juce::ListBox::backgroundColourId, UI::bgRoot.darker(0.2f));
	trackList.setColour(juce::ListBox::outlineColourId,    UI::bgCard);
	trackList.setOutlineThickness(1);
	trackList.setRowHeight(26);
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
	auto content = contentBounds().toFloat();

	// Panel background sits ONLY inside the content area so the deck's tab
	// rail underneath remains visible.
	CustomLookAndFeel::paintPanelBackground(g, content, true, UI::kPanelRadius);

	// Single deck-coloured accent stripe on the inner edge.
	const float stripW = 3.0f;
	const float stripX = (deckIndex == 0) ? content.getX()
	                                       : content.getRight() - stripW;
	g.setColour(theme);
	g.fillRect(juce::Rectangle<float>(stripX, content.getY() + 6.0f,
	                                  stripW, content.getHeight() - 12.0f));
}

juce::Rectangle<int> DeckLibrarySidebar::contentBounds() const
{
	auto r = getLocalBounds();
	const int railClear = UI::kRailWidth + 4;
	if (deckIndex == 0)
		r.removeFromLeft(railClear);
	else
		r.removeFromRight(railClear);
	return r;
}

void DeckLibrarySidebar::resized()
{
	auto r = contentBounds().reduced(8);

	// Extra inset on the inner edge so the accent stripe reads as accent
	// rather than as a panel border.
	if (deckIndex == 0) r.removeFromLeft(6);
	else                r.removeFromRight(6);

	auto header = r.removeFromTop(28);
	closeBtn.setBounds(header.removeFromRight(28));
	header.removeFromRight(4);
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
	{
		juce::FlexBox fb;
		fb.flexDirection = juce::FlexBox::Direction::row;
		for (auto* b : { &addFolderBtn, &renameFolderBtn, &removeFolderBtn, &importFolderBtn })
			fb.items.add(juce::FlexItem(*b).withFlex(1.0f).withMargin({ 0, 2, 0, 2 }));
		fb.performLayout(folderBtns.toFloat());
	}
	folderList.setBounds(folderArea);

	r.removeFromTop(6);

	// Search bar above track list
	searchEditor.setBounds(r.removeFromTop(24));
	r.removeFromTop(4);

	// Track buttons just below the table
	auto trackBtns = r.removeFromBottom(28);
	{
		juce::FlexBox fb;
		fb.flexDirection = juce::FlexBox::Direction::row;
		for (auto* b : { &addFilesBtn, &removeTrackBtn })
			fb.items.add(juce::FlexItem(*b).withFlex(1.0f).withMargin({ 0, 2, 0, 2 }));
		fb.performLayout(trackBtns.toFloat());
	}

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

	if (rowIsSelected || rowNumber == owner.selectedFolder) {
		g.setColour(th);
		g.fillRect(0, 4, 2, height - 8);
	}

	auto name  = lib.getFolderName(rowNumber);
	auto count = lib.getNumTracksInFolder(rowNumber);

	g.setColour(juce::Colours::white);
	g.setFont(13.0f);
	g.drawText(name, 10, 0, width - 56, height, juce::Justification::centredLeft, true);

	// Track-count pill on the right.
	juce::String countStr(count);
	juce::Rectangle<int> pill(width - 38, height / 2 - 9, 32, 18);
	g.setColour(th.withAlpha(0.25f));
	g.fillRoundedRectangle(pill.toFloat(), 9.0f);
	g.setColour(th.brighter(0.3f));
	g.setFont(11.0f);
	g.drawText(countStr, pill, juce::Justification::centred);
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
	if (rowIsSelected) {
		g.fillAll(theme.withAlpha(0.28f));
		g.setColour(theme);
		g.fillRect(0, 0, 2, height);
	}
	else if ((rowNumber & 1) == 0)
		g.fillAll(UI::bgElevated);
	else
		g.fillAll(juce::Colours::transparentBlack);
}

void DeckLibrarySidebar::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                    int width, int height, bool /*rowIsSelected*/)
{
	auto t = trackForRow(rowNumber);
	g.setFont(12.0f);

	juce::String text;
	juce::Justification just = juce::Justification::centredLeft;
	if (columnId == 1) {
		text = t.title;
		g.setColour(juce::Colours::white);
	}
	else if (columnId == 2) {
		text = juce::String(track::getLengthString(t.lengthInSeconds));
		g.setColour(juce::Colours::lightgrey);
	}
	else if (columnId == 3) {
		text = t.bpm > 0.0 ? juce::String(t.bpm, 1) : juce::String("-");
		g.setColour(t.bpm > 0.0 ? theme.withAlpha(0.85f) : juce::Colours::grey);
		just = juce::Justification::centredRight;
	}

	g.drawText(text, 4, 0, width - 8, height, just, true);
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
	// Encode as "folderIdx|absoluteTrackIdx|identity" - sidebar always wraps
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
