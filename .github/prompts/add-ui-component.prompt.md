---
mode: "ask"
description: "Add a new UI component to the DJ application"
---

# Add UI Component

You are adding a new visual component to DJDecks.

## Context Gathering

Before proceeding, read these files:
1. The parent component where the new component will be placed (likely `src/DeckGUI.*` or `src/MainComponent.*`)
2. `src/CustomLookAndFeel.h` - for consistent appearance
3. `.github/instructions/ui-components.instructions.md` - layout model and patterns
4. `.github/instructions/general-instructions.md` - class hierarchy and style rules

## Implementation Steps

For the component described by the user:

1. **Create the header** (`src/NewComponent.h`):
   - `#pragma once`, include `<JuceHeader.h>`
   - Doxygen class doc comment
   - Inherit from `juce::Component` (+ any listener interfaces)
   - Declare `paint()` and `resized()` overrides
   - Add `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`

2. **Create the implementation** (`src/NewComponent.cpp`):
   - Include the header
   - Implement constructor (addAndMakeVisible, addListener, configure sliders/buttons)
   - Implement `paint()` (use theme colors from instructions)
   - Implement `resized()` (use setBounds)
   - Implement listener callbacks

3. **Register in parent**:
   - Add as member in parent header
   - Call `addAndMakeVisible()` in parent constructor
   - Add `setBounds()` in parent `resized()`

4. **Update build**: Add both files to `SOURCE_FILES` in `src/CMakeLists.txt`

## Constraints

- Match the existing dark theme (RGBA 25,25,25 background)
- Use `juce::Colour` theme parameter if the component should have deck-specific coloring
- Use tab indentation
- Follow Doxygen comment style
