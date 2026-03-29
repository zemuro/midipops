import os
import sys
import argparse
import re
from typing import List

def create_bank(output_file: str, input_files: List[str], notes: List[int], normalize: float, trim: bool) -> None:
    bank_contents: List[str] = []
    array_names: List[str] = []
    total_bytes: int = 0
    
    for f in input_files:
        try:
            with open(f, 'r') as infile:
                content = infile.read()
                
                # Try to extract the full C array to allow dynamic re-normalization
                match = re.search(r'const unsigned char\s+([A-Za-z0-9_]+)\[\d*\]\s*PROGMEM\s*=\s*\{([^}]+)\};', content)
                if match:
                    name = str(match.group(1))
                    array_names.append(name)
                    numbers_str = str(match.group(2))
                    samples = [int(x.strip()) for x in numbers_str.split(',') if x.strip()]
                    
                    if normalize > 0.0:
                        # Convert to signed to process amplitude
                        signed_samples = [s - 128 for s in samples]
                        max_amp = max((abs(s) for s in signed_samples), default=0)
                        if max_amp > 0:
                            scale = (127.0 * normalize) / max_amp
                            samples = [max(0, min(255, int(round(s * scale + 128)))) for s in signed_samples]
                            
                    if trim:
                        trimmed_count = 0
                        # 128 is pure center silence unsigned. We allow ±1 tolerance for minor floor noise.
                        while len(samples) > 0 and (127 <= samples[-1] <= 129):
                            samples.pop()
                            trimmed_count += 1
                        if trimmed_count > 0:
                            print(f"Trimmed {trimmed_count} trailing silence bytes from {name}")
                            
                    # Re-format back to C string
                    out = f"const unsigned char {name}[{len(samples)}] PROGMEM =\n{{\n  "
                    for i, val in enumerate(samples):
                        out += f"{val},"
                        if (i + 1) % 16 == 0:
                            out += "\n  "
                    if not out.endswith("  "):
                        out += "\n"
                    out += "};\n"
                    bank_contents.append(out + "\n")
                    total_bytes += len(samples)
                else:
                    print(f"Warning: Could not parse valid PROGMEM array in {f}")
                    # Fallback to direct inclusion
                    bank_contents.append(content + "\n\n")
                    match_name = re.search(r'const unsigned char\s+([A-Za-z0-9_]+)\[', content)
                    if match_name:
                        array_names.append(match_name.group(1))
                    else:
                        base_name = str(os.path.splitext(os.path.basename(f))[0])
                        name = base_name.upper().replace(" ", "_")
                        name = ''.join(str(c) for c in name if c.isalnum() or c == '_')
                        array_names.append(name)
                        
                        # Fallback parsing to count bytes if possible
                        match_size = re.search(r'\[(\d+)\]', content)
                        if match_size:
                            total_bytes += int(match_size.group(1))
                        else:
                            # Heuristic: count commas in the presumably large curly braces section
                            match_braces = re.search(r'\{([^}]+)\}', content)
                            if match_braces:
                                total_bytes += match_braces.group(1).count(',') + 1
        except Exception as e:
            print(f"Error reading {f}: {e}")
            return
                
    num_samples = len(array_names)
    if num_samples == 0:
        print("No samples provided.")
        return
    
    # Generate metadata structure
    metadata = f"\n#define NUM_SAMPLES {num_samples}\n"
    
    metadata += f"const unsigned char* const kit_ptrs[NUM_SAMPLES] PROGMEM = {{ {', '.join(array_names)} }};\n"
    metadata += f"const uint16_t kit_lens[NUM_SAMPLES] PROGMEM = {{ {', '.join(f'sizeof({name})' for name in array_names)} }};\n"
    
    # Pad MIDI notes if the user did not provide enough
    padded_notes = list(notes)
    while len(padded_notes) < num_samples:
        if len(padded_notes) == 0:
            padded_notes.append(36)
        else:
            # Standard General MIDI drum spacing: 36 (Kick), 38 (Snare), 42 (CHat), 46 (OHat)
            # We'll just increment sequentially if none are provided
            padded_notes.append(padded_notes[-1] + 1)
            
    padded_notes = [padded_notes[i] for i in range(num_samples)]
    metadata += f"const uint8_t kit_notes[NUM_SAMPLES] PROGMEM = {{ {', '.join(str(n) for n in padded_notes)} }};\n"
    
    # Write aggregated output
    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    with open(output_file, 'w') as f:
        # Add basic include guard
        guard = os.path.basename(output_file).replace('.', '_').upper()
        f.write(f"#ifndef {guard}\n#define {guard}\n\n#include <avr/pgmspace.h>\n\n")
        f.write("".join(bank_contents))
        f.write(metadata)
        f.write(f"\n#endif // {guard}\n")
        
    print(f"Successfully created '{output_file}' with {num_samples} samples.")
    print(f"Arrays included:   {', '.join(array_names)}")
    print(f"MIDI Notes mapped: {', '.join(str(n) for n in padded_notes)}")
    
    total_ms = float(total_bytes) / 20.0
    print(f"Total Size:        {total_bytes} bytes ({total_ms:.1f} ms @ 20kHz)")

if __name__ == "__main__":
    desc = "Create a compiled Midipops drum bank header from individual sample header arrays."
    epilog = """
Examples:
  Basic combination (MIDI notes default sequentially starting at 36 / C1):
    python make_bank.py --output include/kit_c.h --files kick_data.h snare_data.h
    
  Specify exact MIDI note triggers for each sample:
    # kick=36, snare=38, hat=42
    python make_bank.py -o kit_d.h -f kick_data.h snare_data.h hat_data.h -n 36 38 42
    
  Normalize the bank sum per channel to prevent hard clipping:
    python make_bank.py -o kit_e.h -f kick_data.h snare_data.h --normalize 0.33

  Trim trailing silence automatically to vastly reduce flash memory overhead:
    python make_bank.py -o kit_f.h -f samples.h --trim
    
This script merges all input files and automatically generates the 
kit_ptrs, kit_lens, and kit_notes pointer mappings required by the sketch.
"""
    parser = argparse.ArgumentParser(description=desc, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("-o", "--output", required=True, help="Output header file path (e.g., include/kit_c.h)")
    parser.add_argument("-f", "--files", nargs='+', required=True, help="Input sample header files generated by wav2c.py")
    parser.add_argument("-n", "--notes", type=int, nargs='+', default=[], help="Optional list of MIDI notes corresponding to the input files")
    parser.add_argument("--normalize", type=float, default=0.0, help="Normalize peak amplitude (1.0 = max volume, 0.5 = half scale mixing)")
    parser.add_argument("--trim", action="store_true", help="Automatically cull trailing trailing silence (values ~128) from array tails")
    
    args = parser.parse_args()
    create_bank(args.output, args.files, args.notes, args.normalize, args.trim)
