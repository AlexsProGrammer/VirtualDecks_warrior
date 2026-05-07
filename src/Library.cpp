
#include "UIConstants.h"
#include "Library.h"

//==============================================================================

/**
 * Implementation of a constructor for Library
 *
 * Data members are initialized and initial configurations are applied to
 * components here. File reading occurs from a fixed path defined in the header
 * file. The retrieved value tree from reading off the xml file is used to
 * populate the elements of the trackFolders data structure.
 *
 */
Library::Library(juce::AudioFormatManager &_formatManager)
    : formatManager(_formatManager), playlist(_formatManager),
      bpmAnalysisManager(_formatManager) {
  filePath = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                 .getChildFile(".otodecks")
                 .getChildFile("Resource.xml")
                 .getFullPathName();

  juce::File file(filePath);
  if (!file.getParentDirectory().exists()) {
    file.getParentDirectory().createDirectory();
  }
  if (!file.existsAsFile()) {
    DBG("FILE DONT EXIST");
    DBG((file.create().wasOk() ? "Creation Success" : "Creation Failed"));
    std::pair<juce::String, std::vector<track>> folder;
    folder.first = "Main";
    trackFolders.push_back(folder);
  } else {
    DBG("FILE EXIST");
    juce::FileInputStream in(file);
    if (in.openedOk()) {
      auto newValueTree = juce::ValueTree::readFromStream(in);
      for (auto i = 0; i < newValueTree.getNumChildren(); ++i) {
        std::pair<juce::String, std::vector<track>> folder;
        folder.first = newValueTree.getChild(i).getProperty("name");
        for (auto j = 0; j < newValueTree.getChild(i).getNumChildren(); ++j) {
          auto song = newValueTree.getChild(i).getChild(j);
          track refSong{song.getProperty("title"), song.getProperty("length"),
                        juce::URL(song.getProperty("url").toString()),
                        song.getProperty("identity")};
          // Read fileHash and bpm if present (new fields)
          if (song.hasProperty("fileHash"))
            refSong.fileHash = song.getProperty("fileHash").toString();
          if (song.hasProperty("bpm"))
            refSong.bpm = static_cast<double>(song.getProperty("bpm"));
          folder.second.push_back(refSong);
        }
        trackFolders.push_back(folder);
      }
    }
  }

  if (trackFolders.empty()) {
    std::pair<juce::String, std::vector<track>> folder;
    folder.first = "Main";
    trackFolders.push_back(folder);
  }

  selectedFolderIndex = 0;
  playlist.setTrackTitles(trackFolders[selectedFolderIndex].second);
  addAndMakeVisible(playlist);
  playlist.setLookAndFeel(&customLookAndFeel);

  directoryComponent.getHeader().addColumn("Folders", 1, 360);
  directoryComponent.setModel(this);
  addAndMakeVisible(directoryComponent);
  directoryComponent.setColour(juce::ListBox::ColourIds::backgroundColourId,
                               UI::bgRoot);
  directoryComponent.selectRow(selectedFolderIndex);

  // Setup folder management buttons
  addAndMakeVisible(addFolderBtn);
  addAndMakeVisible(removeFolderBtn);
  addAndMakeVisible(renameFolderBtn);
  addAndMakeVisible(addFilesBtn);
  addAndMakeVisible(removeTrackBtn);

  auto buttonColour = UI::bgCard;
  addFolderBtn.setColour(juce::TextButton::buttonColourId, buttonColour);
  removeFolderBtn.setColour(juce::TextButton::buttonColourId, buttonColour);
  renameFolderBtn.setColour(juce::TextButton::buttonColourId, buttonColour);
  addFilesBtn.setColour(juce::TextButton::buttonColourId, buttonColour);
  removeTrackBtn.setColour(juce::TextButton::buttonColourId, buttonColour);

  addAndMakeVisible(importFolderBtn);
  importFolderBtn.setColour(juce::TextButton::buttonColourId, buttonColour);

  // Progress strip - hidden until an ingest job is running.
  addAndMakeVisible(ingestProgressBar);
  ingestProgressBar.setJustificationType(juce::Justification::centredLeft);
  ingestProgressBar.setColour(juce::Label::backgroundColourId,
                              juce::Colour::fromRGBA(0, 0, 0, 200));
  ingestProgressBar.setColour(juce::Label::textColourId, juce::Colours::white);
  ingestProgressBar.setFont(juce::Font(13.0f, juce::Font::bold));
  ingestProgressBar.setVisible(false);
  ingestProgressBar.setInterceptsMouseClicks(false, false);

  addFolderBtn.addListener(this);
  removeFolderBtn.addListener(this);
  renameFolderBtn.addListener(this);
  addFilesBtn.addListener(this);
  removeTrackBtn.addListener(this);
  importFolderBtn.addListener(this);

  bpmAnalysisManager.addListener(this);

  // 1. Build the snapshot synchronously (we are already on the Message Thread!)
  struct Pending { juce::String identity; juce::File file; };
  std::vector<Pending> pending;
  
  for (auto& folder : trackFolders) {
    for (auto& t : folder.second) {
      if (t.fileHash.isEmpty()) {
        pending.push_back({ t.identity, t.url.getLocalFile() });
      }
    }
  }

  // 2. Defer hashing + BPM dispatch onto a background thread
  // Pass the 'pending' vector into the lambda by value using std::move
  juce::Thread::launch([this, pending = std::move(pending)]() {
    
    // Hash off-thread.
    for (auto& p : pending) {
      if (! p.file.existsAsFile())
        continue;
        
      auto hash = FileHasher::computeHash(p.file);
      if (hash.isEmpty())
        continue;
        
      auto identity = p.identity;
      juce::MessageManager::callAsync([this, identity, hash]() {
        for (auto& folder : trackFolders) {
          for (auto& t : folder.second) {
            if (t.identity == identity && t.fileHash.isEmpty()) {
              t.fileHash = hash;
            }
          }
        }
      });
    }

    // Dispatch BPM analysis once hashing is broadly settled.
    juce::MessageManager::callAsync([this]() {
      for (auto& folder : trackFolders) {
        queueBpmAnalysis(folder.second);
      }
    });
  });
}

