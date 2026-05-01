
#define _USE_MATH_DEFINES
#include "UIConstants.h"
#include "JogWheel.h" 
//==============================================================================

/**
 * Implementation of a constructor for JogWheel
 *
 * Having inherited from ZoomWaveform, the passed in values are passed as arguments
 * into the ZoomWaveform constructor.
 *
 */
JogWheel::JogWheel(juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, juce::Colour _colour) : ZoomedWaveform(formatManagerToUse, cacheToUse, _colour)
{
}

/**
 * Implementation of a destructor for JogWheel
 *
 */
JogWheel::~JogWheel()
{
}

//==============================================================================

/**
 * Implementation of paint method for JogWheel
 *
 * The number of total rotations is defined here, with the angle position
 * of the current playhead determined by mapping the current position of
 * the song to an angle between 0 and the number of rotations multiplied
 * by 360.
 * With the current angle position, a line is drawn from the middle to the
 * edge of the component, creating a playhead for the component.
 * The current time is also drawn on the component when a file is loaded
 */
void JogWheel::paint(juce::Graphics& g)
{
	const float w = (float) getWidth();
	const float h = (float) getHeight();
	const float cx = w * 0.5f;
	const float cy = h * 0.5f;
	const float outerR = juce::jmin(w, h) * 0.5f - 1.0f;

	// --- Static background cache (rim + inner disc) ---
	// Only re-rendered when the component is resized.
	if (getLocalBounds() != bgCachedBounds || bgCache.isNull())
	{
		bgCachedBounds = getLocalBounds();
		bgCache = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
		juce::Graphics cg(bgCache);

		// Outer rim - vertical gradient from elevated to root
		const float bw = (float) getWidth();
		const float bh = (float) getHeight();
		const float bcx = bw * 0.5f;
		const float bcy = bh * 0.5f;
		juce::ColourGradient rimGrad(UI::bgElevated, bcx, bcy - outerR,
		                              UI::bgRoot,    bcx, bcy + outerR, false);
		cg.setGradientFill(rimGrad);
		cg.fillEllipse(bcx - outerR, bcy - outerR, outerR * 2, outerR * 2);

		// Theme-tinted rim stroke drawn by caller using live `theme` - skip here
		// (colour not available in static cache). Use subtle border instead.
		cg.setColour(UI::borderSubtle);
		cg.drawEllipse(bcx - outerR, bcy - outerR, outerR * 2, outerR * 2, 1.5f);

		// Inner disc
		const float innerR = outerR - 10.0f;
		juce::ColourGradient innerGrad(UI::bgRoot.darker(0.4f), bcx, bcy - innerR,
		                                UI::bgRoot,             bcx, bcy + innerR, false);
		cg.setGradientFill(innerGrad);
		cg.fillEllipse(bcx - innerR, bcy - innerR, innerR * 2, innerR * 2);

		cg.setColour(UI::borderSubtle);
		cg.drawEllipse(bcx - innerR, bcy - innerR, innerR * 2, innerR * 2, 1.0f);
	}

	// Blit cached background (zero cost)
	g.drawImageAt(bgCache, 0, 0);

	// Theme-tinted rim stroke (live colour, cheap)
	g.setColour(theme.withAlpha(0.35f));
	g.drawEllipse(cx - outerR, cy - outerR, outerR * 2, outerR * 2, 1.5f);

	// --- Rotating arm (redrawn every frame, cheap path stroke) ---
	noRotations = audioThumb.getTotalLength() / 2;
	const float angle = getPosition() * 360.0f * noRotations;
	const float piAngle = angle * (float) M_PI / 180.0f;

	startPoint.x = cx;
	startPoint.y = cy;
	line.setStart(startPoint);
	endPoint.x = cx + (outerR - 4) * std::cos(piAngle);
	endPoint.y = cy + (outerR - 4) * std::sin(piAngle);
	line.setEnd(endPoint);
	g.setColour(theme);
	g.drawLine(line, 6.0f);

	// Centre dot (cheap ellipse fill)
	g.setColour(theme.withAlpha(0.9f));
	g.fillEllipse(cx - 3, cy - 3, 6, 6);

	if (isLoaded) {
		std::string time = track::getLengthString(position * audioThumb.getTotalLength(), true);
		juce::Rectangle<float> rect(0, cy - 10, w, 14);
		g.setColour(UI::textPrimary);
		g.setFont(jogFont);
		g.drawText(time, rect, juce::Justification::centred);
	}
}

/**
 * Implementation of resized method for JogWheel
 *
 */
void JogWheel::resized()
{
}

//==============================================================================

/**
 * Implementation of mouseDrag method for JogWheel
 *
 * The current angle of the mouse is compared to the previous recorded angle
 * to determine if the playhead's value should move forward or backwards.
 *
 */
void JogWheel::mouseDrag(const juce::MouseEvent& e) {
	juce::Point<double> centre(getWidth() / 2, getHeight() / 2);
	juce::Point<double> currentPoint(e.x, e.y);
	juce::Point<double> prevPoint(prevX, prevY);
	if (isEnabled()) {
		sliderIsDragged = true;
		DBG("MOUSE DRAGGED :: jog");
		if (centre.getAngleToPoint(currentPoint) - centre.getAngleToPoint(prevPoint) > 0) {
			setValue(position + 0.1 / audioThumb.getTotalLength());
		}
		else if (centre.getAngleToPoint(currentPoint) - centre.getAngleToPoint(prevPoint) < 0) {
			setValue(position - 0.1 / audioThumb.getTotalLength());
		}
		prevX = e.x;
		prevY = e.y;
		setPositionRelative(getValue());
	}
};

//==============================================================================
