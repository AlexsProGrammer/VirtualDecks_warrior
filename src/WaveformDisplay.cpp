
#include "UIConstants.h"
#include "WaveformDisplay.h"

//==============================================================================

/**
 * Implementation of a constructor for WaveformDisplay
 *
 * Initializes data members and configure component details
 *
 */
WaveformDisplay::WaveformDisplay(juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, juce::Colour _colour) : audioThumb(50, formatManagerToUse, cacheToUse), position(0), theme(_colour)
{
	audioThumb.addChangeListener(this);
}

/**
 * Implementation of a destructor for WaveformDisplay
 *
 */
WaveformDisplay::~WaveformDisplay()
{
}

//==============================================================================

/**
 * Implementation of getPosition method for WaveformDisplay
 *
 * Returns the position data member
 *
 */
double WaveformDisplay::getPosition() {
	return position;
}

/**
 * Implementation of isSliderDragged method for WaveformDisplay
 *
 * Returns the sliderIsDragged data member
 *
 */
bool WaveformDisplay::isSliderDragged() {
	return sliderIsDragged;
};

/**
 * Implementation of isFileLoaded method for WaveformDisplay
 *
 * Returns the isLoaded data member
 *
 */
bool WaveformDisplay::isFileLoaded() {
	return isLoaded;
};

//==============================================================================

/**
 * Implementation of loadTrack method for WaveformDisplay
 *
 * Calls loadURL using the track object's url data member
 * and stores the loaded song name
 *
 */
void WaveformDisplay::loadTrack(track track) {
	loadURL(track.url);
	if (isLoaded) {
		songNameLoaded = track.title;
		trackDefaultBpm = track.bpm;
	}
};

//==============================================================================

/**
 * Implementation of setPositionRelative method for WaveformDisplay
 *
 * Sets the relative position playhead of the component and repaints the component
 *
 */
void WaveformDisplay::setPositionRelative(double pos) {
	if (pos != position) {
		position = pos;
		repaint();
	}
}

/**
 * Implementation of setCuePoints method for WaveformDisplay
 *
 * Updates the cueTargets data member with std::pairs containing
 * the cue positions and cue colours.
 *
 */
void WaveformDisplay::setCuePoints(std::map<juce::TextButton*, std::pair<double, float>>& _cueTargets) {
	cueTargets.clear();
	std::map<juce::TextButton*, std::pair<double, float>>::iterator it;
	for (it = _cueTargets.begin(); it != _cueTargets.end(); it++)
	{
		cueTargets.push_back(&(it->second));
	}
	DBG("cueTargets size" << cueTargets.size());
};

/**
 * Implementation of setBeatGrid method for WaveformDisplay
 *
 * Updates the beat grid parameters used for rendering beat lines.
 */
void WaveformDisplay::setBeatGrid(double bpm, double offsetSecs, double speed) {
	if (beatGridBpm != bpm || beatGridOffsetSecs != offsetSecs || speedRatio != speed) {
		beatGridBpm = bpm;
		beatGridOffsetSecs = offsetSecs;
		speedRatio = speed;
		repaint();
	}
};

/**
 * Implementation of setLoopRegion method for WaveformDisplay
 *
 * Updates the loop region parameters used for rendering the loop highlight.
 */
void WaveformDisplay::setLoopRegion(double inRelative, double outRelative, bool active) {
	if (loopInRel != inRelative || loopOutRel != outRelative || loopIsActive != active) {
		loopInRel = inRelative;
		loopOutRel = outRelative;
		loopIsActive = active;
		repaint();
	}
};

/**
 * Implementation of setBandData for WaveformDisplay.
 *
 * Replaces the per-column 3-band colour table used by paint(). Pass nullptr
 * (or an empty data pointer) to revert to the theme-colour render path.
 * Rebuilds the cached fill paths so paint() can stay at 3 fillPath calls.
 */
void WaveformDisplay::setBandData(BandDataPtr data) {
	bandData = std::move(data);
	rebuildBandPaths();
	repaint();
}

/**
 * Implementation of rebuildBandPaths for WaveformDisplay.
 *
 * Builds three filled-area paths in normalised coordinates: x ∈ [0, numFrames],
 * y ∈ [-1, +1]. Each path traces the top edge left→right then mirrors back
 * along the bottom edge so a single fillPath() in paint produces a symmetric
 * band-stack shape.
 */
