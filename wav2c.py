import wave
import sys
import os
import struct

def resample(samples, old_rate, new_rate):
    if old_rate == new_rate:
        return samples
    new_len = int(len(samples) * new_rate / old_rate)
    new_samples = []
    ratio = old_rate / new_rate
    for i in range(new_len):
        pos = i * ratio
        idx = int(pos)
        frac = pos - idx
        s1 = samples[idx]
        s2 = samples[idx + 1] if idx + 1 < len(samples) else s1
        new_samples.append(s1 * (1 - frac) + s2 * frac)
    return new_samples

def wav_to_c(file_path):
    target_sr = 20000 # 20kHz, corresponding to 800 OCR1A with no prescaler at 16MHz
    try:
        with wave.open(file_path, 'rb') as wf:
            channels = wf.getnchannels()
            width = wf.getsampwidth()
            rate = wf.getframerate()
            n_frames = wf.getnframes()
            frames = wf.readframes(n_frames)
            
            samples = []
            if width == 1:
                # 8-bit WAV is unsigned (0-255)
                fmt = f"{n_frames * channels}B"
                raw = struct.unpack(fmt, frames)
                samples = [s - 128 for s in raw]
            elif width == 2:
                # 16-bit WAV is signed (-32768 to 32767)
                fmt = f"{n_frames * channels}h"
                raw = struct.unpack(fmt, frames)
                samples = [s / 256.0 for s in raw]
            else:
                print(f"Unsupported bit depth: {width*8} bits. Please use 8-bit or 16-bit WAV.")
                return
                
            # Convert to mono by averaging channels
            if channels == 2:
                mono_samples = [(samples[i] + samples[i+1]) / 2.0 for i in range(0, len(samples), 2)]
            else:
                mono_samples = samples
                
            # Resample to 20kHz
            resampled = resample(mono_samples, rate, target_sr)
            
            # Convert back to 8-bit unsigned (0-255) with 128-center
            final_samples = []
            for s in resampled:
                val = int(round(s + 128))
                val = max(0, min(255, val))
                final_samples.append(val)
                
            # Create a clean C array name
            name = os.path.splitext(os.path.basename(file_path))[0].upper().replace(" ", "_")
            name = ''.join(c for c in name if c.isalnum() or c == '_')
            
            # Generate C PROGMEM output
            out = f"const unsigned char {name}[{len(final_samples)}] PROGMEM =\n{{\n  "
            for i, val in enumerate(final_samples):
                out += f"{val},"
                if (i + 1) % 16 == 0:
                    out += "\n  "
            if not out.endswith("  "):
                out += "\n"
            out += "};\n"
            
            out_file = name.lower() + "_data.h"
            with open(out_file, "w") as f:
                f.write(out)
            print(f"Successfully created {out_file} with array {name}[{len(final_samples)}]")
            
    except Exception as e:
        print(f"Error processing {file_path}: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python wav2c.py <input.wav>")
        print("This tool takes standard WAV files, converts them to mono, resamples to 20kHz,")
        print("and outputs a C-header file with a PROGMEM array formatted for the minipops.ino sketch.")
        sys.exit(1)
    
    for i in range(1, len(sys.argv)):
        wav_to_c(sys.argv[i])