/**
 * Implementation of a destructor for Library
 *
 * The trackFolders data structure is used to populate a value tree before
 * writing it to the xml file at the same path.
 *
 */
Library::~Library() {
  bpmAnalysisManager.removeListener(this);

  // Stop coalescing timer first so it cannot dispatch a new save mid-shutdown.
  if (saveDebounceTimer != nullptr)
    saveDebounceTimer->stopTimer();

  // Drain any in-flight ingest jobs (will respect shouldExit()).
  ingestPool.removeAllJobs(true, 5000);

  // Wait for any pending background save to flush.
  savePool.removeAllJobs(true, 5000);

  // Final synchronous save so the latest state hits disk.
  juce::File file(filePath);
  file.deleteFile();
  file.create();
  juce::ValueTree tree = buildPersistenceTree();
  Library::persistTreeToDisk(tree, filePath);
  DBG("FILE SAVED (shutdown)");
}

//==============================================================================

/**
 * Implementation of selectionIsValid method for Library
 *
 * Returns if the folder and track selection is valid.
 * This is ensured by checking that the selectedFolderIndex respect the bounds
 * of the trackFolders size and that the playlist track is selected
 *
 */
bool Library::selectionIsValid() {
  DBG((playlist.trackIsSelected() ? "true" : "3false"));
  DBG((selectedFolderIndex >= 0 ? "true" : "1false"));
  DBG((selectedFolderIndex < trackFolders.size() ? "true" : "2false"));
  return selectedFolderIndex >= 0 &&
         selectedFolderIndex < trackFolders.size() &&
         playlist.trackIsSelected();
};

/**
 * Implementation of getSelectedTrack method for Library
 *
 * Returns the selected track from the playlist instance
 *
 */
track Library::getSelectedTrack() { return playlist.getSelectedTrack(); };

/**
 * Implementation of deleteItem method for Library
 *
 * Check if only folder is selected or both folder and track is selected.
 * In the former case, the entire folder element is erased off the trackFolders
 * data structure. In the latter case, the track element is erased off the
 * selected folder in trackFolders. Identity hash strings of tracks are compared
 * to confirm the track to be deleted.
 *
 */
void Library::deleteItem() {
  if (selectedFolderIndex >= 0 && selectedFolderIndex < trackFolders.size()) {
    if (playlist.trackIsSelected()) {
      auto &selectedPlaylist = trackFolders[selectedFolderIndex].second;
      auto selectedTrack = -1;
      for (auto i = 0; i < selectedPlaylist.size(); ++i) {
        if (selectedPlaylist[i].identity == getSelectedTrack().identity) {
          DBG("True delete match");
          selectedTrack = i;
          break;
        }
      }
      if (selectedTrack > -1) {
        selectedPlaylist.erase(selectedPlaylist.begin() + selectedTrack);
      }
      playlist.setTrackTitles(trackFolders[selectedFolderIndex].second);
    } else {
      if (trackFolders.size() > 1) {
        trackFolders.erase(trackFolders.begin() + selectedFolderIndex);
        selectedFolderIndex = 0;
        playlist.setTrackTitles(trackFolders[selectedFolderIndex].second);
        directoryComponent.selectRow(selectedFolderIndex);
      }
      directoryComponent.updateContent();
    }
  }
};

