
#pragma once

#include <JuceHeader.h>
#include "DJAudioPlayer.h"
#include "AudioEngine.h"
#include "WaveformDisplay.h"
#include "ZoomedWaveform.h"
#include "JogWheel.h"

#include "CustomLookAndFeel.h"
#include "Library.h"
#include "TrackDataCache.h"
#include "BeatSyncManager.h"
#include "FxIds.h"
#include "DeckQueue.h"
#include "IconTabButton.h"
//==============================================================================

/**
 * Definition of a DeckGUI component
 *
 * A graphics component that performs as a DJ Deck and contains multiple components
 * to control audio functionality on the DJAudioPlayer instance. Being a FileDragAndDropTarget,
 * the component has track loading functionality via file drag and drop. Track loading
 * functionality is also included in an add button, communicating with a Library instance to
 * load selected tracks.
 *
 */
class DeckGUI : public juce::Component,
	public juce::Button::Listener,
	public juce::Slider::Listener,
	public juce::FileDragAndDropTarget,
	public juce::Timer,
	public AudioEngine::Listener
{
public:
	//==============================================================================

	/**
		* Class Constructor for DeckGUI, initializes member variables and configures component details.
		*
		* @param juce::AudioFormatManager reference that manages audio formats
		* @param AudioThumbnailCache reference that manages a cache of juce::AudioThumbnail objects
		* @param ZoomedWaveform pointer
		* @param Library reference to load track selections
		* @param juce::Colour that defines the theme colour of the component
		* @param BeatSyncManager pointer for cross-deck sync coordination (may be null)
		* @param deckIndex 0 for Deck 1, 1 for Deck 2
	*/
	DeckGUI(DJAudioPlayer* player, juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, ZoomedWaveform* _zoomedDisplay, Library& _library, juce::Colour _colour, BeatSyncManager* _syncManager = nullptr, int _deckIndex = 0, AudioEngine* _audioEngine = nullptr);

	/**
	 * Called by BeatSyncManager when sync-related state changes. Refreshes
	 * the SYNC tab controls, slave-slider lockout state, etc.
	 */
	void syncStateChanged();

	/**
		* Class Destructor for DeckGUI, clears dynamically allocated variables.
	*/
	~DeckGUI() override;

	//==============================================================================
	// Public API for the Library sidebar / queue host (MainComponent).

	/// Load a track into this deck. Public wrapper around loadDeck() so
	/// external sidebars can drive the deck without coupling to internals.
	void loadTrack(const track& t);

	/// Append a track to this deck's queue (no playback change).
	void enqueueTrack(const track& t);

	/// Optional callback fired when the user clicks the deck's load button.
	/// Wired by MainComponent to open the appropriate sidebar. If unset, the
	/// load button is a no-op.
	std::function<void(int deckIndex)> onLoadButtonClicked;

	/// Optional callback fired when the user clicks the CUE button.
	/// MainComponent wires this to toggle headphone cue routing via AudioEngine.
	std::function<void(int deckIndex)> onCueButtonClicked;

	/// Update the visual state of the CUE button (lit = active, unlit = inactive).
	void setCueActive(bool active) noexcept;

	//==============================================================================

private:

	//==============================================================================

	/**
		* Paints the DeckGUI Component.
		*
		* @param juce::Graphics object for the component to draw itself on
	*/
	void paint(juce::Graphics&) override;

	/**
		* Set bounds of member components
	*/
	void resized() override;

	/**
		* Called on mouse down to handle right-click context menus on cue buttons.
		*
		* @param juce::MouseEvent object
	*/
	void mouseDown(const juce::MouseEvent& event) override;

	//==============================================================================

	/**
		* Called when button in DeckGUI listener is clicked.
		*
		* @param juce::Button object that has added this component as its listener
	*/
	void buttonClicked(juce::Button* button) override;

	//==============================================================================

	/**
		* Called when slider knob in DeckGUI listener is dragged.
		*
		* @param juce::Slider object that has added this component as its listener
	*/
	void sliderValueChanged(juce::Slider* slider)override;

	//==============================================================================

	/**
		* Called when file is dragged into DeckGUI.
		*
		* @param juce::StringArray object containing the files dragged over the component
		* @return true
	*/
	bool isInterestedInFileDrag(const juce::StringArray& files) override;

	/**
		* Called when file is dropped into DeckGUI.
		*
		* @param juce::StringArray object containing the files dragged over the component
		* @param x position of file dropped
		* @param y position of file dropped
	*/
	void filesDropped(const juce::StringArray& files, int x, int y) override;

	//==============================================================================

	/**
		* Called at specific intervals defined in startTimer() params.
	*/
	void timerCallback() override;

	//==============================================================================

	/**
		* Loads the track object into the DeckGUI.
		*
		* If an AudioEngine is wired, the actual file I/O happens off-thread
		* and the post-load setup runs in onLoadCompleted(). Otherwise the
		* legacy synchronous path runs immediately.
		*
		* @param track object to be loaded into the component
	*/
	void loadDeck(track track);

	/** Updates cue button colours and labels from cueTargets/flash state.
	 *  Called from timerCallback() instead of paint() to avoid per-frame layout work. */
	void updateCueButtons();

	/**
		* Final post-load setup: thumbnails, gain, BPM cache, autoplay.
		* Runs on the message thread once the audio source is ready.
	*/
	void finishLoadDeck();

	/// AudioEngine listener: react to deck load-state transitions.
	void deckLoadingStateChanged(int deckIdx, DJAudioPlayer::LoadingState newState) override;

	//==============================================================================

	/// Pointer to Library component.
	Library* library;

	/// Pointer to DJAudioPlayer instance.
	DJAudioPlayer* player;

	/// Audio format manager (held for off-thread waveform-band analysis).
	juce::AudioFormatManager* formatManager = nullptr;

	/// Optional pointer to the shared AudioEngine for asynchronous track loading.
	AudioEngine* audioEngine = nullptr;

	/// Track currently being loaded asynchronously - deferred post-load step.
	track pendingTrack;

	/// "Loading..." overlay label shown while a track loads off-thread.
	juce::Label loadingLabel { "LOADING", "Loading..." };

	/// Instance of CustomLookAndFeel class.
	CustomLookAndFeel customLookAndFeel;

	/// Unique pointer to juce::Drawable storing the stop button image.
	std::unique_ptr<juce::Drawable> stopButtonImage;

	/// Unique pointer to juce::Drawable storing the hovered stop button image.
	std::unique_ptr<juce::Drawable> stopButtonHoverImage;

	/// Unique pointer to juce::Drawable storing the play button image.
	std::unique_ptr<juce::Drawable> playButtonImage;

	/// Unique pointer to juce::Drawable storing the hovered play button image.
	std::unique_ptr<juce::Drawable>  playButtonHoverImage;

	/// Unique pointer to juce::Drawable storing the load button image.
	std::unique_ptr<juce::Drawable> loadButtonImage;

	/// Unique pointer to juce::Drawable storing the hovered load button image.
	std::unique_ptr<juce::Drawable>  loadButtonHoverImage;

	/// juce::DrawableButton for the play button component
	juce::DrawableButton playButton{ "Play", juce::DrawableButton::ButtonStyle::ImageFitted };

	/// juce::DrawableButton for the load button component
	juce::DrawableButton loadButton{ "Load", juce::DrawableButton::ButtonStyle::ImageFitted };

	/// Unique pointer to juce::Drawable storing the cue (headphone) button image.
	std::unique_ptr<juce::Drawable> cueButtonImage;
	/// Active (cue-on) tinted version of the headphone icon.
	std::unique_ptr<juce::Drawable> cueButtonImageActive;

	/// DrawableButton that routes this deck's audio to the headphone (cue) output.
	juce::DrawableButton cueButton{ "Cue (headphone)", juce::DrawableButton::ButtonStyle::ImageFitted };

	/// Blocks right-click (popup-menu) presses from triggering a DrawableButton click.
	/// Optionally runs an onRightClick callback (e.g. context menu) on mouse-up.
	struct RightClickGuard : public juce::MouseListener
	{
		juce::Button* btn = nullptr;
		std::function<void()> onRightClick;
		bool blockActive = false;

		~RightClickGuard() { if (btn) btn->removeMouseListener(this); }

		void init(juce::Button& b, std::function<void()> cb = {})
		{
			btn = &b;
			onRightClick = std::move(cb);
			b.addMouseListener(this, false);
		}

		void mouseDown(const juce::MouseEvent& e) override
		{
			if (e.mods.isPopupMenu()) {
				btn->setEnabled(false);
				blockActive = true;
			}
		}

		void mouseUp(const juce::MouseEvent& e) override
		{
			if (blockActive) {
				btn->setEnabled(true);
				blockActive = false;
				if (onRightClick)
					onRightClick();
			}
		}
	};

	/// juce::Colour to define the theme of the DeckGUI
	juce::Colour theme;

	/// Rotary slider subclass that passes right-click events to the parent so the
	/// context-menu handler in DeckGUI::mouseDown can show a reset popup.
	struct ResetableKnob : public juce::Slider {
		ResetableKnob() : juce::Slider(juce::Slider::SliderStyle::RotaryVerticalDrag, juce::Slider::TextEntryBoxPosition::NoTextBox) {}
		void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDown(e); }
		void mouseDrag(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDrag(e); }
	};

	/// juce::Label to label the BPM slider
	juce::Label speedLabel{ "SPEED", "SPEED" };

	/// Slider subclass that ignores right-click mouse events so right-click only opens the context menu.
	struct SpeedSlider : public juce::Slider {
		SpeedSlider() : juce::Slider(juce::Slider::SliderStyle::LinearVertical, juce::Slider::TextEntryBoxPosition::NoTextBox) {}
		void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDown(e); }
		void mouseDrag(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu()) juce::Slider::mouseDrag(e); }
	};

	/// SpeedSlider to adjust the resampled speed(BPM) of the audio source.
	SpeedSlider speedSlider;

	/// juce::Label to display the current BPM value
	juce::Label bpmValueLabel{ "BPM_VAL", "---" };

	/// juce::Label to display the speed % deviation
	juce::Label bpmPercentLabel{ "BPM_PCT", "" };

	/// juce::Slider to adjust the low band filter on the audio source.
	ResetableKnob lowBandFilter;

	/// juce::Label to label the low band slider
	juce::Label lbLabel{ "LOW", "LOW" };

	/// juce::Slider to adjust the mid band filter on the audio source.
	ResetableKnob midBandFilter;

	/// juce::Label to label the mid band slider
	juce::Label mbLabel{ "MID", "MID" };

	/// juce::Slider to adjust the high band filter on the audio source.
	ResetableKnob highBandFilter;

	/// juce::Label to label the high band slider
	juce::Label hbLabel{ "HIGH", "HIGH" };

	/// Instance of WaveformDisplay class.
	WaveformDisplay waveformDisplay;

	/// Instance of JogWheel class.
	JogWheel jogWheel;

	/// Pointer to ZoomedWaveform component.
	ZoomedWaveform* zoomedDisplay;

	/// Vector of WaveformDisplay pointers consisting of all Waveform references in DeckGUI.
	std::vector<WaveformDisplay*> displays{ &waveformDisplay , zoomedDisplay, &jogWheel };

	/// Vector of juce::TextButton pointers for cue buttons
	std::vector<juce::TextButton*> cues;

	/// Map of juce::TextButton pointers to std::pair of double and floats. Maps cue buttons to a pair containing double for audio position and float for hue colour of cue button.
	std::map<juce::TextButton*, std::pair<double, float>> cueTargets;

	/// Previous audio player position used to compare to the current player position.
	double prevPlayerPos;

	/// Determines if the player can continue playing
	bool canContinue = true;

	/// Determines if the deck playing mode.
	bool modeIsPlaying = false;

	/// Determines the WaveformDisplay object being dragged in displays vector.
	int draggedIndex;

	/// Determines if cue buttons should be lit up with their hue colours in cueTargets map.
	bool flash;

	/// Simple counter that increments every call to timerCallback
	int counter;

	/// Determines the average root mean square value derived from the DJAudioPlayer
	float volRMS;

	//==============================================================================
	// Tab mode: Hot Cues vs Beat Grid

	/// Enum for the cue/grid/jump/loop/quantize/sync/fx tab mode
	enum class CueGridMode { HotCues, BeatGrid, BeatJump, Loop, Quantize, Sync, PadFx, BeatFx, ReleaseFx };

	/// Current tab mode
	CueGridMode cueGridMode = CueGridMode::HotCues;

	/// Tab button for hot cues
	IconTabButton cueTabButton{ "Hot Cues", BinaryData::iconCues_svg, BinaryData::iconCues_svgSize };

	/// Tab button for beat grid controls
	IconTabButton gridTabButton{ "Beat Grid", BinaryData::iconGrid_svg, BinaryData::iconGrid_svgSize };

	/// Tab button for beat jump controls
	IconTabButton jumpTabButton{ "Beat Jump", BinaryData::iconJump_svg, BinaryData::iconJump_svgSize };

	/// Tab button for loop controls
	IconTabButton loopTabButton{ "Loop", BinaryData::iconLoop_svg, BinaryData::iconLoop_svgSize };

	/// Tab button for sync controls
	IconTabButton syncTabButton{ "Sync", BinaryData::iconSync_svg, BinaryData::iconSync_svgSize };

	/// Tab button for Pad FX (momentary effects).
	IconTabButton padFxTabButton{ "Pad FX", BinaryData::iconPadFx_svg, BinaryData::iconPadFx_svgSize };

	/// Tab button for Beat FX (latched effects).
	IconTabButton beatFxTabButton{ "Beat FX", BinaryData::iconBeatFx_svg, BinaryData::iconBeatFx_svgSize };

	/// Tab button for Release FX (momentary release effects).
	IconTabButton releaseFxTabButton{ "Release FX", BinaryData::iconReleaseFx_svg, BinaryData::iconReleaseFx_svgSize };

	//==============================================================================
	// Beat Grid Controls

	/// Label for the grid BPM editor
	juce::Label gridBpmLabel{ "GRID_BPM", "BPM:" };

	/// Editable text field for BPM override
	juce::TextEditor gridBpmEditor;

	/// Button to nudge grid offset earlier
	juce::TextButton gridNudgeLeftBtn{ "<" };

	/// Button to nudge grid offset later
	juce::TextButton gridNudgeRightBtn{ ">" };

	/// Label for nudge buttons
	juce::Label gridOffsetLabel{ "GRID_OFF", "OFFSET" };

	/// Tap tempo button
	juce::TextButton tapTempoBtn{ "TAP" };

	/// Button to reset grid to detected values
	juce::TextButton gridResetBtn{ "RESET" };

	/// Timestamps of tap tempo presses
	std::vector<double> tapTimes;

	//==============================================================================
	// Beat Jump Controls

	/// Beat jump buttons: -16, -8, -4, -1, +1, +4, +8, +16
	juce::TextButton jumpBackward16Btn{ "-16" };
	juce::TextButton jumpBackward8Btn{ "-8" };
	juce::TextButton jumpBackward4Btn{ "-4" };
	juce::TextButton jumpBackward1Btn{ "-1" };
	juce::TextButton jumpForward1Btn{ "+1" };
	juce::TextButton jumpForward4Btn{ "+4" };
	juce::TextButton jumpForward8Btn{ "+8" };
	juce::TextButton jumpForward16Btn{ "+16" };

	/// Label for beat jump section
	juce::Label jumpLabel{ "BEAT JUMP", "BEAT JUMP" };

	/// Sets visibility of beat jump controls
	void setBeatJumpControlsVisible(bool visible);

	//==============================================================================
	// Loop Controls

	/// Loop IN button
	juce::TextButton loopInBtn{ "IN" };

	/// Loop OUT button
	juce::TextButton loopOutBtn{ "OUT" };

	/// Reloop (toggle) button
	juce::TextButton reloopBtn{ "RELOOP" };

	/// Halve loop length button
	juce::TextButton loopHalveBtn{ juce::CharPointer_UTF8("\xc3\x97\xc2\xbd") };

	/// Double loop length button
	juce::TextButton loopDoubleBtn{ juce::CharPointer_UTF8("\xc3\x97\x32") };

	/// Clear loop button
	juce::TextButton loopClearBtn{ "CLR" };

	/// Sets visibility of loop controls
	void setLoopControlsVisible(bool visible);

	//==============================================================================
	// Quantize Controls

	/// Struct describing a pending quantized action
	struct PendingQuantizeAction {
		enum class Type { None, PlayStart, PlayStop, LoopIn, LoopOut,
		                  LoopHalve, LoopDouble, BeatJump, HotCueJump, HotCueSet };
		Type              type           = Type::None;
		double            fireAtRealTime = 0.0;
		juce::Button*     srcButton      = nullptr;
		int               beatJumpBeats  = 0;
		double            hotCueRelPos   = -1.0;
		double            hotCueSetPos   = -1.0;
		float             hotCueHue      = 0.0f;
		juce::TextButton* cueButtonTarget = nullptr;
		bool isValid() const { return type != Type::None; }
		void clear()         { *this = PendingQuantizeAction{}; }
	};

	/// Current pending quantized action
	PendingQuantizeAction pendingAction;

	/// Tab button for quantize controls
	IconTabButton quantizeTabButton{ "Quantize", BinaryData::iconQuantize_svg, BinaryData::iconQuantize_svgSize };

	/// Label for quantize combo box
	juce::Label quantizeLabel{ "Q_LABEL", "QUANTIZE:" };

	/// ComboBox to select the quantize subdivision
	juce::ComboBox quantizeComboBox;

	/// Returns the quantize interval in seconds (0 = off)
	double getQuantizeIntervalSecs() const;

	/// Returns the next beat-grid-aligned quantize boundary in track seconds
	double getNextQuantizeBoundarySecs(double currentSecs) const;

	/// Queue an action for quantized execution, or execute immediately if quantize is off
	void queueOrExecute(PendingQuantizeAction::Type type, juce::Button* btn,
	                    int beats = 0, double hotCueRelPos = -1.0,
	                    double hotCueSetPos = -1.0, float hotCueHue = 0.0f,
	                    juce::TextButton* cueBtnTarget = nullptr);

	/// Cancel and revert the current pending action
	void clearPendingAction();

	/// Fire the current pending action
	void executePendingAction();

	/// Sets visibility of quantize controls
	void setQuantizeControlsVisible(bool visible);

	//==============================================================================
	// Sync Controls

	/// Pointer to shared BeatSyncManager (owned by MainComponent). Nullable.
	BeatSyncManager* syncManager = nullptr;

	/// Index of this deck within the BeatSyncManager (0 = Deck 1, 1 = Deck 2).
	int deckIndex = 0;

	/// Toggles this deck as sync master (only one master at a time).
	juce::TextButton masterToggleBtn{ "MASTER" };

	/// Engages/disengages sync on this deck (slave only).
	juce::TextButton syncEngageBtn{ "SYNC" };

	/// Slave half/double multiplier override buttons.
	juce::TextButton multHalfBtn{ juce::CharPointer_UTF8("\xc3\x97\xc2\xbd") };
	juce::TextButton multOneBtn{ juce::CharPointer_UTF8("\xc3\x97\x31") };
	juce::TextButton multTwoBtn{ juce::CharPointer_UTF8("\xc3\x97\x32") };

	/// Displays the resolved target BPM the slave is locked to.
	juce::Label targetBpmLabel{ "TGT_BPM", "-> ---" };

	/// Displays human-readable sync status ("SYNCED", "OUT OF RANGE", etc.).
	juce::Label syncStatusLabel{ "SYNC_ST", "" };

	/// Unique pointer to juce::Drawable storing the fast-sync button image.
	std::unique_ptr<juce::Drawable> fastSyncBtnImage;
	/// Green-tinted version of the bolt icon shown while sync is engaged.
	std::unique_ptr<juce::Drawable> fastSyncBtnImageActive;

	/// DrawableButton for fast (one-click) sync, placed near play/load buttons.
	juce::DrawableButton fastSyncBtn{ "Fast Sync", juce::DrawableButton::ButtonStyle::ImageFitted };

	RightClickGuard playBtnGuard, loadBtnGuard, cueBtnGuard, fastSyncGuard;

	/// Resets sync + master + speed back to defaults (track plays normally).
	juce::TextButton syncResetBtn{ "RESET" };

	/// Snap-quantisation combo for sync phase alignment (1 BAR / 1/2 / 1/4).
	juce::ComboBox snapBox;

	/// Sets visibility of sync tab controls.
	void setSyncControlsVisible(bool visible);

	/// Pushes current speed ratio to all waveform displays for live stretch/squish.
	void propagateSpeedToDisplays();

	//==============================================================================

	/// Identity hash of the currently loaded track (legacy)
	juce::String currentTrackIdentity;

	/// Content-based file hash of the currently loaded track
	juce::String currentFileHash;

	/// Helper to save current beat grid + detected BPM to the track data cache
	void saveTrackData(const BeatGrid& grid);

	/// Sets visibility of cue buttons
	void setCueButtonsVisible(bool visible);

	/// Sets visibility of grid controls
	void setGridControlsVisible(bool visible);

	/// Updates the grid BPM editor text from the player
	void updateGridBpmDisplay();

	//==============================================================================
	// FX panels (Pad / Beat / Release)

	/**
	 * Small helper button used as an FX tile.
	 *
	 * - Left-click & hold engages the effect; releasing the mouse disengages
	 *   it (Pioneer-style momentary behavior).
	 * - Right-click opens the parameter modal for the tile's effect.
	 */
	struct MomentaryFxTile : public juce::TextButton
	{
		std::function<void(bool)> onEngageChanged;
		std::function<void()>     onShowParameters;
		void mouseDown(const juce::MouseEvent& e) override
		{
			if (e.mods.isPopupMenu()) { if (onShowParameters) onShowParameters(); return; }
			juce::TextButton::mouseDown(e);
			if (onEngageChanged) onEngageChanged(true);
		}
		void mouseUp(const juce::MouseEvent& e) override
		{
			juce::TextButton::mouseUp(e);
			if (! e.mods.isPopupMenu() && onEngageChanged) onEngageChanged(false);
		}
	};

	/// Pad FX tiles (4×2 grid, 8 slots - 7 effects + 1 spare).
	std::vector<std::unique_ptr<MomentaryFxTile>> padFxTiles;

	/// Release FX tiles (3 slots: V.Brake, R.Echo, Back Spin).
	std::vector<std::unique_ptr<MomentaryFxTile>> releaseFxTiles;

	/// Beat FX selector (drop-down of all beat-FX algorithms).
	juce::ComboBox beatFxSelector;

	/// Beat FX beat-division selector (1/16 … 4 beats).
	juce::ComboBox beatFxDivisionBox;

	/// Beat FX engage toggle (latched).
	juce::TextButton beatFxOnButton{ "ON / OFF" };

	/// Wet/dry knob for the Beat FX (visible parameter).
	juce::Slider beatFxWetSlider;
	juce::Label  beatFxWetLabel{ "B_FX_WET", "WET" };

	/// Opens the parameter modal for the active Beat FX algorithm.
	juce::TextButton beatFxEditButton{ "EDIT" };

	/// Sets visibility of pad FX controls.
	void setPadFxControlsVisible(bool visible);

	/// Sets visibility of beat FX controls.
	void setBeatFxControlsVisible(bool visible);

	/// Sets visibility of release FX controls.
	void setReleaseFxControlsVisible(bool visible);

	/// Builds the Pad / Beat / Release FX UI controls (called from constructor).
	void buildFxPanels();

	/// Refreshes the on-screen labels and selected-state of all FX tiles to
	/// match the FxChain inside the player. Safe to call from the message thread.
	void refreshFxUi();

	/// Posts an FxSelect command for the given category/processor index.
	void postFxSelect(FxCategory cat, int processorIndex);

	/// Posts an FxSetEngaged command for the given category.
	void postFxEngaged(FxCategory cat, bool engaged);

	/// Last BPM pushed via FxSetBpm (avoid spamming the FIFO every timer tick).
	double lastFxBpmPushed { 0.0 };

	/// Index of the FX tile currently held down (so we can release it on
	/// timer or unload). -1 = none.
	int padFxHeldIndex     { -1 };
	int releaseFxHeldIndex { -1 };

	/// Opens the parameter modal anchored at the given component's bounds.
	void showFxParameterModal(FxCategory cat, juce::Component* anchor);

	/// Per-deck playback queue, shown as a compact widget on the deck strip.
	std::unique_ptr<DeckQueue> queueWidget;

	//==============================================================================
	// Phase 1 - Structural layout containers (Phase 2 will re-parent children).

	/// Top header area: track title, BPM display and utility controls.
	juce::Component topHeaderContainer;

	/// Hosts the full-waveform display band.
	juce::Component waveformContainer;

	/// Transport row: play button, cue button, load button and speed slider.
	juce::Component transportContainer;

	/// Mixer knob area: Low / Mid / High EQ knobs and their labels.
	juce::Component mixerContainer;

	/// Jog wheel and its surrounding utility buttons (sync, cue icon).
	juce::Component jogWheelContainer;

	/// Full-height vertical tab rail on the deck's outer edge (hosts the 9
	/// IconTabButtons). Lives outside transportContainer so it can span
	/// from the absolute top to the absolute bottom of the deck panel.
	juce::Component sidebarContainer;

	//==============================================================================
	// Phase 1 - New utility button declarations.

	/// Unique pointer to juce::Drawable for the library button icon.
	std::unique_ptr<juce::Drawable> libraryButtonImage;

	/// Unique pointer to juce::Drawable for the hovered library button icon.
	std::unique_ptr<juce::Drawable> libraryButtonHoverImage;

	/// DrawableButton that opens the track library sidebar for this deck.
	juce::DrawableButton libraryButton{ "Library", juce::DrawableButton::ButtonStyle::ImageFitted };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckGUI);
};
