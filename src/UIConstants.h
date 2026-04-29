#pragma once

#include <JuceHeader.h>

/**
 * Centralised UI constants for DJDecks: colour palette, corner radii and
 * layout dimensions used across MainComponent, DeckGUI, sidebars and the
 * custom LookAndFeel.
 *
 * Keep this header lean — only `constexpr` / `inline` values, no behaviour.
 */
namespace UI
{
	// =========================================================================
	// Colour palette — premium black with vibrant neon-leaning accents.
	// =========================================================================

	/// Window background — premium near-black.
	inline const juce::Colour bgRoot       = juce::Colour::fromRGB(0x0A, 0x0A, 0x0D);

	/// Primary surface colour for deck panels and the mixer body.
	inline const juce::Colour bgSurface    = juce::Colour::fromRGB(0x14, 0x14, 0x1A);

	/// Elevated surface for cards, sidebars and the settings panel.
	inline const juce::Colour bgElevated   = juce::Colour::fromRGB(0x1C, 0x1C, 0x24);

	/// Inner card surface (track-table rows, knob bases, search pill).
	inline const juce::Colour bgCard       = juce::Colour::fromRGB(0x23, 0x23, 0x2C);

	/// Hairline between sections.
	inline const juce::Colour borderSubtle = juce::Colour::fromRGB(0x2E, 0x2E, 0x38);

	/// 1px outline for elevated cards.
	inline const juce::Colour borderStrong = juce::Colour::fromRGB(0x3A, 0x3A, 0x45);

	/// Primary (high-contrast) text — soft white, easier on the eye than pure white.
	inline const juce::Colour textPrimary   = juce::Colour::fromRGB(0xF5, 0xF5, 0xF7);

	/// Secondary text — labels, placeholders, units.
	inline const juce::Colour textSecondary = juce::Colour::fromRGB(0x9A, 0x9A, 0xA5);

	/// Disabled text.
	inline const juce::Colour textDisabled  = juce::Colour::fromRGB(0x5A, 0x5A, 0x65);

	/// Deck 1 accent — vibrant neon-leaning sky blue (slightly darker than aqua).
	inline const juce::Colour deck1Accent  = juce::Colour::fromRGB(0x1F, 0xB8, 0xFF);

	/// Deck 2 accent — pink/red hybrid (crimson-coral, neon).
	inline const juce::Colour deck2Accent  = juce::Colour::fromRGB(0xFF, 0x3D, 0x5C);

	/// Confirm / sync-on accent (replaces the old dodgerblue toggle).
	inline const juce::Colour accentPositive = juce::Colour::fromRGB(0x10, 0xD1, 0x82);

	/// Warning / master-on accent (replaces orange).
	inline const juce::Colour accentWarning  = juce::Colour::fromRGB(0xFF, 0xB0, 0x20);

	// =========================================================================
	// Layout — corner radii.
	// =========================================================================

	inline constexpr float kPanelRadius  = 10.0f; ///< Outer panels (sidebars, mixer, deck cards).
	inline constexpr float kCardRadius   =  8.0f; ///< Inner cards (folder list, track table).
	inline constexpr float kButtonRadius =  6.0f; ///< Default rounded-rect button radius.
	inline constexpr float kPillRadius   = 14.0f; ///< Pill-shaped (search, filter chips).

	// =========================================================================
	// Layout — sizes.
	// =========================================================================

	inline constexpr int kRailWidth        = 44;  ///< Width of vertical icon-tab rails.
	inline constexpr int kRailIconInset    =  8;  ///< Inner padding inside a rail tab.

	inline constexpr int kDeckPaddingX     = 10;  ///< Horizontal padding inside deck panel.
	inline constexpr int kDeckPaddingY     =  6;  ///< Vertical padding inside deck panel.
	inline constexpr int kSectionGap       =  8;  ///< Gap between deck sections.

	inline constexpr int kKnobSize         = 50;  ///< EQ / filter knob square size.
	inline constexpr int kKnobLabelHeight  = 14;  ///< Label below a knob.

	inline constexpr int kButtonHeight     = 28;  ///< Standard rounded button height.

	// =========================================================================
	// Layout — responsive deck architecture metrics.
	// =========================================================================

	inline constexpr int kDeckMargin       =  8;  ///< Outer margin around the deck panel content area.
	inline constexpr int kComponentPadding =  6;  ///< Standard padding between sibling child components.
	inline constexpr int kHeaderHeight     = 40;  ///< Height of the top header strip (track title, BPM).
	inline constexpr int kWaveformHeight   = 60;  ///< Height of the full waveform display band.
	inline constexpr int kTransportHeight  = 50;  ///< Height of the transport row (play, cue, speed).
	inline constexpr int kJogWheelMinSize  = 120; ///< Minimum width/height of the jog wheel area.
	inline constexpr int kCueButtonSize    = 50;  ///< Performance pad square.

	inline constexpr int kTopBarHeight     = 36;  ///< Top status / settings bar.
	inline constexpr int kZoomedWaveHeight = 70;  ///< Per-deck zoomed waveform strip.

	inline constexpr int kMinWindowW       = 1024;
	inline constexpr int kMinWindowH       = 600;

	// =========================================================================
	// Helpers.
	// =========================================================================

	/// Mix a deck accent colour with a translucent "glow" alpha for halos.
	inline juce::Colour withGlowAlpha(juce::Colour c, float a = 0.35f) { return c.withAlpha(a); }
}
