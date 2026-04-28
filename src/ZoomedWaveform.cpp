
#include "ZoomedWaveform.h"
//==============================================================================

/**
 * Implementation of a constructor for ZoomedWaveform
 *
 * Having inherited from WaveformDisplay, the passed in values are passed as arguments
 * into the WaveformDisplay constructor.
 *
 */
ZoomedWaveform::ZoomedWaveform(juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, juce::Colour _colour) : WaveformDisplay(formatManagerToUse, cacheToUse, _colour)
{
}

/**
 * Implementation of a destructor for ZoomedWaveform
 *
 */
ZoomedWaveform::~ZoomedWaveform()
{
}

//==============================================================================

/**
 * Implementation of paint method for ZoomedWaveform
 *
 * Similar to WaveformDisplay, calls the drawChannel method on audioThumb to
 * draw the waveform.
 * However, the waveform drawn is zoomed in and instead of a moving playhead,
 * the drawn waveform moves against a fixed playhead in the middle.
 *
 */
void ZoomedWaveform::paint(juce::Graphics& g)
{
	g.fillAll(juce::Colour::fromRGBA(0, 0, 0, 255));
	g.setColour(juce::Colours::grey);

	if (isLoaded) {
		double thisPos = position * audioThumb.getTotalLength();
		double half = (audioThumb.getTotalLength() / 80) * speedRatio;
		double left = thisPos - half;
		double right = thisPos + half;
		g.setColour(theme);

		if (bandData != nullptr && ! bandData->empty()) {
			// 3-band RGB column rendering for the zoomed window.
			const auto& frames = *bandData;
			const int numFrames = (int)frames.size();
			const double total = audioThumb.getTotalLength();
			const int w = getWidth();
			const int h = getHeight();
			const float centerY = (float)h * 0.5f;
			for (int x = 0; x < w; ++x) {
				const double t = juce::jmap((double)x, 0.0, (double)juce::jmax(1, w), left, right);
				if (t < 0.0 || t > total) continue;
				const int idx = juce::jlimit(0, numFrames - 1, (int)(t / juce::jmax(1e-9, total) * numFrames));
				const auto& f = frames[(size_t)idx];
				const float halfH = (f.amp / 255.0f) * centerY;
				g.setColour(juce::Colour::fromRGB(f.low, f.mid, f.high));
				g.drawLine((float)x, centerY - halfH, (float)x, centerY + halfH, 1.0f);
			}
		}
		else {
			audioThumb.drawChannel(g, getLocalBounds(), left, right, 0, .7);
		}

		if (left < 0) {
			double widthRect = juce::jmap(fabs(left), (double)0, half * 2, (double)0, (double)getWidth());
			g.setColour(juce::Colour::fromRGBA(0, 0, 0, 255));
			g.fillRect(0.0f, 0.0f, (float)widthRect, (float)getHeight() - 1);
		}

		// Draw beat grid lines
		if (beatGridBpm > 0.0) {
			double beatIntervalSecs = 60.0 / beatGridBpm;
			// Calculate first beat in visible range
			double firstBeat;
			if (beatIntervalSecs > 0.0) {
				firstBeat = std::ceil((left - beatGridOffsetSecs) / beatIntervalSecs) * beatIntervalSecs + beatGridOffsetSecs;
			}
			else {
				firstBeat = left;
			}

			// Determine the beat index for downbeat emphasis
			int beatIndex = 0;
			if (beatIntervalSecs > 0.0)
				beatIndex = static_cast<int>(std::round((firstBeat - beatGridOffsetSecs) / beatIntervalSecs));

			for (double beatTime = firstBeat; beatTime <= right; beatTime += beatIntervalSecs) {
				double xPos = juce::jmap(beatTime, left, right, 0.0, (double)getWidth());
				if (xPos >= 0 && xPos <= getWidth()) {
					if (beatIndex % 4 == 0) {
						// Downbeat: brighter line
						g.setColour(juce::Colours::white.withAlpha(0.5f));
						g.drawLine((float)xPos, 0.0f, (float)xPos, (float)getHeight(), 1.5f);
					}
					else {
						// Regular beat: subtle line
						g.setColour(juce::Colours::white.withAlpha(0.2f));
						g.drawLine((float)xPos, 0.0f, (float)xPos, (float)getHeight(), 1.0f);
					}
				}
				beatIndex++;
			}
		}

		for (auto i = 0; i < cueTargets.size(); ++i) {
			if ((cueTargets[i]->first * audioThumb.getTotalLength()) > left && (cueTargets[i]->first * audioThumb.getTotalLength()) < right) {
				g.setColour(juce::Colour::fromHSL(static_cast<float>(cueTargets[i]->second), 1.0f, 0.5f, 1.0f));
				double widthPos = juce::jmap(cueTargets[i]->first * audioThumb.getTotalLength(), left, right, (double)0, (double)getWidth());
				g.drawRect(widthPos, 0, 1, getHeight());
			}
		}

		// Draw loop region highlight in zoomed view
		if (loopInRel >= 0.0) {
			double loopInSecs = loopInRel * audioThumb.getTotalLength();
			// Always draw the IN marker as an blue line if visible
			if (loopInSecs >= left && loopInSecs <= right) {
				float lx = static_cast<float>(juce::jmap(loopInSecs, left, right, 0.0, (double)getWidth()));
				g.setColour(juce::Colours::blue);
				g.drawLine(lx, 0.0f, lx, static_cast<float>(getHeight()), 2.0f);
			}

			if (loopOutRel > loopInRel) {
				double loopOutSecs = loopOutRel * audioThumb.getTotalLength();
				// Only draw region if it overlaps the visible range
				if (loopOutSecs > left && loopInSecs < right) {
					double drawLeft = std::max(loopInSecs, left);
					double drawRight = std::min(loopOutSecs, right);
					float x1 = static_cast<float>(juce::jmap(drawLeft, left, right, 0.0, (double)getWidth()));
					float x2 = static_cast<float>(juce::jmap(drawRight, left, right, 0.0, (double)getWidth()));
					juce::Colour loopColour = loopIsActive
						? juce::Colours::limegreen.withAlpha(0.25f)
						: juce::Colours::blue.withAlpha(0.20f);
					g.setColour(loopColour);
					g.fillRect(x1, 0.0f, x2 - x1, static_cast<float>(getHeight()));
					// Draw boundary lines if visible
					juce::Colour lineColour = loopIsActive ? juce::Colours::limegreen : juce::Colours::blue;
					g.setColour(lineColour);
					if (loopInSecs >= left && loopInSecs <= right) {
						float lx = static_cast<float>(juce::jmap(loopInSecs, left, right, 0.0, (double)getWidth()));
						g.drawLine(lx, 0.0f, lx, static_cast<float>(getHeight()), 2.0f);
					}
					if (loopOutSecs >= left && loopOutSecs <= right) {
						float rx = static_cast<float>(juce::jmap(loopOutSecs, left, right, 0.0, (double)getWidth()));
						g.drawLine(rx, 0.0f, rx, static_cast<float>(getHeight()), 2.0f);
					}
				}
			}
		}

		g.setColour(juce::Colours::grey);
		g.drawRect(getWidth() / 2, 0, 1, getHeight());

		// Draw speed % deviation overlay
		if (std::abs(speedRatio - 1.0) > 0.001 && beatGridBpm > 0.0) {
			double pct = (speedRatio - 1.0) * 100.0;
			juce::String sign = pct > 0 ? "+" : "";
			juce::String pctText = sign + juce::String(pct, 1) + "%";
			g.setColour(juce::Colour::fromRGBA(0, 0, 0, 160));
			g.fillRect(getWidth() - 52, 2, 50, 16);
			g.setColour(theme);
			g.setFont(juce::Font(juce::FontOptions(12.0f)));
			g.drawText(pctText, getWidth() - 52, 2, 50, 16, juce::Justification::centred);
		}
	}
}

/**
 * Implementation of resized method for ZoomedWaveform
 *
 */
void ZoomedWaveform::resized() {}

//==============================================================================

/**
 * Implementation of mouseDown method for ZoomedWaveform
 *
 * Overrides WaveformDisplay::mouseDown method with an empty
 * implementation.
 *
 */
void ZoomedWaveform::mouseDown(const juce::MouseEvent& e) {}

/**
 * Implementation of mouseDrag method for ZoomedWaveform
 *
 * The current X position of the mouse is compared to the previous recorded X position
 * to determine if the playhead's value should move forward or backwards.
 *
 */
void ZoomedWaveform::mouseDrag(const juce::MouseEvent& e) {
	if (isEnabled()) {
		sliderIsDragged = true;
		DBG("MOUSE DRAGGED :: Zoomed");
		if ((double)prevX > (double)e.x) {
			setValue(position + 0.1 / audioThumb.getTotalLength());
		}
		else if ((double)prevX < (double)e.x) {
			setValue(position - 0.1 / audioThumb.getTotalLength());
		}
		prevX = e.x;
		setPositionRelative(getValue());
	}
}

//==============================================================================