//==============================================================================

/**
 * Implementation of paint method for Library
 */
void Library::paint(juce::Graphics &g) {}

/**
 * Implementation of resized method for Library
 *
 * Call setBounds method on the juce::Component data members playlist and
 * directoryComponent.
 */
void Library::resized() {
  auto buttonBarHeight = 28;
  auto dirWidth = 1.5 * getWidth() / 8;
  auto playlistX = dirWidth;
  auto playlistWidth = getWidth() - dirWidth;
  auto contentHeight = getHeight() - buttonBarHeight;

  directoryComponent.setBounds(0, 0, dirWidth, contentHeight);

  if (selectedFolderIndex != -1) {
    playlist.setBounds(playlistX, 0, playlistWidth, contentHeight);
  }

  // Folder buttons below the directory list
  auto folderBtnWidth = dirWidth / 4;
  addFolderBtn.setBounds(0, contentHeight, folderBtnWidth, buttonBarHeight);
  removeFolderBtn.setBounds(folderBtnWidth, contentHeight, folderBtnWidth, buttonBarHeight);
  renameFolderBtn.setBounds(2 * folderBtnWidth, contentHeight, folderBtnWidth, buttonBarHeight);
  importFolderBtn.setBounds(3 * folderBtnWidth, contentHeight, dirWidth - 3 * folderBtnWidth, buttonBarHeight);

  // Track buttons below the playlist
  auto trackBtnWidth = playlistWidth / 2;
  addFilesBtn.setBounds(playlistX, contentHeight, trackBtnWidth, buttonBarHeight);
  removeTrackBtn.setBounds(playlistX + trackBtnWidth, contentHeight, playlistWidth - trackBtnWidth, buttonBarHeight);

  // Progress strip overlays the bottom button bar across full width while
  // an ingest job is running.
  ingestProgressBar.setBounds(0, contentHeight, getWidth(), buttonBarHeight);
}

//==============================================================================

/**
 * Implementation of getNumRows method for Library
 *
 * Returns the size of the data structure trackFolders
 */
int Library::getNumRows() { return trackFolders.size(); };

/**
 * Implementation of paintRowBackground method for Library
 *
 * Change the colour of the row if they are selected.
 */
void Library::paintRowBackground(juce::Graphics &g, int rowNumber, int width,
                                 int height, bool rowIsSelected) {
  if (rowNumber < trackFolders.size()) {
    if (rowIsSelected) {
      g.fillAll(juce::Colour::fromRGBA(0, 125, 225, 255));
    } else {
      g.fillAll(juce::Colour::fromRGBA(100, 100, 100, 255));
    }
  }
};

/**
 * Implementation of paintCell method for Library
 *
 * Draw the text of the folder names on the rows
 *
 */
void Library::paintCell(juce::Graphics &g, int rowNumber, int columnId,
                        int width, int height, bool rowIsSelected) {
  g.setColour(juce::Colours::white);
  if (rowNumber < trackFolders.size()) {
    g.drawText(trackFolders[rowNumber].first, 2, 0, width - 4, height,
               juce::Justification::centredLeft, true);
  }
};

/**
 * Implementation of cellClicked method for Library
 *
 * Sets the selected folder index
 * Sets the playlist folder with the selected folder
 *
 */
void Library::cellClicked(int rowNumber, int columnId,
                          const juce::MouseEvent &e) {
  DBG(" PlaylistComponent::cellClicked " << rowNumber);
  selectedFolderIndex = rowNumber;
  playlist.setTrackTitles(trackFolders[selectedFolderIndex].second);

  if (e.mods.isPopupMenu()) {
    juce::PopupMenu menu;
    menu.addItem(1, "Add Folder");
    menu.addItem(2, "Rename Folder");
    menu.addItem(3, "Remove Folder", trackFolders.size() > 1);
    menu.addSeparator();
    menu.addItem(4, "Add Files to Folder");

    menu.showMenuAsync(juce::PopupMenu::Options(),
      [this](int result) {
        if (result == 1)
          addFolder();
        else if (result == 2)
          renameFolder();
        else if (result == 3)
          removeFolder();
        else if (result == 4)
          addFilesToFolder();
      });
  }
};

//==============================================================================

/**
 * Implementation of isInterestedInFileDrag method for Library
 *
 * Returns true
 *
 */
bool Library::isInterestedInFileDrag(const juce::StringArray &files) {
  return true;
};

