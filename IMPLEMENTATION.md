
# IMPLEMENTATION.md

## 1. Project Context & Architecture
- **Goal:** Refactor the DJ Deck UI (`DeckGUI`) from an absolute-positioned layout to a responsive, container-based architecture. Standardize utility buttons (Headphone, Sync, Library) to inherit the visual design language of the primary transport controls (Play/Pause).
- **Tech Stack & Dependencies:** - C++17 or later
  - JUCE Framework (v7/v8 depending on local setup)
  - CMake (Build System)
  - No external package managers required.
- **File Structure:** ```text
  ├── assets/
  │   ├── iconHeadphone.svg
  │   ├── iconSync.svg
  │   └── iconLibrary.svg (or similar local asset)
  ├── src/
  │   ├── UIConstants.h           # Centralized layout metrics
  │   ├── DeckGUI.h               # Header declarations for new containers and buttons
  │   ├── DeckGUI.cpp             # Implementation of resizing and re-parenting
  │   ├── CustomLookAndFeel.h     # Styling overrides
  │   └── CustomLookAndFeel.cpp   # Standardized button paint logic
  ```
- **Attention Points:** - **Responsive Sizing:** Strictly avoid hardcoded pixel bounds in `resized()`. Use `juce::Rectangle::removeFromTop/Left/etc.` and `juce::FlexBox`.
  - **DSGVO / Data Privacy (CRITICAL):** The application must not phone home. All UI assets (SVGs, PNGs) and Typography (Fonts) MUST be loaded locally from the binary using `BinaryData` or local file streams. Do not implement any web-based font loaders (e.g., Google Fonts API).

---

## 2. Execution Phases

#### Phase 1: Layout Constants & Scaffolding
- [ ] **Step 1.1:** In `src/UIConstants.h`, define `constexpr` values for global layout metrics (e.g., `deckMargin`, `componentPadding`, `headerHeight`, `waveformHeight`). 
- [ ] **Step 1.2:** In `src/DeckGUI.h`, declare empty `juce::Component` objects to act as structural containers: `topHeaderContainer`, `waveformContainer`, `transportContainer`, `mixerContainer`, and `jogWheelContainer`.
- [ ] **Step 1.3:** In `src/DeckGUI.h`, declare any missing new buttons requested (e.g., `juce::TextButton libraryButton;` or `DrawableButton`).
- [ ] **Verification:** Run `cmake --build build` (or your configured build task) to ensure the newly declared members compile without errors.

#### Phase 2: Component Re-parenting & Data Flow
- [ ] **Step 2.1:** In `src/DeckGUI.cpp` (Constructor), call `addAndMakeVisible` for the new containers directly on the `DeckGUI` object.
- [ ] **Step 2.2:** In `src/DeckGUI.cpp` (Constructor), change the parent of existing buttons and sliders. Instead of adding them to the main `DeckGUI`, add them to their respective logical containers (e.g., `transportContainer.addAndMakeVisible(&playButton);`).
- [ ] **Verification:** Run the build command. The project must compile. Running the app at this stage will show stacked/invisible components at [0,0], which is expected.

#### Phase 3: Responsive Slicing Architecture
- [ ] **Step 3.1:** In `src/DeckGUI.cpp`, delete the existing absolute-position math inside the `resized()` method.
- [ ] **Step 3.2:** In `src/DeckGUI.cpp` `resized()`, implement top-down UI slicing using `getLocalBounds().reduced()`. Slice out the `topHeaderContainer` and `waveformContainer` using `removeFromTop()`. Split the remaining area into left (`jogWheelContainer`, `transportContainer`) and right (`mixerContainer`).
- [ ] **Step 3.3:** In `src/DeckGUI.cpp`, implement localized layout logic for the sub-containers. Assign bounds to the specific buttons (Play, Pause, Sync, Headphone) inside their parent containers using `juce::FlexBox` to ensure even spacing.
- [ ] **Verification:** Compile and launch the application. Resize the main window. Verify that all components scale dynamically and no elements overlap or clip unexpectedly.

#### Phase 4: Button Standardization & LookAndFeel
- [ ] **Step 4.1:** In `src/CustomLookAndFeel.cpp`, locate the `drawButtonBackground` and `drawButtonText` (or `drawDrawableButton`) overrides.
- [ ] **Step 4.2:** Refactor the painting logic so that the `Headphone`, `Sync`, and newly added `Library` buttons utilize the exact same styling branch (SVG scaling, color palettes, border radius) as the `Play` and `Pause` buttons. Ensure hover and down states are applied universally.
- [ ] **Step 4.3:** In `src/DeckGUI.cpp` (Constructor), assign the updated `CustomLookAndFeel` instance to the new Library, Headphone, and Sync buttons if not already applied globally.
- [ ] **Verification:** Launch the application. Click and hover over Play, Pause, Sync, Headphone, and Library buttons. Verify visually that their geometries, colors, and interactive states match perfectly.

---

## 3. Global Testing Strategy
- **Window Resizing Edge Cases:** Rapidly resize the application window to its minimum allowed size constraints to ensure the `juce::FlexBox` and `juce::Rectangle` slicing handles tight spaces gracefully without throwing assertions.
- **Asset Loading Verification:** Disconnect the machine from the internet and launch the compiled binary to explicitly verify DSGVO/local-asset compliance (ensuring no missing font boxes or broken SVGs).
- **Leak Detection:** Close the application and monitor the console output to verify no memory leaks are reported by `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` regarding the newly added Library buttons or containers.
