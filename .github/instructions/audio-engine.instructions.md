---
applyTo: "src/DJAudioPlayer.*"
---

# Audio Engine Instructions

> Context-aware instructions for modifying `DJAudioPlayer` and related audio code.

## Before You Touch This Code

1. Read `src/DJAudioPlayer.h` and `src/DJAudioPlayer.cpp` fully.
2. Read `.github/general-instructions.md` section "Audio Signal Chain" for the filter chain order.
3. If your change involves audio formats, check `juce::AudioFormatManager` usage in `MainComponent.cpp`.

## Audio Signal Chain (DO NOT REORDER)

```
AudioFormatReaderSource
  → AudioTransportSource (gain, playback)
    → ResamplingAudioSource (speed/BPM)
      → IIRFilter: Low Band (low shelf @ 500 Hz)
        → IIRFilter: Mid Band (peak @ 3250 Hz)
          → IIRFilter: High Band (high shelf @ 5000 Hz)
            → IIRFilter: High Pass
              → IIRFilter: Low Pass
                → [output]
```

Each filter wraps the previous one. The last source in the chain (`audioLPFilter`) is what `getNextAudioBlock()` calls. If adding a new effect, insert it at the correct position in this chain.

## Thread Safety Rules

- `getNextAudioBlock()` is called on the **real-time audio thread**.
- NEVER allocate memory (`new`, `std::vector::push_back`, `juce::String` construction).
- NEVER lock a mutex or do I/O (file reads, network, logging).
- NEVER call `DBG()` or `std::cout` inside `getNextAudioBlock()`.
- Use **atomic variables** for communication between UI thread and audio thread.
- Pre-allocate any buffers in `prepareToPlay()`.

## IIR Filter Patterns

Current band filter frequencies:
- Low band: 500 Hz low shelf
- Mid band: 3250 Hz peak filter
- High band: 5000 Hz high shelf

The Q factor is `1.0 / juce::MathConstants<double>::sqrt2` for all bands.

When the HP/LP filter knob is at 0, both filters are made inactive. Positive values activate HP, negative activate LP.

## Gain Management

Two gain sources multiply together: `playerVol × crossFadeVol`. The `setGain()` method has a `bool isVol` parameter:
- `true` → sets `playerVol` (volume slider)
- `false` → sets `crossFadeVol` (crossfader in MainComponent)

Always validate gain is in [0, 1] before calling `transportSource.setGain()`.

## Adding New Audio Effects

1. Declare the new `juce::IIRFilterAudioSource` (or other AudioSource wrapper) as a member.
2. Insert it in the chain at the correct position - chain it from the previous source.
3. Update every downstream source to take the new one as input.
4. Call `prepareToPlay()` on the new source in `DJAudioPlayer::prepareToPlay()`.
5. Add a public setter method following the pattern of `setLBFilter()` / `setHBFilter()`.
6. Wire the setter to a slider in `DeckGUI::sliderValueChanged()`.
