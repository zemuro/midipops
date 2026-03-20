// -----------------------------------------------------------------------------
// Midipops - Arduino Pro Micro (ATmega32U4) Port
// Polyphonic 8-bit, 20kHz audio sample playback via PWM on digital pin 5 (Timer3).
// Connect MIDI input (via optocoupler) to RX pin (UART RX).
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
#include "MIDIUSB.h"

#include "include/kit_b.h"

// Struct representing an active, currently playing sound
struct Voice {
  const unsigned char* sample; // Pointer to the sample data in flash memory
  uint16_t length;             // Total number of samples in the array
  uint16_t position;           // Current playback position offset
  bool active;                 // True if the sound is currently playing
};

#define MAX_VOICES NUM_SAMPLES
Voice voices[MAX_VOICES];

ISR(TIMER1_COMPA_vect) {
  int16_t total = 0; // Accumulator for the software mixer
  
  // Mix all currently active voices directly in the interrupt
  for (uint8_t i = 0; i < MAX_VOICES; i++) {
    if (voices[i].active) {
      // Read sample byte from PROGMEM, convert to signed (-128 to 127) and cut volume by 50%
      int8_t s = pgm_read_byte_near(voices[i].sample + voices[i].position) - 128;
      total += (s / 2);
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
  
  // Output directly to the 8-bit DAC on Timer 3 (Pin 5)
  OCR3A = total + 128;
}

void triggerDrum(uint8_t index) {
  if(index < MAX_VOICES) {
    cli(); // Disable interrupts to ensure atomic trigger
    voices[index].position = 0;
    voices[index].active = true;
    sei(); // Re-enable interrupts
  }
}

// MIDI global state machine variables
uint8_t midiState = 0; // 0=Wait for status, 1=Wait for Note, 2=Wait for Velocity
uint8_t midiNote = 0;  // Holds the received MIDI note number
const uint8_t MIDI_CHANNEL = 10; // Listen to Channel 10 (Standard MIDI Drum Channel)

// Reads serial stream and parses basic MIDI Note On messages
void processMIDI() {
  // --- Check for Native USB MIDI ---
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read(); // Read a packet
    if (rx.header != 0) { // If it's a valid MIDI packet
      uint8_t type = rx.byte1 & 0xF0;
      uint8_t channel = (rx.byte1 & 0x0F) + 1;
      
      if (type == 0x90 && channel == MIDI_CHANNEL) {
        uint8_t note = rx.byte2;
        uint8_t velocity = rx.byte3;
        
        if (velocity > 0) {
          for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
            if (note == pgm_read_byte(&kit_notes[i])) {
              triggerDrum(i);
              Serial.print("USB MIDI Triggered: ");
              Serial.println(i);
              break;
            }
          }
        }
      }
    }
  } while (rx.header != 0);

  // --- Check for Hardware DIN MIDI on RX1 ---
  // Pro Micro uses Serial1 for the hardware RX/TX pins.
  // (Serial is used for the Native USB connection)
  while (Serial1.available()) {
    uint8_t byte = Serial1.read();
    
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
        uint8_t velocity = byte;
        if (velocity > 0) {
          for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
            if (midiNote == pgm_read_byte(&kit_notes[i])) {
              triggerDrum(i);
              Serial.print("Hardware MIDI Triggered: ");
              Serial.println(i);
              break;
            }
          }
        }
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
      voices[i].position = 0;
      voices[i].active = false;
    }

    // Setup MIDI Baud Rate on the hardware RX pin
    Serial1.begin(31250);
    // Setup USB Serial for generic debugging
    Serial.begin(115200);
    
    // 8-bit PWM DAC pin. On ATmega32U4, Timer 3 Channel A is on Pin 5
    pinMode(5, OUTPUT);

    // Set up Timer 1 as a 20kHz interrupt timer for audio sampling
    // It will trigger ISR(TIMER1_COMPA_vect) every 50 microseconds
    cli(); // Disable interrupts during configuration
    TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12); // Configure for CTC mode
    TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));    
    TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10); // No prescaler (16MHz clock)
    TIMSK1 |= _BV(OCIE1A);   // Enable Timer 1 Compare Match A interrupt
    OCR1A = 800; // Counter value for 20kHz: 16MHz / 800 = 20kHz
    sei(); // Re-enable interrupts

    // Set up Timer 3 to output 8-bit Fast PWM on Pin 5 (Arduino Pro Micro)
    // This acts as a simple digital-to-analog converter (DAC)
    TCCR3A = _BV(COM3A1) | _BV(WGM30); // Fast PWM, 8-bit. Clear OC3A on compare match, set at BOTTOM
    TCCR3B = _BV(WGM32) | _BV(CS30);   // Fast PWM, 8-bit. No prescaler, run at full 16MHz clock
    OCR3A = 128; // Start at center voltage (DC offset for 8-bit unsigned audio is 128)
}

void loop() {  
  while(1) { 
    // Quickly handle any incoming MIDI messages from both USB and DIN
    processMIDI();
  }
}
