
#include "../../utilities/UIConstants.h"
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
 * Blits the visible sub-region from the pre-rendered full-track strip cache
 * so that only cheap overlays (beat grid, cues, loop region, playhead) are
 * redrawn on every frame instead of calling the expensive audioThumb.drawChannel.
 */
void ZoomedWaveform::paint(juce::Graphics& g)
{
	g.fillAll(UI::bgSurface);

	if (!isLoaded)
		return;

	// Rebuild the full-track strip cache if data or size changed.
	if (stripCacheDirty || stripCache.getHeight() != getHeight())
		rebuildStripCache();

	const double totalLen = audioThumb.getTotalLength();
	if (totalLen <= 0.0)
		return;

	const double thisPos = position * totalLen;
	const double half    = (totalLen / 80.0) * speedRatio;
	const double left    = thisPos - half;
	const double right   = thisPos + half;
	const double winLen  = right - left;

	const int W = getWidth();
	const int H = getHeight();

	// ── Blit visible sub-region from strip cache ─────────────────────────────
	const int sw = stripCache.getWidth();

	// Clamp to valid track range [0, totalLen]
	const double t0 = std::max(0.0, left);
	const double t1 = std::min(totalLen, right);

	if (t1 > t0)
	{
		const int srcX = (int)(t0 / totalLen * sw);
		const int srcW = std::max(1, (int)std::ceil((t1 - t0) / totalLen * sw));
		const int dstX = (int)((t0 - left) / winLen * W);
		const int dstW = std::max(1, (int)std::ceil((t1 - t0) / winLen * W));
		g.drawImage(stripCache,
		            dstX, 0, dstW, H,
		            juce::jlimit(0, sw - 1, srcX), 0,
		            juce::jlimit(1, sw - juce::jlimit(0, sw - 1, srcX), srcW), H);
	}

	// Fill pre-track region with black
	if (left < 0.0)
	{
		const int blackW = (int)((-left) / winLen * W) + 1;
		g.setColour(juce::Colours::black);
		g.fillRect(0, 0, blackW, H);
	}

	// Fill post-track region with black
	if (right > totalLen)
	{
		const int blackX = (int)((totalLen - left) / winLen * W);
		g.setColour(juce::Colours::black);
		g.fillRect(blackX, 0, W - blackX, H);
	}

	// ── Beat grid lines ───────────────────────────────────────────────────────
	if (beatGridBpm > 0.0)
	{
		const double beatIntervalSecs = 60.0 / beatGridBpm;
		double firstBeat = (beatIntervalSecs > 0.0)
		    ? std::ceil((left - beatGridOffsetSecs) / beatIntervalSecs) * beatIntervalSecs + beatGridOffsetSecs
		    : left;

		int beatIndex = (beatIntervalSecs > 0.0)
		    ? static_cast<int>(std::round((firstBeat - beatGridOffsetSecs) / beatIntervalSecs))
		    : 0;

		for (double beatTime = firstBeat; beatTime <= right; beatTime += beatIntervalSecs)
		{
			const double xPos = juce::jmap(beatTime, left, right, 0.0, (double)W);
			if (xPos >= 0 && xPos <= W)
			{
				if (beatIndex % 4 == 0)
				{
					g.setColour(juce::Colours::white.withAlpha(0.5f));
					g.drawLine((float)xPos, 0.0f, (float)xPos, (float)H, 1.5f);
				}
				else
				{
					g.setColour(juce::Colours::white.withAlpha(0.2f));
					g.drawLine((float)xPos, 0.0f, (float)xPos, (float)H, 1.0f);
				}
			}
			beatIndex++;
		}
	}

	// ── Cue markers ───────────────────────────────────────────────────────────
	for (auto i = 0; i < (int)cueTargets.size(); ++i)
	{
		const double cueSecs = cueTargets[i]->first * totalLen;
		if (cueSecs > left && cueSecs < right)
		{
			g.setColour(juce::Colour::fromHSL(static_cast<float>(cueTargets[i]->second), 1.0f, 0.5f, 1.0f));
			const double xPos = juce::jmap(cueSecs, left, right, 0.0, (double)W);
			g.drawRect((int)xPos, 0, 1, H);
		}
	}

	// ── Loop region ───────────────────────────────────────────────────────────
	if (loopInRel >= 0.0)
	{
		const double loopInSecs = loopInRel * totalLen;
		if (loopInSecs >= left && loopInSecs <= right)
		{
			const float lx = static_cast<float>(juce::jmap(loopInSecs, left, right, 0.0, (double)W));
			g.setColour(juce::Colours::blue);
			g.drawLine(lx, 0.0f, lx, static_cast<float>(H), 2.0f);
		}

		if (loopOutRel > loopInRel)
		{
			const double loopOutSecs = loopOutRel * totalLen;
			if (loopOutSecs > left && loopInSecs < right)
			{
				const double drawLeft  = std::max(loopInSecs,  left);
				const double drawRight = std::min(loopOutSecs, right);
				const float x1 = static_cast<float>(juce::jmap(drawLeft,  left, right, 0.0, (double)W));
				const float x2 = static_cast<float>(juce::jmap(drawRight, left, right, 0.0, (double)W));
				g.setColour(loopIsActive ? juce::Colours::limegreen.withAlpha(0.25f)
				                        : juce::Colours::blue.withAlpha(0.20f));
				g.fillRect(x1, 0.0f, x2 - x1, static_cast<float>(H));

				const juce::Colour lineCol = loopIsActive ? juce::Colours::limegreen : juce::Colours::blue;
				g.setColour(lineCol);
				if (loopInSecs >= left && loopInSecs <= right)
				{
					const float lx = static_cast<float>(juce::jmap(loopInSecs, left, right, 0.0, (double)W));
					g.drawLine(lx, 0.0f, lx, static_cast<float>(H), 2.0f);
				}
				if (loopOutSecs >= left && loopOutSecs <= right)
				{
					const float rx = static_cast<float>(juce::jmap(loopOutSecs, left, right, 0.0, (double)W));
					g.drawLine(rx, 0.0f, rx, static_cast<float>(H), 2.0f);
				}
			}
		}
	}

	// ── Fixed centre playhead ─────────────────────────────────────────────────
	g.setColour(juce::Colours::grey);
	g.drawRect(W / 2, 0, 1, H);

	// ── Speed deviation overlay ───────────────────────────────────────────────
	if (std::abs(speedRatio - 1.0) > 0.001 && beatGridBpm > 0.0)
	{
		const double pct = (speedRatio - 1.0) * 100.0;
		const juce::String sign = pct > 0 ? "+" : "";
		const juce::String pctText = sign + juce::String(pct, 1) + "%";
		g.setColour(juce::Colour::fromRGBA(0, 0, 0, 160));
		g.fillRect(W - 52, 2, 50, 16);
		g.setColour(theme);
		g.setFont(juce::Font(juce::FontOptions(12.0f)));
		g.drawText(pctText, W - 52, 2, 50, 16, juce::Justification::centred);
	}
}

