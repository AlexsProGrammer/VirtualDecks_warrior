---
mode: "ask"
description: "Refactor or improve existing code quality"
---

# Refactor Code

You are refactoring or improving code quality in DJDecks.

## Context Gathering Strategy

1. **Read `.github/copilot-instructions.md`** for the full architecture.
2. **Read ALL files that will be affected** by the refactor - including callers and inheritors.
3. If touching the waveform hierarchy, read `WaveformDisplay.h`, `ZoomedWaveform.h`, and `JogWheel.h` - protected members are shared.
4. If touching DJAudioPlayer, read `.github/instructions/audio-engine.instructions.md` for chain order rules.

## Refactoring Guidelines

- **Preserve behavior**: Refactoring must not change observable behavior. No feature additions.
- **Preserve the architecture**: The class hierarchy and signal flow are intentional. Don't flatten or restructure without explicit request.
- **Small steps**: Make one logical change at a time. Verify it builds before the next change.
- **Match existing style**: Even if the existing code isn't ideal, match its style for consistency (tabs, Doxygen comments, `juce::` prefix, listener pattern).

## Common Improvements

- Replacing `new`/`delete` with `std::unique_ptr` (e.g., cue buttons in DeckGUI use raw `new`)
- Using `auto` for iterator-based loops
- Extracting magic numbers into named constants
- Adding `const` correctness to methods that don't modify state
- Using structured bindings for pair/tuple iteration
- Replacing `std::string` with `juce::String` where JUCE types are expected

## What NOT to Refactor Without Explicit Request

- The audio filter chain order in DJAudioPlayer
- The `WaveformDisplay → ZoomedWaveform → JogWheel` inheritance hierarchy
- The Library's XML persistence format (would break existing user data)
- The `track` struct layout (would require serialization updates)
