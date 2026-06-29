
#pragma once


#include "WaveformDisplay.h"
//==============================================================================

/**
 * Definition of a ZoomedWaveform Component
 *
 * A component that has similar appearance to WaveformDisplay
 * but a different playback control functionality. Acts as an application level
 * Waveform display with playback functionality. Communicates with DJAudioPlayer
 * via the DeckGUI interface
 *
 */
class ZoomedWaveform : public WaveformDisplay
{
public:

	//============================================================================== 

	/**
		* Class Constructor for ZoomedWaveform, initializes member variables.
		*
		* @param juce::AudioFormatManager reference that manages audio formats
		* @param AudioThumbnailCache reference that manages a cache of juce::AudioThumbnail objects
		* @param juce::Colour that defines the theme colour of the component
	*/
	ZoomedWaveform(juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, juce::Colour _colour);

	/**
		* Class Destructor for ZoomedWaveform
	*/
	~ZoomedWaveform() override;

	//============================================================================== 

private:

	//============================================================================== 

	/**
	   * Paints the ZoomedWaveform Component.
	   *
	   * @param juce::Graphics object for the component to draw itself on
   */
	void paint(juce::Graphics&) override;

	/**
		* Set bounds of member components
	*/
	void resized() override;

	//============================================================================== 

	/**
		* Called when mouse is clicked on component
		*
		* @param juce::MouseEvent reference
	*/
	void mouseDown(const juce::MouseEvent& e);

	/**
		* Called when mouse is dragging on component
		*
		* @param juce::MouseEvent reference
	*/
	void mouseDrag(const juce::MouseEvent& e);

	//============================================================================== 

	/**
	 * Override to rebuild the strip cache when thumbnail data changes.
	 */
	void changeListenerCallback(juce::ChangeBroadcaster* source) override;

	/**
	 * Override to invalidate the strip cache when band data changes.
	 */
	void setBandData(BandDataPtr data) override;

	/**
	 * Renders the entire track waveform into stripCache at a fixed
	 * horizontal density. Called once per track/data load or resize;
	 * subsequent frames just blit the visible sub-region.
	 */
	void rebuildStripCache();

	//============================================================================== 

	/// Full-track rendered waveform strip. X maps linearly to track time.
	/// Built once on load/resize; blitted per frame instead of re-rendering.
	juce::Image stripCache;

	/// True when stripCache must be rebuilt before the next blit.
	bool stripCacheDirty = true;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoomedWaveform)
};
