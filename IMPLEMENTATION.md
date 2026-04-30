I see exactly where the last layout plan fell short. When you want an element like the sidebar to take up the *absolute full height* of the deck, you have to slice that out of the layout bounds *first* before doing any top-to-bottom slicing. 

Let's restructure the layout logic so it perfectly matches your drawing and resolves the button theme inconsistencies. Here is a precise, short implementation plan you can hand to your AI agent.

### **Implementation Plan: Deck Layout Correction**

**Objective:** Refactor `DeckGUI::resized()` to strictly follow the hand-drawn wireframe hierarchy, re-position the 4 main buttons around the jog wheel, enlarge the queue, and unify the button themes.

#### **Phase 1: Base Slicing & The Sidebar**
- [ ] **Target:** `src/DeckGUI.cpp` -> `resized()`
- [ ] **Logic:** Grab the total local bounds first. Extract the sidebar *before* anything else so it spans top-to-bottom.
  ```cpp
  auto deckBounds = getLocalBounds();
  // 1. Slice full-height sidebar first (Left for Deck 1, Right for Deck 2)
  auto sidebarBounds = deckBounds.removeFromLeft(40); // adjust width and side based on deck index
  sidebarContainer.setBounds(sidebarBounds);
  ```

#### **Phase 2: Top-to-Bottom Stacking**
- [ ] **Target:** `src/DeckGUI.cpp` -> `resized()`
- [ ] **Logic:** With the remaining `deckBounds`, slice off the top and bottom sections horizontally.
  ```cpp
  // 2. Waveforms at the absolute top of the remaining space
  waveformContainer.setBounds(deckBounds.removeFromTop(120)); // Adjust height
  
  // 3. Tab Area immediately below the waveform, using full remaining width
  tabAreaContainer.setBounds(deckBounds.removeFromTop(80)); 
  
  // 4. Queue at the bottom, given more height
  queueContainer.setBounds(deckBounds.removeFromBottom(150)); // Increased height
  ```

#### **Phase 3: The Central Deck Area & 4-Corner Buttons**
- [ ] **Target:** `src/DeckGUI.cpp` -> `resized()`
- [ ] **Logic:** The remaining `deckBounds` is now the center box. We need to manually place the spinning disk, the 4 corner buttons, the BPM, and the knobs.
  - Calculate a center square for the `JogWheel` and set its bounds.
  - Position the 4 buttons (e.g., Play, Cue, Sync, etc.) by placing them relative to the `JogWheel`'s `getX()`, `getY()`, `getRight()`, and `getBottom()` coordinates.
  - Place the `bpmLabel` to the immediate right or left of the `JogWheel`.
  - Slice a vertical strip from the right side for the `Speed` slider.
  - Divide the remaining bottom portion of this center area equally for the `Low`, `Mid`, and `High` knobs.

#### **Phase 4: Unified Button Theming**
- [ ] **Target:** `src/CustomLookAndFeel.cpp` (or wherever button styling is handled)
- [ ] **Logic:** The 4 buttons around the disk currently have mismatched themes (some have dark backgrounds, some are white/transparent circles). 
  - Update `drawButtonBackground` and `drawButtonText` (or their specific image drawing logic if using `DrawableButton`).
  - Ensure all 4 corner buttons use the exact same background shape (e.g., circular outline), background color, border thickness, and icon scale so they look like a matching set.

#### **Phase 5: Verification**
- [ ] **Action:** Compile and run.
- [ ] **Checklist:**
  - Sidebar reaches the absolute top and bottom edge.
  - Waveform is at the top, followed by the full-width tab area.
  - Play/Cue buttons are explicitly positioned in the 4 corners surrounding the spinning disk with a matching visual theme.
  - Queue is noticeably taller at the bottom.
```