/**
 * Implementation of filesDropped method for Library
 *
 * Checks if the dropped items are in the library or playlist components.
 * The addition of tracks/folders are performed on the trackFolders data
 * structure, storing all the folder/track data in the library level. Selection
 * of the folder and what the playlist displays is communicated to the playlist
 * instance using the trackFolders data. Adds tracks into the currently selected
 * folder if items are dropped on the playlist component. Adds folder of tracks
 * into the library if items are dropped on the library component.
 *
 */
void Library::filesDropped(const juce::StringArray &files, int x, int y) {
  // Drop on playlist area => add to currently selected folder.
  // Drop on directory area => create new folder per dropped directory and
  // import its contents.
  if (x > 1.5 * getWidth() / 8) {
    if (selectedFolderIndex < 0)
      return;

    juce::Array<juce::File> audioFiles;
    for (auto i = 0; i < files.size(); ++i) {
      juce::File f { files[i] };
      if (f.isDirectory()) {
        for (auto& child : f.findChildFiles(juce::File::TypesOfFileToFind::findFiles, false))
          audioFiles.add(child);
      } else {
        audioFiles.add(f);
      }
    }
    enqueueIngest(audioFiles, selectedFolderIndex, {});
  } else {
    for (auto i = 0; i < files.size(); ++i) {
      juce::File audioFile { files[i] };
      if (audioFile.isDirectory()) {
        juce::Array<juce::File> contents = audioFile.findChildFiles(
            juce::File::TypesOfFileToFind::findFiles, false);
        enqueueIngest(contents, -1, audioFile.getFileNameWithoutExtension());
      }
    }
  }
}

//==============================================================================

/**
 * Implementation of buttonClicked method for Library
 *
 * Routes button clicks to the appropriate action method.
 */
void Library::buttonClicked(juce::Button *button) {
  if (button == &addFolderBtn)
    addFolder();
  else if (button == &removeFolderBtn)
    removeFolder();
  else if (button == &renameFolderBtn)
    renameFolder();
  else if (button == &addFilesBtn)
    addFilesToFolder();
  else if (button == &removeTrackBtn)
    removeSelectedTrack();
  else if (button == &importFolderBtn)
    importFolderFromDisk();
}

//==============================================================================

/**
 * Implementation of addFolder method for Library
 *
 * Prompts user for a folder name and creates a new empty folder.
 */
void Library::addFolder() {
  auto *editor = new juce::AlertWindow("New Folder", "Enter folder name:",
                                       juce::MessageBoxIconType::NoIcon);
  editor->addTextEditor("name", "New Folder", "Folder Name:");
  editor->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
  editor->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

  editor->enterModalState(true, juce::ModalCallbackFunction::create(
    [this, editor](int result) {
      if (result == 1) {
        auto name = editor->getTextEditorContents("name").trim();
        if (name.isNotEmpty()) {
          std::pair<juce::String, std::vector<track>> folder;
          folder.first = name;
          trackFolders.push_back(folder);
          selectedFolderIndex = static_cast<int>(trackFolders.size()) - 1;
          directoryComponent.updateContent();
          directoryComponent.selectRow(selectedFolderIndex);
          playlist.setTrackTitles(trackFolders[selectedFolderIndex].second);
          scheduleAsyncSave();
        }
      }
      delete editor;
    }), true);
}

/**
 * Implementation of removeFolder method for Library
 *
 * Removes the currently selected folder (if more than one exists).
 */
void Library::removeFolder() {
  if (selectedFolderIndex >= 0 &&
      selectedFolderIndex < static_cast<int>(trackFolders.size()) &&
      trackFolders.size() > 1) {
    trackFolders.erase(trackFolders.begin() + selectedFolderIndex);
    selectedFolderIndex = 0;
    directoryComponent.updateContent();
    directoryComponent.selectRow(selectedFolderIndex);
    playlist.setTrackTitles(trackFolders[selectedFolderIndex].second);
    scheduleAsyncSave();
  }
}

/**
 * Implementation of renameFolder method for Library
 *
 * Prompts user for a new name for the selected folder.
 */
void Library::renameFolder() {
  if (selectedFolderIndex < 0 ||
      selectedFolderIndex >= static_cast<int>(trackFolders.size()))
    return;

  auto currentName = trackFolders[selectedFolderIndex].first;
  auto *editor = new juce::AlertWindow("Rename Folder", "Enter new name:",
                                       juce::MessageBoxIconType::NoIcon);
  editor->addTextEditor("name", currentName, "Folder Name:");
  editor->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
  editor->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

  editor->enterModalState(true, juce::ModalCallbackFunction::create(
    [this, editor](int result) {
      if (result == 1) {
        auto name = editor->getTextEditorContents("name").trim();
        if (name.isNotEmpty() && selectedFolderIndex >= 0 &&
            selectedFolderIndex < static_cast<int>(trackFolders.size())) {
          trackFolders[selectedFolderIndex].first = name;
          directoryComponent.updateContent();
          directoryComponent.repaint();
          scheduleAsyncSave();
        }
      }
      delete editor;
    }), true);
}