/**
 * Implementation of resized method for ZoomedWaveform
 *
 * Marks the strip cache dirty so it is rebuilt at the new size on next paint.
 */
void ZoomedWaveform::resized()
{
	stripCacheDirty = true;
}

/**
 * Implementation of changeListenerCallback for ZoomedWaveform.
 *
 * Called by the AudioThumbnail when waveform data loads or updates.
 * Marks the strip cache dirty so it is rebuilt on the next paint call.
 */
void ZoomedWaveform::changeListenerCallback(juce::ChangeBroadcaster* source)
{
	juce::ignoreUnused(source);
	stripCacheDirty = true;
	repaint();
}

/**
 * Override setBandData to invalidate the strip cache when 3-band colour
 * data arrives from WaveformBandAnalyzer, then delegate to the base class
 * to store the data and rebuild the band paths.
 */
void ZoomedWaveform::setBandData(BandDataPtr data)
{
	stripCacheDirty = true;
	WaveformDisplay::setBandData(std::move(data));
}

/**
 * Implementation of rebuildStripCache for ZoomedWaveform.
 *
 * Renders the entire track waveform (0 → totalLength) into a wide Image at
 * a fixed pixel density (component width × 40, capped at 32 000 px).  Each
 * subsequent frame blits the visible sub-region instead of re-calling the
 * expensive audioThumb.drawChannel / drawBandWaveform.
 *
 * Called at most once per: track load, band-data arrival, or window resize.
 */
void ZoomedWaveform::rebuildStripCache()
{
	const double totalLen = audioThumb.getTotalLength();
	const int    h        = getHeight();

	if (totalLen <= 0.0 || h <= 0 || getWidth() <= 0)
	{
		stripCache      = juce::Image();
		stripCacheDirty = false;
		return;
	}

	// Pixel density: enough that the zoomed view at 1× speed fills the
	// component width with crisp pixels. Zoom window ≈ totalLen/40 seconds
	// visible, so we need ~40 × component width pixels total.
	// Cap at 32 000 to stay under ~10 MB per strip (32 000 × 80 × 4 bytes).
	const int w = juce::jlimit(64, 32000, getWidth() * 40);

	stripCache = juce::Image(juce::Image::RGB, w, h, true);
	{
		juce::Graphics cg(stripCache);
		cg.fillAll(UI::bgSurface);

		const juce::Rectangle<int> fullBounds(0, 0, w, h);
		if (bandData != nullptr && !bandData->empty())
			drawBandWaveform(cg, fullBounds, 0.0, totalLen);
		else
			audioThumb.drawChannel(cg, fullBounds, 0.0, totalLen, 0, 0.7f);
	}

	stripCacheDirty = false;
}

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




