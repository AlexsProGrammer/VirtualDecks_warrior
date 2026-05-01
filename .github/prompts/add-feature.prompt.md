---
mode: "agent"
description: "Add new feature to the DJ application"
---

# Add Feature

You are implementing a new feature for DJDecks.

## Context Gathering Strategy

Before writing any code:

1. **Read `.github/copilot-instructions.md`** - understand the full architecture, class hierarchy, and all coding rules.
2. **Identify which components are affected** using the class hierarchy diagram.
3. **Read every file you plan to modify** - all headers and implementations.
4. **Read related instruction files**:
   - Audio changes → `.github/instructions/audio-engine.instructions.md`
   - UI changes → `.github/instructions/ui-components.instructions.md`
   - Library/data changes → `.github/instructions/library-data.instructions.md`
   - Build changes → `.github/instructions/build-system.instructions.md`

## Implementation Planning

Break the feature into these layers and implement bottom-up:

1. **Data model** - Does the `track` struct or `Library` storage need new fields? Update serialization.
2. **Audio engine** - Does `DJAudioPlayer` need new capabilities? Respect the signal chain.
3. **UI components** - What new controls or displays are needed? Follow component patterns.
4. **Wiring** - How do the layers connect? Follow existing listener/callback patterns.
5. **Build** - Do new files need CMake registration? Do new assets need bundling?

## Quality Checklist

Before finishing:
- [ ] All new classes have `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`
- [ ] All new headers use `#pragma once`
- [ ] Tab indentation used throughout
- [ ] `juce::` prefix on all JUCE types
- [ ] Doxygen comments on new classes and public methods
- [ ] No allocations in audio thread code
- [ ] New source files added to `src/CMakeLists.txt` `SOURCE_FILES`
- [ ] New assets registered in both CMake files
- [ ] Build verified: `cmake --build build`