/**
 * Implementation of addFilesToFolder method for Library
 *
 * Opens a file chooser dialog allowing multi-selection of audio files,
 * then adds them to the currently selected folder.
 */
void Library::addFilesToFolder() {
  if (selectedFolderIndex < 0 ||
      selectedFolderIndex >= static_cast<int>(trackFolders.size()))
    return;

  fileChooser = std::make_unique<juce::FileChooser>(
      "Select audio files to add",
      juce::File::getSpecialLocation(juce::File::userHomeDirectory),
      formatManager.getWildcardForAllFormats());

  fileChooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles |
          juce::FileBrowserComponent::canSelectMultipleItems,
      [this](const juce::FileChooser &chooser) {
        auto results = chooser.getResults();
        if (results.isEmpty())
          return;
        if (selectedFolderIndex < 0 ||
            selectedFolderIndex >= static_cast<int>(trackFolders.size()))
          return;
        enqueueIngest(results, selectedFolderIndex, {});
      });
}

/**
 * Implementation of importFolderFromDisk method for Library
 *
 * Opens a directory chooser, scans the selected folder for audio files,
 * and adds them as a new library folder.
 */
void Library::importFolderFromDisk() {
  fileChooser = std::make_unique<juce::FileChooser>(
      "Select a folder to import",
      juce::File::getSpecialLocation(juce::File::userHomeDirectory));

  fileChooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectDirectories,
      [this](const juce::FileChooser &chooser) {
        auto result = chooser.getResult();
        if (!result.isDirectory())
          return;

        auto childFiles = result.findChildFiles(
            juce::File::TypesOfFileToFind::findFiles, false,
            formatManager.getWildcardForAllFormats());

        if (childFiles.isEmpty())
          return;

        enqueueIngest(childFiles, -1, result.getFileNameWithoutExtension());
      });
}

/**
 * Implementation of removeSelectedTrack method for Library
 *
 * Removes the currently selected track from the current folder.
 */
void Library::removeSelectedTrack() {
  if (selectedFolderIndex < 0 ||
      selectedFolderIndex >= static_cast<int>(trackFolders.size()))
    return;

  if (!playlist.trackIsSelected())
    return;

  auto selectedTrackObj = playlist.getSelectedTrack();
  auto &selectedPlaylist = trackFolders[selectedFolderIndex].second;
  for (auto it = selectedPlaylist.begin(); it != selectedPlaylist.end(); ++it) {
    if (it->identity == selectedTrackObj.identity) {
      selectedPlaylist.erase(it);
      break;
    }
  }
  playlist.setTrackTitles(trackFolders[selectedFolderIndex].second);
  scheduleAsyncSave();
}

//==============================================================================

/**
 * Implementation of queueBpmAnalysis method for Library
 *
 * Queues background BPM analysis for tracks that don't yet have a BPM value.
 * Computes fileHash on-the-fly if missing.
 */
void Library::queueBpmAnalysis(std::vector<track>& tracks)
{
	DBG("Library::queueBpmAnalysis - " + juce::String(static_cast<int>(tracks.size())) + " tracks");
	for (auto& t : tracks)
	{
		// Compute fileHash if not yet set
		if (t.fileHash.isEmpty())
		{
			juce::File audioFile = t.url.getLocalFile();
			DBG("  Computing hash for: " + t.title + " file=" + audioFile.getFullPathName() + " exists=" + juce::String(audioFile.existsAsFile() ? "yes" : "no"));
			if (audioFile.existsAsFile())
				t.fileHash = FileHasher::computeHash(audioFile);
		}

		// Queue analysis if BPM is unknown
		if (t.bpm <= 0.0 && t.fileHash.isNotEmpty())
		{
			juce::File audioFile = t.url.getLocalFile();
			bpmAnalysisManager.analyzeTrack(audioFile, t.fileHash);
		}
	}
}

/**
 * Implementation of bpmAnalysisComplete callback for Library
 *
 * Called on the message thread when a background BPM analysis finishes.
 * Updates all tracks with the matching fileHash and refreshes the playlist.
 */
