# Midipops

A polyphonic 8-bit, 20kHz MIDI drum machine for **Arduino Nano** (ATmega328P) and **Arduino Pro Micro** (ATmega32U4).

This version is a unified, highly optimized port designed for **PlatformIO**. It features a modern hardware abstraction layer and aggressive space optimizations to maximize drum kit capacity.

## Features
- **Unified Codebase**: Single `main.cpp` supports both Nano and Pro Micro via the `config.h` abstraction.
- **Polyphonic 20kHz Playback**: Mixes multiple 8-bit samples simultaneously with digital headroom management.
- **Lean GCC Build**: Leverages `-flto` and `--relax` to minimize binary footprint (only ~14KB for the core engine).
- **Dual MIDI Support**: 
    - **Nano**: Standard DIN MIDI on hardware Serial (RX pin).
    - **Pro Micro**: Native USB MIDI + Hardware DIN MIDI (RX1 pin).

## Project Structure
- 📁 **`src/`**: Unified source code (`main.cpp`).
- 📁 **`include/`**: Hardware configuration (`config.h`) and drum kit headers.
- 📁 **`tools/`**: Python scripts for WAV-to-C conversion and bank compilation.
- 📁 **`samples/`**: Raw WAV audio source files.
- 📄 **`platformio.ini`**: Multi-environment build configuration.

## Getting Started

### 1. Configure Hardware
Open [include/config.h](include/config.h) and ensure the correct board is selected (Nano is default). You can also customize audio and LED pins here.

### 2. Prepare Drum Kits
Use the Python tools to convert your own WAVs or use the provided kits:
- **Convert**: `python tools/wav2c.py samples/kick.wav`
- **Compile Bank**: `python tools/make_bank.py -o include/my_kit.h data/kick.h data/snare.h`

### 3. Build & Flash
Use the PlatformIO CLI or IDE to flash your board:

**For Arduino Nano:**
```bash
pio run -e nanoatmega328 -t upload
```

**For Arduino Pro Micro:**
```bash
pio run -e sparkfun_promicro16 -t upload
```

## Credits & Origins
Built upon the original "O2" (Minipops 7) source code by Jan Ostman. 
- Original DSP Logic: Jan Ostman (CC0).
- MIDI Implementation & Porting: Antigravity AI.

## License
Licensed under the **Creative Commons Attribution 4.0 International (CC BY 4.0)**.
