# Midipops

A polyphonic 8-bit, 20kHz MIDI drum machine for Arduino (Pro Micro / ATmega32U4).

## Features
- **Polyphonic Playback**: Mixes multiple 8-bit samples simultaneously with digital headroom management.
- **20kHz Sample Rate**: Optimized interrupt-driven audio output.
- **Dual MIDI Interface**: Supports both Native USB MIDI and Hardware DIN MIDI (Channel 10).
- **Dynamic Kit Loading**: Modular header-based drum kit system.
- **Flash Optimization**: Automated silence trimming to maximize storage on limited AVR hardware.

## Project Structure
- 📁 **`samples/`**: Raw WAV audio files.
- 📁 **`data/`**: Intermediate C headers for individual samples.
- 📁 **`tools/`**: Python scripts for sample conversion and bank compilation.
- 📁 **`include/`**: Finalized standardized drum kit banks (e.g., `kit_b.h`).
- 📄 **`midipops_promicro.ino`**: Main firmware for Arduino Pro Micro.

## Getting Started

### 1. Convert Samples
Convert WAV samples to C arrays using `wav2c.py`:
```bash
python tools/wav2c.py samples/kick.wav --length 200
```

### 2. Compile a Bank
Merge individual samples into a standardized bank using `make_bank.py`:
```bash
python tools/make_bank.py -o include/kit_b.h -f data/kick_data.h data/snare_data.h --normalize 0.4 --trim
```

### 3. Flash Firmware
Ensure the desired bank is included in `midipops_promicro.ino`, then use PlatformIO to upload:
```bash
pio run -t upload
```

## Credits & Origins
This project builds upon the original "O2" (Minipops 7 rhythm box) source code created by DIY Synthesizer pioneer Jan Ostman.
- **Original O2 Source Code**: [Web Archive Link to Jan Ostman's blog](https://web.archive.org/web/20170107051059/https://janostman.wordpress.com/the-o2-source-code/)
- **Korg Mini-Pops DIY Drum Machine Details**: [Blog Hoskins](https://bloghoskins.blogspot.com/2016/11/korg-mini-pops-diy-drum-machine.html)

## License
The original public domain dedication by Jan Ostman is preserved (CC0 1.0 Universal). 
The MIDI implementations and modifications in this repository are licensed under the **Creative Commons Attribution 4.0 International (CC BY 4.0)** license.