void Library::bpmAnalysisComplete(const juce::String& fileHash, double bpm)
{
	DBG("Library::bpmAnalysisComplete - hash=" + fileHash + " bpm=" + juce::String(bpm));
	bool updated = false;
	for (auto& folder : trackFolders)
	{
		for (auto& t : folder.second)
		{
			if (t.fileHash == fileHash && t.bpm <= 0.0)
			{
				t.bpm = bpm;
				updated = true;
			}
		}
	}

	if (updated && selectedFolderIndex >= 0 &&
		selectedFolderIndex < static_cast<int>(trackFolders.size()))
	{
		playlist.setTrackTitles(trackFolders[selectedFolderIndex].second);
	}

	if (updated)
		scheduleAsyncSave();
}

//==============================================================================
// Phase 2 - Off-thread library ingestion

/**
 * Background ingest job. Builds completed track records (probing duration via
 * a thread-local AudioFormatManager, computing FileHasher hash, looking up
 * cached BPM) and streams them back to the message thread in small batches
 * so the UI can update progressively.
 */
class Library::LibraryIngestJob : public juce::ThreadPoolJob
{
public:
	LibraryIngestJob(Library& ownerRef,
	                 juce::Array<juce::File> filesToScan,
	                 int targetFolderIdx,
	                 juce::String newFolderNameIn)
		: juce::ThreadPoolJob("LibraryIngest"),
		  owner(ownerRef),
		  files(std::move(filesToScan)),
		  targetFolderIndex(targetFolderIdx),
		  newFolderName(std::move(newFolderNameIn))
	{}

	juce::ThreadPoolJob::JobStatus runJob() override
	{
		// Use a thread-local AudioFormatManager: the shared one is not
		// guaranteed thread-safe for concurrent createReaderFor() calls.
		juce::AudioFormatManager localFormatManager;
		localFormatManager.registerBasicFormats();

		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);
		std::ostringstream oss;
		oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
		const std::string timeString = oss.str();

		std::vector<track> batch;
		const int batchSize     = 8;
		const int totalCount    = files.size();
		int       processedSoFar = 0;
		std::hash<std::string> hasher;

		for (auto& audioFile : files)
		{
			if (shouldExit())
				break;

			++processedSoFar;
			juce::String currentName = audioFile.getFileName();

			std::unique_ptr<juce::AudioFormatReader> reader(
				localFormatManager.createReaderFor(audioFile));
			if (reader == nullptr)
			{
				notifyProgressOnly(processedSoFar, totalCount, currentName);
				continue;
			}

			track t {
				audioFile.getFileNameWithoutExtension(),
				reader->lengthInSamples / reader->sampleRate,
				juce::URL { audioFile }
			};

			size_t h = hasher(
				t.title.toStdString() +
				std::to_string(t.lengthInSeconds) +
				t.url.toString(false).toStdString() +
				std::to_string(processedSoFar) +
				timeString);
			char hashString[64] = "";
			std::snprintf(hashString, sizeof hashString, "%zu", h);
			t.identity = juce::String(hashString);

			t.fileHash = FileHasher::computeHash(audioFile);
			if (t.fileHash.isNotEmpty() && TrackDataCache::exists(t.fileHash))
				t.bpm = TrackDataCache::load(t.fileHash).detectedBpm;

			batch.push_back(std::move(t));

			if (static_cast<int>(batch.size()) >= batchSize)
				flushBatch(batch, processedSoFar, totalCount, currentName, false);
		}

		// Always send a final batch (even if empty) so the UI knows we are done.
		flushBatch(batch, processedSoFar, totalCount, {}, true);
		return jobHasFinished;
	}

private:
	void flushBatch(std::vector<track>& batch,
	                int processed,
	                int total,
	                juce::String currentName,
	                bool isFinal)
	{
		auto* ownerPtr = &owner;
		int   idx      = targetFolderIndex;
		auto  name     = newFolderName;
		auto  payload  = std::make_shared<std::vector<track>>(std::move(batch));
		batch.clear();

		juce::MessageManager::callAsync([ownerPtr, idx, name, payload, processed, total, currentName, isFinal]() {
			ownerPtr->onIngestBatchReady(idx, name, std::move(*payload),
			                              processed, total, currentName, isFinal);
		});
	}

	void notifyProgressOnly(int processed, int total, juce::String currentName)
	{
		auto* ownerPtr = &owner;
		juce::MessageManager::callAsync([ownerPtr, processed, total, currentName]() {
			{
				juce::ScopedLock sl(ownerPtr->ingestProgressLock);
				ownerPtr->ingestProgressText = "Importing " + juce::String(processed) + " / "
				                              + juce::String(total) + "  \u2014  " + currentName;
			}
			ownerPtr->ingestListeners.call([](IngestProgressListener& l){ l.ingestProgressChanged(); });
			ownerPtr->refreshIngestProgressUI();
		});
	}

	Library&                  owner;
	juce::Array<juce::File>   files;
	int                       targetFolderIndex;
	juce::String              newFolderName;
};

