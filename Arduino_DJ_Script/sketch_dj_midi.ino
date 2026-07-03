// Needed Libraries
// - MIDI Library by Francois Best
// - Control Surface by Pieter P
#include <Control_Surface.h>

// 1. Instantiate the MIDI Interface
// Since your Uno's USB chip is flashed to be a MIDI device, 
// we use standard hardware serial at the MIDI baud rate (31250).
USBMIDI_Interface midi;

// --- BUTTONS ---
// Define an array of multiple buttons
NoteButton buttons[] {
  {10, {MIDI_Notes::C(4), CHANNEL_1}}, // Button on Pin 10 sends C4
  {9,  {MIDI_Notes::D(4), CHANNEL_1}}, // Button on Pin 9 sends D4
  {8,  {MIDI_Notes::E(4), CHANNEL_1}}  // Button on Pin 8 sends E4
};

// 3. Define the Potentiometers
// Sends MIDI Control Change (CC) messages when turned.
// Map Pin A4 to CC 16, and Pin A5 to CC 17.
CCPotentiometer potentiometers[] {
  {A0, {MIDI_CC::Pan, CHANNEL_1}},
  {A1, {MIDI_CC::Channel_Volume, CHANNEL_1}},
};

// --- LEDS ---
// Define an array of multiple LEDs
NoteLED leds[] {
  {11, {MIDI_Notes::C(4), CHANNEL_1}}, // LED on Pin 11 listens for C4 (Note 60)
  {12, {MIDI_Notes::D(4), CHANNEL_1}}, // LED on Pin 12 listens for D4 (Note 62)
  {13, {MIDI_Notes::E(4), CHANNEL_1}}  // LED on Pin 13 listens for E4 (Note 64)
};

void setup() {
  randomSeed(micros());

  // Control_Surface.begin() automatically sets up all pin modes, 
  // starts the serial connection, and initializes the components.
  Control_Surface.begin();
}

void loop() {
  // Control_Surface.loop() continuously checks the pots and buttons, 
  // sends MIDI if they changed, and listens for incoming MIDI to update the LED.
  Control_Surface.loop();
}