void WaveformDisplay::rebuildBandPaths() {
	bandPathLow.clear();
	bandPathMid.clear();
	bandPathHigh.clear();

	if (bandData == nullptr || bandData->empty())
		return;

	const auto& frames = *bandData;
	const int n = (int)frames.size();
	constexpr float invByte = 1.0f / 255.0f;

	// 3-tap [0.25, 0.5, 0.25] smoothing kernel applied at path-build time —
	// removes the visible 50 ms steps and produces fluent rounded shapes
	// without costing anything at paint time.
	auto smoothed = [&](int i, auto raw) {
		const int iL = (i > 0) ? i - 1 : i;
		const int iR = (i + 1 < n) ? i + 1 : i;
		return 0.25f * raw(iL) + 0.5f * raw(i) + 0.25f * raw(iR);
	};

	auto buildPath = [&](juce::Path& p, auto raw) {
		p.preallocateSpace((n + 1) * 6);
		p.startNewSubPath(0.0f, -smoothed(0, raw));
		for (int i = 1; i < n; ++i)
			p.lineTo((float)i, -smoothed(i, raw));
		for (int i = n - 1; i >= 0; --i)
			p.lineTo((float)i, +smoothed(i, raw));
		p.closeSubPath();
	};

	buildPath(bandPathLow,  [&](int i) { return frames[(size_t)i].low  * invByte; });
	buildPath(bandPathMid,  [&](int i) { return frames[(size_t)i].mid  * invByte; });
	buildPath(bandPathHigh, [&](int i) { return frames[(size_t)i].high * invByte; });
}

/**
 * Implementation of drawBandWaveform for WaveformDisplay.
 *
 * Renders the cached band paths into `bounds`, mapping the time window
 * [t0Sec, t1Sec] of the loaded track to the rectangle's full width via a
 * single AffineTransform per band — three fillPath calls total regardless
 * of frame count or zoom level.
 *
 * Colour palette mirrors Pioneer rekordbox:
 *   low  = blue  (behind, fully opaque),
 *   mid  = orange (overlaid, slight alpha),
 *   high = white (top, slight alpha).
 */
void WaveformDisplay::drawBandWaveform(juce::Graphics& g,
                                       juce::Rectangle<int> bounds,
                                       double t0Sec,
                                       double t1Sec) const
{
	if (bandData == nullptr || bandData->empty() || bandPathLow.isEmpty())
		return;

	const double total = audioThumb.getTotalLength();
	if (total <= 0.0 || t1Sec <= t0Sec)
		return;

	const int n = (int)bandData->size();
	const float frameStart = (float)juce::jlimit(0.0, (double)n, t0Sec / total * n);
	const float frameEnd   = (float)juce::jlimit(0.0, (double)n, t1Sec / total * n);
	if (frameEnd <= frameStart + 1e-3f)
		return;

	const float w = (float)bounds.getWidth();
	const float h = (float)bounds.getHeight();
	const float cy = (float)bounds.getCentreY();
	const float sx = w / (frameEnd - frameStart);
	const float sy = h * 0.45f; // 90% of half-height; leaves a little headroom

	const auto trans = juce::AffineTransform::translation(-frameStart, 0.0f)
	                       .scaled(sx, sy)
	                       .translated((float)bounds.getX(), cy);

	// Pioneer-style 3-colour palette.
	const juce::Colour kLow  = juce::Colour::fromRGB( 50, 130, 230); // blue
	const juce::Colour kMid  = juce::Colour::fromRGB(255, 145,  35); // orange
	const juce::Colour kHigh = juce::Colour::fromRGB(245, 245, 245); // white

	g.setColour(kLow);
	g.fillPath(bandPathLow, trans);
	g.setColour(kMid.withAlpha(0.85f));
	g.fillPath(bandPathMid, trans);
	g.setColour(kHigh.withAlpha(0.85f));
	g.fillPath(bandPathHigh, trans);
}

//==============================================================================

/**
 * Implementation of paint method for WaveformDisplay
 *
 * Checks if track is loaded onto component.
 * Upon loading, calls the drawChannel method on audioThumb to
 * draw the waveform.
 * Cue point data containing time stamps are drawn on the component as
 * vertical lines.
 */