void Library::enqueueIngest(juce::Array<juce::File> files,
                            int targetFolderIndex,
                            juce::String newFolderName)
{
	if (files.isEmpty())
		return;

	ingestInFlight.fetch_add(1, std::memory_order_acq_rel);
	ingestActive.store(true, std::memory_order_release);

	{
		juce::ScopedLock sl(ingestProgressLock);
		ingestProgressText = "Importing 0 / " + juce::String(files.size()) + "\u2026";
	}
	ingestListeners.call([](IngestProgressListener& l){ l.ingestProgressChanged(); });

	refreshIngestProgressUI();

	ingestPool.addJob(new LibraryIngestJob(*this, std::move(files),
	                                       targetFolderIndex,
	                                       std::move(newFolderName)),
	                  true);
}

void Library::onIngestBatchReady(int targetFolderIndex,
                                 juce::String newFolderName,
                                 std::vector<track> batch,
                                 int processedCount,
                                 int totalCount,
                                 juce::String currentFileName,
                                 bool isFinalBatch)
{
	// Resolve / lazily create the destination folder.
	int destIndex = targetFolderIndex;
	if (destIndex < 0)
	{
		// New-folder ingest: create the folder on the first batch that
		// arrives, then remember its index for subsequent batches by
		// matching the name (cheap because batches arrive serially).
		int existing = -1;
		for (int i = 0; i < (int) trackFolders.size(); ++i)
			if (trackFolders[i].first == newFolderName)
				{ existing = i; break; }

		if (existing < 0)
		{
			std::pair<juce::String, std::vector<track>> folder;
			folder.first = newFolderName;
			trackFolders.push_back(std::move(folder));
			destIndex = static_cast<int>(trackFolders.size()) - 1;
			selectedFolderIndex = destIndex;
			directoryComponent.updateContent();
			directoryComponent.selectRow(destIndex);
		}
		else
		{
			destIndex = existing;
		}
	}

	if (destIndex >= 0 && destIndex < static_cast<int>(trackFolders.size()))
	{
		auto& dest = trackFolders[destIndex].second;
		for (auto& t : batch)
			dest.push_back(std::move(t));

		if (selectedFolderIndex == destIndex)
			playlist.setTrackTitles(dest);
	}

	// Update progress text + notify listeners.
	{
		juce::ScopedLock sl(ingestProgressLock);
		if (isFinalBatch)
			ingestProgressText = {};
		else
			ingestProgressText = "Importing " + juce::String(processedCount) + " / "
			                    + juce::String(totalCount)
			                    + (currentFileName.isNotEmpty()
			                           ? juce::String("  \u2014  ") + currentFileName
			                           : juce::String());
	}
	ingestListeners.call([](IngestProgressListener& l){ l.ingestProgressChanged(); });

	refreshIngestProgressUI();

	if (isFinalBatch)
	{
		auto remaining = ingestInFlight.fetch_sub(1, std::memory_order_acq_rel) - 1;
		if (remaining <= 0)
			ingestActive.store(false, std::memory_order_release);

		// Newly-imported tracks need BPM analysis dispatch (cache lookup + queue).
		if (destIndex >= 0 && destIndex < static_cast<int>(trackFolders.size()))
			queueBpmAnalysis(trackFolders[destIndex].second);

		scheduleAsyncSave();
	}
}

juce::String Library::getIngestProgressText() const
{
	juce::ScopedLock sl(ingestProgressLock);
	return ingestProgressText;
}

void Library::refreshIngestProgressUI()
{
	const auto text = getIngestProgressText();
	ingestProgressBar.setText("  " + text, juce::dontSendNotification);
	ingestProgressBar.setVisible(text.isNotEmpty());
	if (text.isNotEmpty())
		ingestProgressBar.toFront(false);
}

//==============================================================================
// Phase 2.5 - Async XML persistence

void Library::scheduleAsyncSave()
{
	saveDirtyCount.fetch_add(1, std::memory_order_relaxed);

	// Fire data-change listeners (sidebars rendering their own UI). Always
	// dispatched on the message thread.
	notifyLibraryChanged();

	if (saveDebounceTimer == nullptr)
	{
		struct DebTimer : public juce::Timer
		{
			Library* lib;
			explicit DebTimer(Library* l) : lib(l) {}
			void timerCallback() override
			{
				stopTimer();
				lib->flushSaveNow();
			}
		};
		saveDebounceTimer = std::make_unique<DebTimer>(this);
	}

	// Restart the debounce window. Multiple rapid mutations coalesce into
	// a single write 2 s after the last edit.
	saveDebounceTimer->startTimer(2000);
}

