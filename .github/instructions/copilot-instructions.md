# Copilot Instructions - DJDecks

> Global instructions for GitHub Copilot when working in this repository.

## Context Gathering Strategy

You are working on a **JUCE C++17 DJ application**. Before generating or modifying code:

1. **Read `.github/general-instructions.md`** for full project architecture, class hierarchy, and coding rules.
2. **Read the target file(s)** you are about to modify - understand existing patterns before writing.
3. **Read related files** when your change touches cross-component interactions (check the class hierarchy in general-instructions.md).
4. If unsure about JUCE API usage, check the JUCE module headers in `build/_deps/juce-src/modules/` for the authoritative API reference.

## Code Generation Rules

- Generate **C++17** code. Use `auto`, structured bindings, `std::optional`, `constexpr`, `if constexpr` where appropriate.
- Always prefix JUCE types with `juce::` - never use `using namespace juce`.
- Use `#pragma once` for all headers.
- Match the existing **tab-based indentation** (not spaces).
- Place `#include <JuceHeader.h>` first, then project headers, in every file.
- Add `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClassName)` to every new class.
- Use **Doxygen `/** */` comments** for classes and methods, `///` for member variables.
- Prefer `juce::String` over `std::string` for anything user-facing.

## Audio Code Constraints

- **Thread safety**: `getNextAudioBlock()` runs on the audio thread - never allocate memory, lock mutexes, or do I/O in it.
- **Filter chain**: `DJAudioPlayer` has a fixed filter chain order (see general-instructions.md). New audio effects must be inserted correctly in the chain.
- **Sample rate**: Always use the stored `thisSampleRate` - never hardcode sample rates.
- **Gain values**: Always clamp between 0.0 and 1.0 before passing to `transportSource.setGain()`.

## UI Code Constraints

- New components must override `paint(juce::Graphics&)` and `resized()`.
- Register as visible with `addAndMakeVisible()` in the parent constructor.
- Follow the existing listener pattern: `addListener(this)` in constructor, implement callback.
- Use `juce::Colour::fromRGBA(25, 25, 25, 255)` as primary dark background.
- Deck 1 theme = `juce::Colours::aqua`, Deck 2 theme = `juce::Colours::hotpink`.
- For custom drawing, use `CustomLookAndFeel` - read `src/CustomLookAndFeel.h` before creating new LookAndFeel overrides.

## Build System

- New `.cpp`/`.h` files must be added to `SOURCE_FILES` in `src/CMakeLists.txt`.
- New assets must be registered in both `CMakeLists.txt` (root) and `src/CMakeLists.txt`.
- The project links against: `juce_audio_basics`, `juce_audio_devices`, `juce_audio_utils`, `juce_dsp`, `juce_core`, `juce_data_structures`, `juce_graphics`, `juce_gui_basics`, `juce_analytics`, and TagLib.

## Testing Changes

After making changes, verify the build compiles:
```bash
cmake --build build
```

## Avoid

- Do NOT use `new`/`delete` for JUCE components - use `std::unique_ptr` or parent ownership via `addAndMakeVisible`.
- Do NOT add global `using namespace` declarations.
- Do NOT hardcode file paths - use `juce::File::getSpecialLocation()` for platform-portable paths.
- Do NOT modify the audio filter chain order in `DJAudioPlayer` without understanding the signal flow.
- Do NOT introduce new external dependencies without adding them via `FetchContent` in the root `CMakeLists.txt`.
