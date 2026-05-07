
#pragma once

#include "../../ui/settings/CustomLookAndFeel.h"
#include "../../ui/library/PlaylistComponent.h"
#include "../analysis/BpmAnalysisManager.h"
#include "FileHasher.h"
#include <JuceHeader.h>
#include <atomic>

//==============================================================================

/**
 * Definition of a Library Component
 *
 * A component to manage a library of playlist folders.
 * Functionality to select playlist folders and display
 * folder's track list. Contains folder and track add/delete
 * functionality as well as data persistance.
 *
 */
class Library : public juce::Component,
                public juce::TableListBoxModel,
                public juce::FileDragAndDropTarget,
                public juce::Button::Listener,
                public BpmAnalysisManager::Listener {
public:
  //==============================================================================

  /**
   * Class Constructor for Library, reads xml library data into class,
   * initializes member variables and configures component details.
   *
   * @param AudioFormatManager reference
   */
  Library(juce::AudioFormatManager &_formatManager);

  /**
   * Class destructor for Library, used to write library data to xml file
   */
  ~Library() override;

  //==============================================================================

  /**
   * @return if the selected cell is a valid selection
   */
  bool selectionIsValid();

  /**
   * @return selected track object
   */
  track getSelectedTrack();

  /**
   * Removes a pair element from trackFolders, or a track object from the
   * selected pair's vector
   */
  void deleteItem();

  //==============================================================================

  /**
   * Adds audio files to the currently selected folder via file chooser dialog
   */
  void addFilesToFolder();

  /**
   * Adds a new empty folder to the library
   */
  void addFolder();

  /**
   * Removes the currently selected folder from the library
   */
  void removeFolder();

  /**
   * Renames the currently selected folder
   */
  void renameFolder();

  /**
   * Removes the currently selected track from the current folder
   */
  void removeSelectedTrack();

  /**
   * Opens a directory chooser to import a folder of audio files into the library
   */
  void importFolderFromDisk();

  //==============================================================================
  // Public read-only data accessors (used by per-deck sidebars that render their
  // own UI without depending on Library's own widgets).

  /// @return Number of folders in the library.
  int getNumFolders() const { return (int) trackFolders.size(); }

  /// @return Folder display name at the given index, or empty if out of range.
  juce::String getFolderName(int folderIndex) const;

  /// @return Number of tracks in the folder at folderIndex (0 if out of range).
  int getNumTracksInFolder(int folderIndex) const;

  /// @return Track at (folderIndex, trackIndex), or empty track if out of range.
  track getTrack(int folderIndex, int trackIndex) const;

  /// Programmatically set the active folder (mirrors what cellClicked does
  /// on the directoryComponent). Safe to call when Library is not visible.
  void setActiveFolder(int folderIndex);

  /// Remove a single track by absolute index. Persists asynchronously.
  void removeTrackAt(int folderIndex, int trackIndex);

  /// Listener interface fired when folder list or any folder's track list
  /// changes (folder added/removed/renamed, tracks added/removed/ingested).
  class Listener
  {
  public:
    virtual ~Listener() = default;
    virtual void libraryChanged() = 0;
  };

  /// Add an external listener for library-data changes.
  void addListener(Listener* l) { listeners.add(l); }

  /// Remove an external listener.
  void removeListener(Listener* l) { listeners.remove(l); }

  /// Listener interface for ingest progress changes (used by IngestProgressBar
  /// or any external observer that wants to react when an ingest job starts,
  /// progresses, or completes).
  class IngestProgressListener
  {
  public:
    virtual ~IngestProgressListener() = default;
    virtual void ingestProgressChanged() = 0;
  };

  //==============================================================================

private:
  //==============================================================================

  /**
   * Paints the Library Component.
   *
   * @param juce::Graphics object
   */
  void paint(juce::Graphics &) override;

  /**
   * Set bounds of member components
   */
  void resized() override;

  //==============================================================================

  /**
   * @return Number of rows in the library selection
   */
  int getNumRows() override;

  /**
   * Paints the row's background of the directoryComponent member.
   *
   * @param juce::Graphics object
   * @param Row number
   * @param Row width
   * @param Row height
   * @param If the row is selected
   */
  void paintRowBackground(juce::Graphics &g, int rowNumber, int width,
                          int height, bool rowIsSelected) override;

  /**
   * Paints each cell of the directoryComponent member.
   *
   * @param juce::Graphics object
   * @param Row number
   * @param Column number
   * @param Cell width
   * @param Cell height
   * @param If the row is selected
   */
  void paintCell(juce::Graphics &g, int rowNumber, int columnId, int width,
                 int height, bool rowIsSelected) override;

  /**
   * Called when cell is clicked.
   *
   * @param Row number
   * @param Column number
   * @param juce::MouseEvent triggered by user
   */
  void cellClicked(int rowNumber, int columnId,
                   const juce::MouseEvent &e) override;

  //==============================================================================

  /**
   * Called when file is dragged over component
   *
   * @param const juce::StringArray of files being dragged over the component
   */
  bool isInterestedInFileDrag(const juce::StringArray &files) override;

  /**
   * Called when file is dropped over component
   *
   * @param const juce::StringArray of files being dropped over the component
   * @param x position of files being dropped
   * @param y position of files being dropped
   */
  void filesDropped(const juce::StringArray &files, int x, int y) override;

  //==============================================================================

  /**
   * Called when a button is clicked
   *
   * @param Button that was clicked
   */
  void buttonClicked(juce::Button *button) override;

  //==============================================================================

  /// Instance of CustomLookAndFeel class.
  CustomLookAndFeel customLookAndFeel;

  /// Instance of a PlaylistComponent Class
  PlaylistComponent playlist;

  /// Reference assigned to the AudioFormatManager passed into the constructor
  juce::AudioFormatManager &formatManager;

  /// Reader source for the audio url
  std::unique_ptr<juce::AudioFormatReader> audioReader;

  /// Reflects the trackFolders' elements
  juce::TableListBox directoryComponent;

  /// Data structure to hold playlist folders containing track objects
  std::vector<std::pair<juce::String, std::vector<track>>> trackFolders;

  /// Selected index of the directoryComponent
  int selectedFolderIndex = -1;

  /// File path to read xml data from and load the trackFolders when the
  /// application starts
  juce::String filePath;

  /// Button to add a new folder
  juce::TextButton addFolderBtn{"+ Folder"};

  /// Button to remove the selected folder
  juce::TextButton removeFolderBtn{"- Folder"};

  /// Button to rename the selected folder
  juce::TextButton renameFolderBtn{"Rename"};

  /// Button to add files to the selected folder
  juce::TextButton addFilesBtn{"+ Files"};

  /// Button to remove the selected track
  juce::TextButton removeTrackBtn{"- Track"};

  /// Button to import a folder of audio files from disk
  juce::TextButton importFolderBtn{"Import Folder"};

  /// Non-modal progress strip shown while ingest jobs are running.
  juce::Label ingestProgressBar { "ingestProgress", {} };

  /// Refresh the visibility / text of the progress strip from message thread.
  void refreshIngestProgressUI();

  /// File chooser for adding audio files
  std::unique_ptr<juce::FileChooser> fileChooser;

  /// Background BPM analysis manager
  BpmAnalysisManager bpmAnalysisManager;

  /// Dedicated thread pool for off-thread folder ingestion (file scanning,
  /// duration probing, FileHasher hashing, cache lookups).
  juce::ThreadPool ingestPool { 2 };

  /// True while at least one ingest job is in-flight.
  std::atomic<bool> ingestActive { false };

  /// Number of in-flight ingest jobs (used to flip ingestActive correctly).
  std::atomic<int> ingestInFlight { 0 };

  /// Latest progress text ("Importing N / M - filename"), guarded by lock.
  juce::String ingestProgressText;

  /// CriticalSection guarding ingestProgressText (touched from message thread
  /// only in current design but kept for safety against future progress
  /// callbacks dispatched off-thread).
  juce::CriticalSection ingestProgressLock;

  /// Listeners notified when ingest progress changes.
  juce::ListenerList<IngestProgressListener> ingestListeners;

  /// External listeners notified whenever the folder/track data changes.
  juce::ListenerList<Listener> listeners;

  /// Fire libraryChanged() on all external Listeners on the message thread.
  void notifyLibraryChanged();

  //==============================================================================
  // Async XML save

  /// Background thread pool (single worker) used for debounced XML save.
  juce::ThreadPool savePool { 1 };

  /// Coalescing timer: starts on every mutation, fires once 2 s of quiet
  /// elapses, at which point it builds the tree and dispatches to savePool.
  std::unique_ptr<juce::Timer> saveDebounceTimer;

  /// Number of pending mutations since last save (debug only).
  std::atomic<int> saveDirtyCount { 0 };

  /// Queue all tracks in a folder for background BPM analysis
  void queueBpmAnalysis(std::vector<track>& tracks);

  /// Callback when background BPM analysis completes
  void bpmAnalysisComplete(const juce::String& fileHash, double bpm) override;

  //==============================================================================
  // Off-thread library ingestion (Phase 2)

  /**
   * Background ThreadPoolJob that scans a list of audio files (creating a
   * reader for duration, computing FileHasher hash, looking up cached BPM)
   * and streams completed track records back to the message thread in
   * small batches. UI never blocks on disk I/O during ingest.
   */
  class LibraryIngestJob;
  friend class LibraryIngestJob;

  /**
   * Enqueue an ingest job that scans "files" and appends the resulting
   * tracks into trackFolders[targetFolderIndex]. If targetFolderIndex == -1
   * the tracks are gathered under a brand-new folder named "newFolderName".
   *
   * Safe to call from the message thread only.
   */
  void enqueueIngest(juce::Array<juce::File> files,
                     int targetFolderIndex,
                     juce::String newFolderName);

  /// Message-thread callback invoked by LibraryIngestJob to merge a batch
  /// of finished tracks into the model and refresh the UI.
  void onIngestBatchReady(int targetFolderIndex,
                          juce::String newFolderName,
                          std::vector<track> batch,
                          int processedCount,
                          int totalCount,
                          juce::String currentFileName,
                          bool isFinalBatch);

  //==============================================================================
  // Async XML persistence (Phase 2.5)

  /**
   * Snapshot trackFolders into a juce::ValueTree and write it to disk on
   * a background thread. Subsequent calls within ~2 s are coalesced.
   */
  void scheduleAsyncSave();

  /// Synchronously builds a ValueTree snapshot of trackFolders. Must run
  /// on the message thread (only the latest tree is then handed to the
  /// background save thread).
  juce::ValueTree buildPersistenceTree() const;

  /// Background-thread helper that serialises a ValueTree to disk.
  static void persistTreeToDisk(juce::ValueTree tree, juce::String filePath);

  /// Build the tree on the message thread and dispatch the actual write
  /// onto the savePool. Called by the debounce timer when quiet.
  void flushSaveNow();

  /// True while at least one ingest job is in-flight (drives progress UI).
  bool isIngesting() const { return ingestActive.load(std::memory_order_acquire); }

  /// Latest progress label text (safe from any thread).
  juce::String getIngestProgressText() const;

  void addIngestProgressListener   (IngestProgressListener* l) { ingestListeners.add(l); }
  void removeIngestProgressListener(IngestProgressListener* l) { ingestListeners.remove(l); }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Library)
};
