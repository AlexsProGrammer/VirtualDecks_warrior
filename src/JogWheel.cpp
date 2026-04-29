
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

	// Drop shadow under wheel
	juce::DropShadow(juce::Colours::black.withAlpha(0.55f), 14, { 0, 4 })
		.drawForRectangle(g, juce::Rectangle<int>((int)(cx - outerR), (int)(cy - outerR),
		                                          (int)(outerR * 2), (int)(outerR * 2)));

	// Outer rim — vertical gradient from elevated to root
	juce::ColourGradient rimGrad(UI::bgElevated, cx, cy - outerR,
	                              UI::bgRoot,    cx, cy + outerR, false);
	g.setGradientFill(rimGrad);
	g.fillEllipse(cx - outerR, cy - outerR, outerR * 2, outerR * 2);

	// Subtle theme-tinted rim stroke
	g.setColour(theme.withAlpha(0.35f));
	g.drawEllipse(cx - outerR, cy - outerR, outerR * 2, outerR * 2, 1.5f);

	// Playhead arm — theme color, tapered line
	noRotations = audioThumb.getTotalLength() / 2;
	float angle = getPosition() * 360 * noRotations;
	float piAngle = angle * (float) M_PI / 180.0f;

	startPoint.x = cx;
	startPoint.y = cy;
	line.setStart(startPoint);
	endPoint.x = cx + (outerR - 4) * std::cos(piAngle);
	endPoint.y = cy + (outerR - 4) * std::sin(piAngle);
	line.setEnd(endPoint);
	g.setColour(theme);
	g.drawLine(line, 6.0f);

	// Inner disc — flat dark with slight gradient
	const float innerR = outerR - 10.0f;
	juce::ColourGradient innerGrad(UI::bgRoot.darker(0.4f), cx, cy - innerR,
	                                UI::bgRoot,             cx, cy + innerR, false);
	g.setGradientFill(innerGrad);
	g.fillEllipse(cx - innerR, cy - innerR, innerR * 2, innerR * 2);

	g.setColour(UI::borderSubtle);
	g.drawEllipse(cx - innerR, cy - innerR, innerR * 2, innerR * 2, 1.0f);

	// Centre dot
	g.setColour(theme.withAlpha(0.9f));
	g.fillEllipse(cx - 3, cy - 3, 6, 6);

	if (isLoaded) {
		std::string time = track::getLengthString(position * audioThumb.getTotalLength(), true);
		juce::Rectangle<float> rect(0, cy - 10, w, 14);
		g.setColour(UI::textPrimary);
		g.setFont(juce::Font(11.0f, juce::Font::bold));
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
