---
applyTo: "src/DeckGUI.*,src/WaveformDisplay.*,src/ZoomedWaveform.*,src/JogWheel.*,src/CustomLookAndFeel.*,src/MainComponent.*"
---

# UI Components Instructions

> Context-aware instructions for modifying UI components in this JUCE application.

## Before You Touch UI Code

1. Read the target component's `.h` and `.cpp` file fully.
2. Read `src/CustomLookAndFeel.h` if your change involves slider/table appearance.
3. Read `.github/general-instructions.md` for the class hierarchy and theme colors.
4. If modifying waveform classes, understand the inheritance chain: `WaveformDisplay → ZoomedWaveform → JogWheel`.

## Component Layout Model

The `MainComponent::resized()` lays out all top-level children with absolute positioning:

```
┌─────────────────────────────────────────────┐
│  ZoomedWaveform 1 (aqua)                    │  Top strip
│  ZoomedWaveform 2 (hotpink)                 │  Second strip
├──────────────────────┬──────────────────────┤
│  DeckGUI 1 (aqua)   │  DeckGUI 2 (hotpink) │  Main area
│                      │                      │
├──────────┬───────────┴──────────┬───────────┤
│          │  ◄── CrossFader ──►  │           │
├──────────┴──────────────────────┴───────────┤
│  Library (full width)                        │  Bottom
└─────────────────────────────────────────────┘
```

### DeckGUI Internal Layout

DeckGUI mirrors itself for deck 1 (aqua) vs deck 2 (hotpink) using the `theme` color to choose left/right offsets. Read `DeckGUI::resized()` to understand the offset logic before modifying layout.

Key sub-components:
- Top: `WaveformDisplay` (full width of deck)
- Left/Right column: Volume slider + Volume label + Filter knob + Filter label
- Center: 6 cue buttons in a 3×2 grid + Low/Mid/High band knobs
- Right/Left column: Speed slider + JogWheel + Play button + Load button

## Theme & Color Palette

| Use | Color |
|-----|-------|
| Primary background | `juce::Colour::fromRGBA(25, 25, 25, 255)` |
| Deck surface | `juce::Colour::fromRGBA(50, 50, 50, 255)` |
| Deck 1 accent | `juce::Colours::aqua` |
| Deck 2 accent | `juce::Colours::hotpink` |
| Selected row | `juce::Colour::fromRGBA(0, 125, 225, 255)` |
| Unselected row | `juce::Colour::fromRGBA(100, 100, 100, 255)` |
| Text & slider tracks | `juce::Colour::fromRGBA(255, 255, 255, 255)` |
| Cue button (inactive) | `juce::Colour::fromRGBA(25, 25, 25, 255)` |
| Cue button (active) | HSL with random hue at full saturation |

## Component Patterns to Follow

### Adding a New Slider

```cpp
// In header - declare with inline initialization:
juce::Slider mySlider{ juce::Slider::SliderStyle::RotaryVerticalDrag,
                        juce::Slider::TextEntryBoxPosition::NoTextBox };
juce::Label myLabel{ "LABEL", "LABEL" };

// In constructor:
addAndMakeVisible(mySlider);
addAndMakeVisible(myLabel);
mySlider.setRange(minVal, maxVal);
mySlider.setValue(defaultVal);
mySlider.addListener(this);
mySlider.setLookAndFeel(&customLookAndFeel);
myLabel.setEditable(false);
myLabel.setJustificationType(juce::Justification::centred);

// In resized():
mySlider.setBounds(x, y, w, h);
myLabel.setBounds(x, y, w, h);

// In sliderValueChanged():
if (slider == &mySlider) {
    player->setMyParam(slider->getValue());
}
```

### Adding a New Button

Use `juce::DrawableButton` with SVG images from BinaryData. Follow the pattern in `DeckGUI` constructor for parsing SVGs and setting images.

### Creating a New Component Class

1. Inherit from `juce::Component` (or the appropriate base).
2. Add listener interfaces as public base classes.
3. Override `paint()` and `resized()`.
4. Add `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`.
5. Use `CustomLookAndFeel` for consistent appearance.
6. Register listeners in constructor, implement callbacks.

## Timer Usage

`DeckGUI` uses `juce::Timer` with `startTimer(20)` (50 fps) for real-time UI updates (waveform position, VU meter). Always call `stopTimer()` in the destructor of timed components.

## CustomLookAndFeel

The `CustomLookAndFeel` class overrides:
- `drawLinearSlider()` - custom knob images from SVG, tick marks on slider tracks
- `drawRotarySlider()` - circular knob with angle markers
- `drawTableHeaderBackground()` - custom table header painting

Read `src/CustomLookAndFeel.h` and `src/CustomLookAndFeel.cpp` before adding new LookAndFeel overrides. Extend the existing class rather than creating a new one.