void WaveformDisplay::paint(juce::Graphics& g)
{
	g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
	g.setColour(juce::Colours::grey);
	g.drawRect(getLocalBounds(), 1);

	g.setColour(theme);
	if (isLoaded) {
		// Rebuild waveform cache only when thumbnail data changed or size changed.
		if (waveformCacheDirty ||
		    waveformCache.getWidth()  != getWidth() ||
		    waveformCache.getHeight() != getHeight())
		{
			waveformCache = juce::Image(juce::Image::RGB, getWidth(), getHeight(), true);
			juce::Graphics cg(waveformCache);
			cg.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
			cg.setColour(theme);
			if (bandData != nullptr && ! bandData->empty()) {
				drawBandWaveform(cg, getLocalBounds(), 0.0, audioThumb.getTotalLength());
			}
			else {
				audioThumb.drawChannel(cg, getLocalBounds(), 0, audioThumb.getTotalLength(), 0, 0.55);
			}
			waveformCacheDirty = false;
		}

		// Blit cached waveform (zero cost, just a memcpy).
		g.drawImageAt(waveformCache, 0, 0);

		// Draw text overlays on top of the waveform.
		g.setFont(juce::Font(juce::FontOptions(11.0f)));
		g.setColour(juce::Colours::white.withAlpha(0.85f));
		g.drawText(songNameLoaded, 5, 3, getWidth() * 3 / 4, 14, juce::Justification::left, true);
		if (trackDefaultBpm > 0.0) {
			juce::String bpmText = juce::String(trackDefaultBpm, 1) + " BPM";
			g.drawText(bpmText, getWidth() * 3 / 4, 3, getWidth() / 4 - 4, 14, juce::Justification::right, false);
		}

		// Draw dynamic overlays (playhead, hover, cues, loop) on top.
		g.setColour(juce::Colours::lightgreen);
		g.drawRect(position * getWidth(), 0, 1, getHeight());

		if (mouseEntered) {
			g.setColour(juce::Colours::white);
			g.drawRect(prevX, 0, 1, getHeight());
		}

		for (auto i = 0; i < cueTargets.size(); ++i) {
			g.setColour(juce::Colour::fromHSL(static_cast<float>(cueTargets[i]->second), 1.0f, 0.5f, 1.0f));
			g.drawRect(cueTargets[i]->first * getWidth(), 0, 1, getHeight());
		}

		// Draw loop region highlight
		if (loopInRel >= 0.0) {
			float loopX1 = static_cast<float>(loopInRel * getWidth());
			// Always draw the IN marker as an blue line
			g.setColour(juce::Colours::blue);
			g.drawLine(loopX1, 0.0f, loopX1, static_cast<float>(getHeight()), 2.0f);

			if (loopOutRel > loopInRel) {
				float loopX2 = static_cast<float>(loopOutRel * getWidth());
				juce::Colour loopColour = loopIsActive
					? juce::Colours::limegreen.withAlpha(0.25f)
					: juce::Colours::blue.withAlpha(0.20f);
				g.setColour(loopColour);
				g.fillRect(loopX1, 0.0f, loopX2 - loopX1, static_cast<float>(getHeight()));
				// Draw boundary lines
				juce::Colour lineColour = loopIsActive ? juce::Colours::limegreen : juce::Colours::blue;
				g.setColour(lineColour);
				g.drawLine(loopX1, 0.0f, loopX1, static_cast<float>(getHeight()), 2.0f);
				g.drawLine(loopX2, 0.0f, loopX2, static_cast<float>(getHeight()), 2.0f);
			}
		}
	}
	else {
		g.setFont(20.0f);
		g.drawText("File not loaded...", getLocalBounds(),
			juce::Justification::centred, true);
	}
}

/**
 * Implementation of resized method for WaveformDisplay
 *
 */
void WaveformDisplay::resized()
{
	waveformCacheDirty = true;
}

//==============================================================================

/**
 * Implementation of changeListenerCallback method for WaveformDisplay
 *
 * Calls repaint function to draw the component again.
 *
 */
void WaveformDisplay::changeListenerCallback(juce::ChangeBroadcaster* source) {
	waveformCacheDirty = true;
	repaint();
}

//==============================================================================

/**
 * Implementation of mouseMove method for WaveformDisplay
 *
 * Sets the mouseEntered data member to true
 * Updates the prevX data member with the mouse X position
 * and calls repaint
 *
 */
void WaveformDisplay::mouseMove(const juce::MouseEvent& e) {
	mouseEntered = true;
	if (isEnabled() && prevX != e.x) {
		prevX = e.x;
		repaint();
	}
};

/**
 * Implementation of mouseExit method for WaveformDisplay
 *
 * Sets the mouseEntered data member to false.
 *
 */
void WaveformDisplay::mouseExit(const juce::MouseEvent& e) {
	mouseEntered = false;
};

/**
 * Implementation of mouseDown method for WaveformDisplay
 *
 * Sets it's slider value and relative position using the mouse X position
 * Sets sliderIsDragged to true
 */
void WaveformDisplay::mouseDown(const juce::MouseEvent& e) {
	if (isEnabled()) {
		sliderIsDragged = true;
		DBG("MOUSE DOWNED");
		setValue((double)e.x / (double)getWidth());
		setPositionRelative(getValue());
	}
}

/**
 * Implementation of mouseDrag method for WaveformDisplay
 *
 * Does the same thing as MouseDown. But updates the prevX data member
 *
 */
void WaveformDisplay::mouseDrag(const juce::MouseEvent& e) {
	if (isEnabled()) {
		prevX = e.x;
		mouseDown(e);
	}
}

/**
 * Implementation of mouseUp method for WaveformDisplay
 *
 * Sets sliderIsDragged to false
 *
 */
void WaveformDisplay::mouseUp(const juce::MouseEvent& e) {
	sliderIsDragged = false;
}

//==============================================================================

/**
 * Implementation of loadURL method for WaveformDisplay
 *
 * Calls the setSource method on the audioThumb and
 * clears all previous track data on data members.
 *
 */
void  WaveformDisplay::loadURL(juce::URL audioURL) {
	isLoaded = false;
	DBG("WaveformDispaly loadURL");
	audioThumb.clear();
	if (audioThumb.setSource(new juce::URLInputSource(audioURL))) {
		DBG("Successfully loaded wfd");
		isLoaded = true;
		setPositionRelative(0);
		cueTargets.clear();
	}
	else {
		DBG("Failed loaded wfd");
	}
}

//==============================================================================


