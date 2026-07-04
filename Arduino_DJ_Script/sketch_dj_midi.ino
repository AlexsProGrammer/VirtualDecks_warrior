#include <Control_Surface.h>

// 1. Instantiate the MIDI Interface
USBMIDI_Interface midi;

// ==========================================
// DECK CONTROL (2 Decks)
// ==========================================
// Create a Bank with 2 slots (Deck 1 and Deck 2)
Bank<2> deckBank;

// Pin 9 will toggle between the two banks (wraps around automatically)
IncrementSelector<2> deckSelector {deckBank, 9};

// Play/Pause Button (Pin 12)
// Bank 0 = Note 60 (C4) | Bank 1 = Note 61 (C#4)
Bankable::NoteButton playButton {deckBank, 12, {MIDI_Notes::C(4), CHANNEL_1}};

// Cue Button (Pin 11)
// Bank 0 = Note 62 (D4) | Bank 1 = Note 63 (D#4)
Bankable::NoteButton cueButton {deckBank, 11, {MIDI_Notes::D(4), CHANNEL_1}};


// ==========================================
// PARAMETER CONTROL (5 Modes for Pots)
// ==========================================
// Create a Bank with 5 slots for the potentiometers
Bank<5> paramBank;

// This incredible class handles the 5 buttons AND their 5 LEDs automatically.
// When you press a button, it switches the bank and turns on only that specific LED.
ManyButtonsSelectorLEDs<5> paramSelector {
  paramBank,
  {{7, 5, 3, A2, A4}}, // Mod Buttons: Filter, Vol, EQ High, EQ Mid, EQ Low (Moved to A4!)
  {{8, 6, 4, 2, A3}}   // Mod LEDs:    Filter, Vol, EQ High, EQ Mid, EQ Low
};

// Left Potentiometer (Pin A1)
// Base CC is 10. The Bank shifts it automatically: 10, 11, 12, 13, 14
Bankable::CCPotentiometer leftPot {paramBank, A1, {10, CHANNEL_1}};

// Right Potentiometer (Pin A0)
// Base CC is 20. The Bank shifts it automatically: 20, 21, 22, 23, 24
Bankable::CCPotentiometer rightPot {paramBank, A0, {20, CHANNEL_1}};


// ==========================================
// SOFTWARE-CONTROLLED LEDS
// ==========================================
// These listen to your DJ app to know when a track is actually playing
NoteLED playDeck1LED {A5, {MIDI_Notes::C(4), CHANNEL_1}};  // Listens to Note 60
NoteLED playDeck2LED {13, {MIDI_Notes::Db(4), CHANNEL_1}}; // Listens to Note 61


void setup() {
  // Set up Pin 10 manually to act as our Deck Modifier indicator
  pinMode(10, OUTPUT);
  
  // Initialize Control Surface
  Control_Surface.begin();
}

void loop() {
  // Process all MIDI, buttons, pots, and standard LEDs
  Control_Surface.loop();

  // Manually update the Deck Modifier LED (Pin 10)
  // getSelection() returns 0 for Deck 1, and 1 for Deck 2.
  if (deckBank.getSelection() == 1) {
    digitalWrite(10, HIGH); // LED ON when on Deck 2
  } else {
    digitalWrite(10, LOW);  // LED OFF when on Deck 1
  }
}