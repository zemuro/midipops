// -----------------------------------------------------------------------------
// Midipops - Unified Arduino Core
// Polyphonic 8-bit, 20kHz audio sample playback via PWM
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

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "config.h"

#ifdef BOARD_PRO_MICRO
  #include "MIDIUSB.h"
#endif

// --- DRUM KIT LOAD (Ensure kit_b is available) ---
#include "kit_b.h"


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

// ============================================================================
// AUDIO RENDERER (TIMER1 COMPA VECTOR) - 20kHz
// ============================================================================
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
  
  // Output directly to the 8-bit hardware DAC defined in config.h
  AUDIO_PWM_OCR = total + 128;
}

// ============================================================================
// MIDI ENGINE
// ============================================================================
uint8_t midiState = 0; // 0=Wait for status, 1=Wait for Note, 2=Wait for Velocity
uint8_t midiNote = 0;  // Holds the received MIDI note number

void triggerDrum(uint8_t index, uint8_t velocity) {
  if(index < MAX_VOICES) {
    cli(); // Disable interrupts to ensure atomic trigger
    voices[index].position = 0;
    voices[index].active = true;
    voices[index].velocity = velocity;
    sei(); // Re-enable interrupts
  }
}

inline void handleNoteOn(uint8_t note, uint8_t velocity) {
  if (velocity == 0) return;
  for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
    if (note == pgm_read_byte(&kit_notes[i])) {
      triggerDrum(i, velocity);
      break;
    }
  }
}

// Parses basic MIDI Note On messages
void parseSerialByte(uint8_t byte) {
    if (byte & 0x80) { 
      // Status byte
      uint8_t type = byte & 0xF0;
      uint8_t channel = (byte & 0x0F) + 1;
      if (type == 0x90 && channel == MIDI_DRUM_CHANNEL) {
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

void processMIDI() {
#ifdef BOARD_PRO_MICRO
  // --- Check for Native USB MIDI on PRO MICRO ---
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read(); // Read a packet
    if (rx.header != 0) { // If it's a valid MIDI packet
      uint8_t type = rx.byte1 & 0xF0;
      uint8_t channel = (rx.byte1 & 0x0F) + 1;
      if (type == 0x90 && channel == MIDI_DRUM_CHANNEL) {
        handleNoteOn(rx.byte2, rx.byte3);
      }
    }
  } while (rx.header != 0);

  // Check hardware DIN MIDI on RX1
  if (Serial1.available()) LED_ON();
  else LED_OFF();

  while (Serial1.available()) {
    parseSerialByte(Serial1.read());
  }

#else
  // --- Check standard Serial MIDI on NANO ---
  if (Serial.available()) LED_ON();
  else LED_OFF();

  while (Serial.available()) {
    parseSerialByte(Serial.read());
  }
#endif
}

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================
void setupHardwareTimers() {
  cli(); // Disable interrupts during configuration

  // 1. Core Sample Interrupt (Timer 1) - Identical on both boards
  //    Triggers ISR(TIMER1_COMPA_vect) every 50 microseconds (20kHz)
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12); // CTC mode
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));    
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10); // No prescaler (16MHz clock)
  TIMSK1 |= _BV(OCIE1A);   // Enable Compare Match A interrupt
  OCR1A = 800; // 16MHz / 800 = 20kHz

#ifdef BOARD_NANO
  // 2. Nano Audio DAC (Timer 2, Pin 11)
  ASSR &= ~(_BV(EXCLK) | _BV(AS2)); // Use internal clock
  TCCR2A |= _BV(WGM21) | _BV(WGM20); // Fast PWM Mode
  TCCR2B &= ~_BV(WGM22);
  TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0); // Clear OC2A on Compare Match
  TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0)); // Disable OC2B output
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10); // No prescaler
  OCR2A = 128; // Center voltage
#else
  // 2. Pro Micro Audio DAC (Timer 3, Pin 5)
  TCCR3A = _BV(COM3A1) | _BV(WGM30); // Fast PWM, 8-bit.
  TCCR3B = _BV(WGM32) | _BV(CS30);   // Fast PWM, 8-bit. No prescaler
  OCR3A = 128; // Center voltage
#endif

  sei(); // Re-enable interrupts
}

void setup() {
  // Initialize voices from config
  for (uint8_t i = 0; i < MAX_VOICES; i++) {
    voices[i].sample = (const unsigned char*)pgm_read_word(&kit_ptrs[i]);
    voices[i].length = pgm_read_word(&kit_lens[i]);
    voices[i].velocity = 0;
    voices[i].position = 0;
    voices[i].active = false;
  }

  // Setup Hardware
  pinMode(AUDIO_PWM_PIN, OUTPUT);

#ifdef BOARD_NANO
  pinMode(STATUS_LED_PIN, OUTPUT);
  Serial.begin(MIDI_BAUD_RATE);
#else
  Serial1.begin(MIDI_BAUD_RATE);
#endif

  setupHardwareTimers();
}

void loop() {  
  while(1) { 
    processMIDI();
  }
}
