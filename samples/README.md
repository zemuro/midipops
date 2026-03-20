# Samples

This directory contains the raw WAV audio files used for the drum machine.

## Format Guidelines
- **Format**: PCM WAV
- **Bit Depth**: 8-bit, 16-bit, or 24-bit are supported by the conversion scripts.
- **Channels**: Mono or Stereo (Conversion script can average channels or pick a specific one).
- **Recommended Processing**: Keep samples short to fit within the limited AVR flash memory (~28KB total).

## Conversion
To convert these files for use in the firmware, use the `tools/wav2c.py` script.