void Library::flushSaveNow()
{
	saveDirtyCount.store(0, std::memory_order_relaxed);

	juce::ValueTree tree = buildPersistenceTree();
	juce::String    path = filePath;

	struct SaveJob : public juce::ThreadPoolJob
	{
		juce::ValueTree tree;
		juce::String    path;
		SaveJob(juce::ValueTree t, juce::String p)
			: juce::ThreadPoolJob("LibrarySave"),
			  tree(std::move(t)),
			  path(std::move(p)) {}

		juce::ThreadPoolJob::JobStatus runJob() override
		{
			Library::persistTreeToDisk(tree, path);
			return jobHasFinished;
		}
	};

	savePool.addJob(new SaveJob(std::move(tree), std::move(path)), true);
}

juce::ValueTree Library::buildPersistenceTree() const
{
	juce::ValueTree main(juce::Identifier("main"));
	for (size_t i = 0; i < trackFolders.size(); ++i)
	{
		juce::ValueTree folder(juce::Identifier(std::to_string(i)));
		folder.setProperty(juce::Identifier("name"), trackFolders[i].first, nullptr);
		for (size_t j = 0; j < trackFolders[i].second.size(); ++j)
		{
			juce::ValueTree song(juce::Identifier(std::to_string(j)));
			const auto& tr = trackFolders[i].second[j];
			song.setProperty("title", tr.title, nullptr);
			song.setProperty("length", tr.lengthInSeconds, nullptr);
			song.setProperty("url", tr.url.toString(false), nullptr);
			song.setProperty("identity", tr.identity, nullptr);
			song.setProperty("fileHash", tr.fileHash, nullptr);
			song.setProperty("bpm", tr.bpm, nullptr);
			folder.addChild(song, static_cast<int>(j), nullptr);
		}
		main.addChild(folder, static_cast<int>(i), nullptr);
	}
	return main;
}

void Library::persistTreeToDisk(juce::ValueTree tree, juce::String filePath)
{
	juce::File file(filePath);
	juce::TemporaryFile temp(file);
	{
		juce::FileOutputStream out(temp.getFile());
		if (out.openedOk())
		{
			out.setPosition(0);
			out.truncate();
			tree.writeToStream(out);
		}
	}
	if (! temp.overwriteTargetFileWithTemporary())
		DBG("Library::persistTreeToDisk - failed to overwrite " + filePath);
}

//==============================================================================
// Public read accessors and mutators used by per-deck sidebars.

juce::String Library::getFolderName(int folderIndex) const
{
if (folderIndex < 0 || folderIndex >= (int) trackFolders.size())
return {};
return trackFolders[(size_t) folderIndex].first;
}

int Library::getNumTracksInFolder(int folderIndex) const
{
if (folderIndex < 0 || folderIndex >= (int) trackFolders.size())
return 0;
return (int) trackFolders[(size_t) folderIndex].second.size();
}

track Library::getTrack(int folderIndex, int trackIndex) const
{
if (folderIndex < 0 || folderIndex >= (int) trackFolders.size())
return {};
const auto& tracks = trackFolders[(size_t) folderIndex].second;
if (trackIndex < 0 || trackIndex >= (int) tracks.size())
return {};
return tracks[(size_t) trackIndex];
}

void Library::setActiveFolder(int folderIndex)
{
if (folderIndex < 0 || folderIndex >= (int) trackFolders.size())
return;
selectedFolderIndex = folderIndex;
playlist.setTrackTitles(trackFolders[(size_t) folderIndex].second);
}

void Library::removeTrackAt(int folderIndex, int trackIndex)
{
if (folderIndex < 0 || folderIndex >= (int) trackFolders.size())
return;
auto& tracks = trackFolders[(size_t) folderIndex].second;
if (trackIndex < 0 || trackIndex >= (int) tracks.size())
return;
tracks.erase(tracks.begin() + trackIndex);
if (selectedFolderIndex == folderIndex)
playlist.setTrackTitles(tracks);
scheduleAsyncSave();
}

void Library::notifyLibraryChanged()
{
auto fire = [this]() { listeners.call([](Listener& l) { l.libraryChanged(); }); };
if (juce::MessageManager::getInstance()->isThisTheMessageThread())
fire();
else
juce::MessageManager::callAsync(std::move(fire));
}
