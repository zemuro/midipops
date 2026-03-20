# Include

This directory contains the finalized drum kit bank headers used by the main sketch.

## Standard Bank Format
All banks (e.g., `kit_a.h`, `kit_b.h`) follow a standardized structure:
- `NUM_SAMPLES`: Total number of instruments in the kit.
- `kit_ptrs[]`: Array of `PROGMEM` pointers to the raw sample data.
- `kit_lens[]`: Array of sample lengths.
- `kit_notes[]`: MIDI note mapping for each sample.

## Usage
To switch kits, update the `#include` line at the top of `midipops_promicro.ino`:
```cpp
#include "include/kit_b.h"
```
