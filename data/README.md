# Data

This directory contains intermediate C header files generated from raw WAV samples.

## Workflow
1. Run `tools/wav2c.py` on a WAV file in `samples/`.
2. The script outputs a `*_data.h` file here.
3. Use `tools/make_bank.py` to aggregate these intermediate files into a final bank in the `include/` directory.

> [!NOTE]
> These files are typically not included directly by the main sketch. They serve as building blocks for the standardized bank headers.
