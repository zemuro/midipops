// -----------------------------------------------------------------------------
// Midipops - Arduino Nano based MIDI Drum Machine
// Polyphonic 8-bit, 20kHz audio sample playback via PWM on digital pin 11.
// Connect MIDI input (via optocoupler) to RX pin (D0 / RX).
// Recommended RC low-pass filter on pin 11: 1kΩ resistor and 10nF capacitor.
//
// Credits:
// Original O2 Source Code by Jan Ostman:
// https://web.archive.org/web/20170107051059/https://janostman.wordpress.com/the-o2-source-code/
// Additional Info & Hardware: 
// https://bloghoskins.blogspot.com/2016/11/korg-mini-pops-diy-drum-machine.html
//
// License: Creative Commons Attribution 4.0 International (CC BY 4.0)
// CC0/Public Domain for original Jan Ostman DSP code.
// -----------------------------------------------------------------------------

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "include/kit_b.h"

// Struct representing an active, currently playing sound
struct Voice {
  const unsigned char* sample; // Pointer to the sample data in flash memory
  uint16_t length;             // Total number of samples in the array
  uint16_t position;           // Current playback position offset
  uint8_t velocity;            // MIDI velocity for amplitude scaling
  bool active;                 // True if the sound is currently playing
};

#define MAX_VOICES NUM_SAMPLES
Voice voices[MAX_VOICES];

ISR(TIMER1_COMPA_vect) {
  int16_t total = 0; // Accumulator for the software mixer
  
  // Mix all currently active voices directly in the interrupt
  for (uint8_t i = 0; i < MAX_VOICES; i++) {
    if (voices[i].active) {
      // Read sample byte from PROGMEM and convert to signed (-128 to 127)
      int8_t s = pgm_read_byte_near(voices[i].sample + voices[i].position) - 128;
      
      // --- LO-FI VELOCITY SCALING (16 Steps) ---
      // We reduce bit depth by right-shifting the signal based on velocity bands.
      // This creates a gritty, stepped amplitude control reminiscent of early digital drums.
      uint8_t v = voices[i].velocity >> 3; // 16 steps (0-15)
      int8_t s_scaled = s;
      if (v < 14) s_scaled >>= 1;
      if (v < 10) s_scaled >>= 1;
      if (v < 6) s_scaled >>= 1;
      if (v < 2) s_scaled >>= 1;
      
      total += s_scaled;
      voices[i].position++;
      
      // Stop voice when it reaches the end of its sample array
      if (voices[i].position >= voices[i].length) {
        voices[i].active = false;
      }
    }
  }
  
  // Limit (clip) the summed signal to the 8-bit signed range to prevent wrapping artifacts
  if (total < -127) total = -127;
  if (total > 127) total = 127;                           
  
  // Output directly to the 8-bit DAC on Timer 2 (Pin 11)
  OCR2A = total + 128;
}

void triggerDrum(uint8_t index, uint8_t velocity) {
  if(index < MAX_VOICES) {
    cli(); // Disable interrupts to ensure atomic trigger
    voices[index].position = 0;
    voices[index].active = true;
    voices[index].velocity = velocity;
    sei(); // Re-enable interrupts
  }
}

// MIDI global state machine variables
uint8_t midiState = 0; // 0=Wait for status, 1=Wait for Note, 2=Wait for Velocity
uint8_t midiNote = 0;  // Holds the received MIDI note number
const uint8_t MIDI_CHANNEL = 10; // Listen to Channel 10 (Standard MIDI Drum Channel)

// Process a MIDI Note On request
inline void handleNoteOn(uint8_t note, uint8_t velocity) {
  if (velocity == 0) return;
  for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
    if (note == pgm_read_byte(&kit_notes[i])) {
      triggerDrum(i, velocity);
      break;
    }
  }
}

// Reads serial stream and parses basic MIDI Note On messages
void processMIDI() {
  // On Arduino Nano, Serial uses hardware RX pin.
  if (Serial.available()) {
    PORTB |= _BV(PORTB5); // Turn ON built-in LED (Pin 13)
  } else {
    PORTB &= ~_BV(PORTB5); // Turn OFF built-in LED
  }

  while (Serial.available()) {
    uint8_t byte = Serial.read();
    
    if (byte & 0x80) { 
      // Status byte
      uint8_t type = byte & 0xF0;
      uint8_t channel = (byte & 0x0F) + 1;
      if (type == 0x90 && channel == MIDI_CHANNEL) {
        midiState = 1; // Expecting note
      } else {
        midiState = 0; // Ignore other messages for now
      }
    } else {
      // Data byte
      if (midiState == 1) {
        midiNote = byte;
        midiState = 2; // Expecting velocity
      } else if (midiState == 2) {
        handleNoteOn(midiNote, byte);
        // Running status: expect next note within the same Note On status
        midiState = 1;
      }
    }
  }
}

void setup() {
    // Initialize voices mapping based on the chosen kit
    for (uint8_t i = 0; i < MAX_VOICES; i++) {
      voices[i].sample = (const unsigned char*)pgm_read_word(&kit_ptrs[i]);
      voices[i].length = pgm_read_word(&kit_lens[i]);
      voices[i].velocity = 0;
      voices[i].position = 0;
      voices[i].active = false;
    }

    OSCCAL=0xFF; // Set internal oscillator max speed if applicable

    // Setup MIDI Baud Rate
    Serial.begin(31250);

    // 8-bit PWM DAC pin
    pinMode(11, OUTPUT);
    // Pin 13 is the built-in LED on Nano
    pinMode(13, OUTPUT);

    // Set up Timer 1 as a 20kHz interrupt timer for audio sampling
    // It will trigger ISR(TIMER1_COMPA_vect) every 50 microseconds
    cli(); // Disable interrupts during configuration
    TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12); // Configure for CTC mode
    TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));    
    TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10); // No prescaler (16MHz clock)
    TIMSK1 |= _BV(OCIE1A);   // Enable Timer 1 Compare Match A interrupt
    OCR1A = 800; // Counter value for 20kHz: 16MHz / 800 = 20kHz
    sei(); // Re-enable interrupts

    // Set up Timer 2 to output 8-bit Fast PWM on Pin D11 (Arduino Nano)
    // This acts as a simple digital-to-analog converter (DAC)
    ASSR &= ~(_BV(EXCLK) | _BV(AS2)); // Use internal clock
    TCCR2A |= _BV(WGM21) | _BV(WGM20); // Fast PWM Mode
    TCCR2B &= ~_BV(WGM22);
    TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0); // Clear OC2A on Compare Match, set at BOTTOM
    TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0)); // Disable OC2B output
    TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10); // No prescaler, run at full 16MHz clock
    OCR2A = 128; // Start at center voltage (DC offset for 8-bit unsigned audio is 128)
}

void loop() {  
  while(1) { 
    // Quickly handle any incoming MIDI messages
    processMIDI();
  }
}
