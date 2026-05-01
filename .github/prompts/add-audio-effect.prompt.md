---
mode: "agent"
description: "Add a new audio effect to the DJ player"
---

# Add Audio Effect

You are adding a new audio effect to DJDecks.

## Context Gathering

Before proceeding, read these files to understand the current implementation:
1. `src/DJAudioPlayer.h` and `src/DJAudioPlayer.cpp` - current audio chain
2. `src/DeckGUI.h` and `src/DeckGUI.cpp` - how sliders wire to the player
3. `.github/instructions/audio-engine.instructions.md` - audio chain rules and thread safety

## Implementation Checklist

For the audio effect described by the user, implement:

1. **DJAudioPlayer** - Add a new IIR filter (or other AudioSource wrapper) member. Insert it in the correct position in the chain. Add `prepareToPlay()` call. Create a public setter method.
2. **DeckGUI** - Add a new slider + label. Wire it to the player setter in `sliderValueChanged()`. Set range, default value, LookAndFeel, and layout in `resized()`.
3. **Thread safety** - Verify the effect setter only modifies coefficients (which is audio-thread-safe for IIR filters) and does not allocate.

## Constraints

- Maintain the existing filter chain order unless the user explicitly asks to change it
- Use `thisSampleRate` for all filter coefficient calculations
- Use `CustomLookAndFeel` for the new slider
- Follow the existing naming pattern (`setXBFilter`, `audioXBFilter`)
