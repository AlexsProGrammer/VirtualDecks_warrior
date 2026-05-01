---
applyTo: "src/Library.*,src/PlaylistComponent.*,src/Track.h"
---

# Library & Data Instructions

> Context-aware instructions for modifying the Library, PlaylistComponent, or Track data model.

## Before You Touch This Code

1. Read `src/Library.h`, `src/Library.cpp`, `src/PlaylistComponent.h`, `src/PlaylistComponent.cpp`, and `src/Track.h`.
2. Read `.github/general-instructions.md` for persistence details.

## Data Model

### `track` Struct (Header-Only)

```
track {
    juce::String title;
    double lengthInSeconds;
    juce::URL url;
    juce::String identity;  // hash for uniqueness
}
```

If adding new fields to `track`, you **must** also update:
1. Library constructor - where tracks are deserialized from `juce::ValueTree`
2. Library destructor - where tracks are serialized to `juce::ValueTree`
3. Any place that constructs `track` objects (Library `filesDropped`, etc.)

### Storage Structure

`Library::trackFolders` is a `std::vector<std::pair<juce::String, std::vector<track>>>`:
- Each pair = one folder: `first` = folder name, `second` = tracks in that folder
- The `PlaylistComponent` receives a reference to the selected folder's track vector

### XML Persistence

- File location: `~/.otodecks/Resource.xml` (created via `juce::File::getSpecialLocation`)
- Format: `juce::ValueTree` written/read as a binary stream (not human-readable XML text)
- Written in `Library::~Library()`, read in `Library::Library()`
- The directory is created if it doesn't exist

## Component Relationships

```
Library (TableListBoxModel - displays folders)
  └─ PlaylistComponent (TableListBoxModel - displays tracks in selected folder)
       └─ TextEditor search box (filters displayed tracks)
```

- `Library` owns `PlaylistComponent` as a member
- `PlaylistComponent` holds a **pointer** to the current folder's track vector (set via `setTrackTitles()`)
- `PlaylistComponent::displayTrackTitles` is a filtered view (vector of pointers to tracks matching the search)

## Key Interactions

- **DeckGUI → Library**: DeckGUI holds a `Library*` pointer. When load button is clicked, it calls `library->selectionIsValid()` and `library->getSelectedTrack()`.
- **MainComponent → Library**: Library is a direct member of MainComponent. Dead key 'D' triggers `library.deleteItem()`.
- **File drag-and-drop**: Both Library and DeckGUI implement `juce::FileDragAndDropTarget`. Library creates track objects from dropped files using `AudioFormatManager`.

## Rules

- Never store raw `juce::File` objects in the track model - use `juce::URL` for portability.
- Track identity hashes must remain unique within a folder.
- When deleting tracks, compare by `identity` string, not by index.
- Always call `playlist.setTrackTitles()` after modifying `trackFolders` to refresh the UI.
- Always call `directoryComponent.updateContent()` after modifying the folder list.
