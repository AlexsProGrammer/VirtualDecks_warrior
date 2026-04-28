
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
 */
void WaveformDisplay::setBandData(BandDataPtr data) {
	bandData = std::move(data);
	repaint();
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
		g.drawText(songNameLoaded, 5, 5, getWidth() * 3 / 4, 6, juce::Justification::left);

		if (bandData != nullptr && ! bandData->empty()) {
			// 3-band RGB column rendering (Serato-style).
			const auto& frames = *bandData;
			const int numFrames = (int)frames.size();
			const int w = getWidth();
			const int h = getHeight();
			const float centerY = (float)h * 0.5f;
			for (int x = 0; x < w; ++x) {
				const int idx = juce::jlimit(0, numFrames - 1, (int)((double)x * numFrames / juce::jmax(1, w)));
				const auto& f = frames[(size_t)idx];
				const float halfH = (f.amp / 255.0f) * centerY;
				g.setColour(juce::Colour::fromRGB(f.low, f.mid, f.high));
				g.drawLine((float)x, centerY - halfH, (float)x, centerY + halfH, 1.0f);
			}
		}
		else {
			audioThumb.drawChannel(g, getLocalBounds(), 0, audioThumb.getTotalLength(), 0, 0.55);
		}

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
}

//==============================================================================

/**
 * Implementation of changeListenerCallback method for WaveformDisplay
 *
 * Calls repaint function to draw the component again.
 *
 */
void WaveformDisplay::changeListenerCallback(juce::ChangeBroadcaster* source) {
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


