# DJDecks - General Project Instructions

> These instructions define the project context, architecture, and rules for any AI assistant working on this codebase. The AI should read the relevant source files to gather specifics - these instructions tell it **where to look** and **what rules to follow**, not duplicate the code.

---

## Project Overview

**DJDecks** is a desktop DJ audio player application built with **C++17** and the **JUCE framework**. It provides dual-deck mixing with waveform visualization, audio filtering (low/mid/high band EQ, high-pass/low-pass), speed/BPM control, cue points, crossfading, and a persistent track library with folder management.

- **Build system**: CMake (minimum 3.25) with `FetchContent` for dependencies
- **C++ standard**: C++17 (`CMAKE_CXX_STANDARD 17`)
- **Primary framework**: JUCE (fetched from `juce-framework/JUCE` master)
- **Additional libraries**: TagLib (audio metadata), xwax (vinyl emulation)
- **Target platforms**: Linux (primary), Windows, macOS, Android/iOS (theoretical)
- **Binary assets**: SVG icons and PNG logo bundled via `juce_add_binary_data` → accessible through `BinaryData::` namespace

---

## Source Architecture

All application source code lives in `src/`. Before modifying any file, **always read it first** to understand the current implementation.

### Class Hierarchy & Relationships

```
Main.cpp
 └─ OtoDecksApplication (juce::JUCEApplication)
     └─ MainWindow (juce::DocumentWindow)
         └─ MainComponent (juce::AudioAppComponent)
              ├─ DJAudioPlayer × 2      - Audio engine (AudioSource chain)
              ├─ DeckGUI × 2            - Per-deck controls & visuals
              │    ├─ WaveformDisplay    - Full waveform + playhead
              │    ├─ JogWheel           - Circular jog display (inherits ZoomedWaveform)
              │    └─ Cue buttons × 6   - Dynamic cue point system
              ├─ ZoomedWaveform × 2     - Top-level zoomed waveform bands
              ├─ Library                 - Folder/playlist manager with XML persistence
              │    └─ PlaylistComponent  - Track list within a folder
              ├─ MixerAudioSource       - Mixes both players
              └─ CrossFader slider      - Cross-fade between decks
```

### Inheritance Chain (Waveform)

```
juce::Slider → WaveformDisplay → ZoomedWaveform → JogWheel
```

`WaveformDisplay` extends `juce::Slider` (for drag-based position control). `ZoomedWaveform` and `JogWheel` override paint/mouse behavior. Protected members in `WaveformDisplay` are shared down the hierarchy.

### Audio Signal Chain (per DJAudioPlayer)

```
AudioFormatReaderSource → AudioTransportSource → ResamplingAudioSource
  → IIRFilter (Low Band) → IIRFilter (Mid Band) → IIRFilter (High Band)
  → IIRFilter (High Pass) → IIRFilter (Low Pass) → [output to MixerAudioSource]
```

### Data Persistence

The `Library` class persists track folders to `~/.otodecks/Resource.xml` using `juce::ValueTree` serialization. The file is read in the constructor and written in the destructor.

---

## Key Data Structures

- **`track` struct** (`src/Track.h`): POD-like struct with `title`, `lengthInSeconds`, `url`, `identity` fields. Contains a static `getLengthString()` utility.
- **`Library::trackFolders`**: `std::vector<std::pair<juce::String, std::vector<track>>>` - folders of tracks.
- **`DeckGUI::cueTargets`**: `std::map<juce::TextButton*, std::pair<double, float>>` - cue point positions + color hue.

---

## File-by-File Reference

When you need context on a component, read the corresponding files:

| Concern | Header | Implementation |
|---------|--------|----------------|
| App entry point | - | `src/Main.cpp` |
| Root component, mixing, crossfade | `src/MainComponent.h` | `src/MainComponent.cpp` |
| Per-deck UI (buttons, sliders, cues) | `src/DeckGUI.h` | `src/DeckGUI.cpp` |
| Audio engine (playback, filters, gain) | `src/DJAudioPlayer.h` | `src/DJAudioPlayer.cpp` |
| Full waveform display | `src/WaveformDisplay.h` | `src/WaveformDisplay.cpp` |
| Zoomed waveform strip | `src/ZoomedWaveform.h` | `src/ZoomedWaveform.cpp` |
| Jog wheel display | `src/JogWheel.h` | `src/JogWheel.cpp` |
| Track library & folders | `src/Library.h` | `src/Library.cpp` |
| Playlist within folder | `src/PlaylistComponent.h` | `src/PlaylistComponent.cpp` |
| Track data model | `src/Track.h` | - (header-only) |
| Custom slider/table rendering | `src/CustomLookAndFeel.h` | `src/CustomLookAndFeel.cpp` |
| Build config | `CMakeLists.txt` | `src/CMakeLists.txt` |

---

## Build & Run

```bash
# Configure (from project root)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run
./build/juceDjApp_artefacts/Debug/juceDjApp
```

The build fetches JUCE, TagLib, and xwax via `FetchContent`. First build will be slow.

---

## Rules for AI Assistants

### Before Writing Any Code

1. **Read the relevant source files first.** Never guess at implementation details - the file reference table above tells you exactly where to look.
2. **Understand the inheritance chain** before modifying waveform-related classes. Changes to `WaveformDisplay` protected members affect `ZoomedWaveform` and `JogWheel`.
3. **Check the audio signal chain order** before modifying `DJAudioPlayer`. The IIR filter chain order matters.

### Code Style & Conventions

4. Use **tab indentation** (the codebase uses tabs, not spaces).
5. Use `juce::` **explicit namespace prefix** everywhere - never use `using namespace juce`.
6. Use `#pragma once` for header guards (no `#ifndef` guards).
7. Include `<JuceHeader.h>` as the first include in every file.
8. Use **Doxygen-style `/** */` block comments** for all class and method documentation.
9. Use `/// Single-line doc comments` for member variable documentation.
10. Follow the existing **listener pattern**: components add listeners in constructors, implement callbacks (e.g., `buttonClicked`, `sliderValueChanged`).
11. Use `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClassName)` at the end of every class.
12. Prefer `juce::String` over `std::string` for UI-facing strings.
13. Use **brace-initialization** for member variables where possible (e.g., `juce::Slider volSlider{ ... }`).

### JUCE-Specific Rules

14. **Never call `delete` on JUCE components** that are managed by `std::unique_ptr` or parent ownership. Use `reset()` or let RAII handle it.
15. New UI components must implement `paint()` and `resized()` and be added via `addAndMakeVisible()`.
16. Timer-based components must call `stopTimer()` in the destructor.
17. Binary assets (images, SVGs) are accessed via the `BinaryData::` namespace - check `assets/` for available files.
18. Use `juce::Colour::fromRGBA()` or named `juce::Colours::` for color values. The dark theme uses `RGBA(25, 25, 25, 255)` as the primary background and `RGBA(50, 50, 50, 255)` as the secondary.
19. Deck 1 uses `juce::Colours::aqua` as its theme color. Deck 2 uses `juce::Colours::hotpink`.

### Architecture Rules

20. **All audio processing stays in `DJAudioPlayer`** - DeckGUI delegates to the player, never processes audio directly.
21. **DeckGUI communicates with Library by pointer reference** - do not create new coupling patterns.
22. **Do not add new global state.** Each deck is independent; shared state goes through `MainComponent`.
23. When adding a new source file, add it to the `SOURCE_FILES` list in `src/CMakeLists.txt`.
24. New binary assets go in `assets/` and must be registered in both `CMakeLists.txt` and `src/CMakeLists.txt` binary data sections.
