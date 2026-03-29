# Tools

This directory contains Python 3 scripts for converting and managing audio samples.

## `wav2c.py`
Converts WAV and AIFF files into C arrays formatted for `PROGMEM` storage.

**Common Usage:**
```bash
python tools/wav2c.py samples/kick.wav --length 200 --fade 20
```
- `--length`: Caps the sample length in milliseconds.
- `--fade`: Applies a linear fade-out to prevent "clicking" at the end of cropped samples.
- `--channel`: Choose `left`, `right`, or `average` (default) for stereo files.

## `make_bank.py`
Aggregates multiple sample headers into a standardized drum kit bank.

**Common Usage:**
```bash
python tools/make_bank.py -o include/kit_b.h -f data/kick_data.h data/snare_data.h --normalize 0.4 --trim
```
- `-o`: Output bank header path.
- `-f`: List of intermediate data headers to include.
- `-n`: Optional list of MIDI note numbers for triggering (defaults to 36, 38, ...).
- `--normalize`: Scales peak amplitude to a fraction of the 8-bit range (e.g., 0.4) to provide headroom for mixing.
- `--trim`: Automatically removes trailing silence (values ~128) to save flash space.
