#ifndef MIDIPOPS_CONFIG_H
#define MIDIPOPS_CONFIG_H

// ============================================================================
//   __  __ _____ _____ _____ _____   ____  _____   _____ 
//  |  \/  |_   _|  __ \_   _|  __ \ / __ \|  __ \ / ____|
//  | \  / | | | | |  | || | | |__) | |  | | |__) | (___  
//  | |\/| | | | | |  | || | |  ___/| |  | |  ___/ /___ /
//  | |  | |_| |_| |__| || |_| |    | |__| | |     ____) |
//  |_|  |_|_____|_____/_____|_|     \____/|_|    |_____/ 
//                                                          
// ============================================================================
// MIDIPOPS HARDWARE CONFIGURATION
// Use this file to select your target board and customize pin mappings.
// ============================================================================

// ----------------------------------------------------------------------------
// 1. SELECT TARGET ARCHITECTURE
// ----------------------------------------------------------------------------
// Uncomment EXACTLY ONE of the boards below. 
// If building via PlatformIO, these are usually passed via build_flags.

#if !defined(BOARD_NANO) && !defined(BOARD_PRO_MICRO)
  // Default to Nano if nothing is specified (e.g. for Arduino IDE)
  #define BOARD_NANO
#endif


// ----------------------------------------------------------------------------
// 2. HARDWARE PIN DEFINITIONS (Read Only - Automatically Assigned)
// ----------------------------------------------------------------------------

#if defined(BOARD_NANO)
  // === Nano Definitions ===
  #define AUDIO_PWM_PIN    11     // 8-bit DAC Output (Timer 2)
  #define STATUS_LED_PIN   13     // Built-in LED
  #define MIDI_BAUD_RATE   31250  // Standard DIN MIDI Speed
  
  // Timer Macros for Nano (Timer 2)
  #define AUDIO_PWM_OCR    OCR2A

  // LED Macros
  #define LED_ON()         PORTB |= _BV(PORTB5)
  #define LED_OFF()        PORTB &= ~_BV(PORTB5)

#elif defined(BOARD_PRO_MICRO)
  // === Pro Micro Definitions ===
  #define AUDIO_PWM_PIN    5      // 8-bit DAC Output (Timer 3)
  #define MIDI_BAUD_RATE   31250  // Standard DIN MIDI Speed (Serial1)
  
  // Timer Macros for Pro Micro (Timer 3)
  #define AUDIO_PWM_OCR    OCR3A

  // LED Macros (TX/RX LEDs are active low on Pro Micro)
  #define LED_ON()         PORTD &= ~(1<<5) // TXLED on
  #define LED_OFF()        PORTD |= (1<<5)  // TXLED off

#else
  #error "No board selected! Please define BOARD_NANO or BOARD_PRO_MICRO"
#endif

// ----------------------------------------------------------------------------
// 3. SYNTHESIS & MIDI ENGINE CONFIGURATION
// ----------------------------------------------------------------------------
#define AUDIO_SAMPLE_RATE  20000  // 20kHz Playback Rate (Do not change unless you recalculate Timer1)
#define MIDI_DRUM_CHANNEL  10     // Standard MIDI drum channel (1-16)

#endif // MIDIPOPS_CONFIG_H
