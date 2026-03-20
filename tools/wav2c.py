import wave
import sys
import os
import struct
import argparse

from typing import List

def resample(samples: List[float], old_rate: int, new_rate: int) -> List[float]:
    if old_rate == new_rate:
        return samples
    new_len = int(len(samples) * new_rate / old_rate)
    new_samples: List[float] = []
    ratio = old_rate / new_rate
    for i in range(new_len):
        pos = i * ratio
        idx = int(pos)
        frac = pos - idx
        s1 = samples[idx]
        s2 = samples[idx + 1] if idx + 1 < len(samples) else s1
        new_samples.append(s1 * (1 - frac) + s2 * frac)
    return new_samples

def wav_to_c(file_path, length_ms=0, fade_ms=0, channel_mode='average'):
    target_sr = 20000 # 20kHz, corresponding to 800 OCR1A with no prescaler at 16MHz
    try:
        with wave.open(file_path, 'rb') as wf:
            channels = wf.getnchannels()
            width = wf.getsampwidth()
            rate = wf.getframerate()
            n_frames = wf.getnframes()
            frames = wf.readframes(n_frames)
            
            samples: List[float] = []
            if width == 1:
                # 8-bit WAV is unsigned (0-255)
                fmt = f"{n_frames * channels}B"
                raw = struct.unpack(fmt, frames)
                samples = [float(s - 128) for s in raw]
            elif width == 2:
                # 16-bit WAV is signed (-32768 to 32767)
                fmt = f"{n_frames * channels}h"
                raw = struct.unpack(fmt, frames)
                samples = [s / 256.0 for s in raw]
            elif width == 3:
                # 24-bit WAV is signed little-endian (-8388608 to 8388607)
                # Parse 3 bytes manually as struct doesn't have 3-byte format
                for i in range(0, len(frames), 3):
                    val = frames[i] | (frames[i+1] << 8) | (frames[i+2] << 16)
                    # Sign extension: 24th bit is sign
                    if val & 0x800000:
                        val -= 0x1000000
                    # Scale down to roughly [-128.0, 127.0]
                    samples.append(val / 65536.0)
            else:
                print(f"Unsupported bit depth: {width*8} bits. Please use 8, 16, or 24-bit WAV.")
                return
                
            # Convert to mono if stereo
            if channels == 2:
                if channel_mode == 'left':
                    mono_samples = [samples[i] for i in range(0, len(samples), 2)]
                elif channel_mode == 'right':
                    mono_samples = [samples[i+1] for i in range(0, len(samples), 2)]
                else: # average
                    mono_samples = [(samples[i] + samples[i+1]) / 2.0 for i in range(0, len(samples), 2)]
            else:
                mono_samples = samples
                
            # Resample to 20kHz
            resampled = resample(mono_samples, rate, target_sr)
            
            # Truncate to length if specified
            if length_ms > 0:
                target_len = int((length_ms / 1000.0) * target_sr)
                if target_len < len(resampled):
                    resampled = [resampled[i] for i in range(target_len)]
                else:
                    # Pad with 0 (silence) if requested length is longer than the actual file
                    resampled.extend([0.0] * (target_len - len(resampled)))
            
            # Apply fade out if specified
            if fade_ms > 0:
                fade_samples = int((fade_ms / 1000.0) * target_sr)
                if fade_samples > len(resampled):
                    fade_samples = len(resampled)
                
                if fade_samples > 0:
                    start_fade_idx = len(resampled) - fade_samples
                    for i in range(fade_samples):
                        multiplier = 1.0 - (i / float(fade_samples))
                        resampled[start_fade_idx + i] *= multiplier

            # Convert back to 8-bit unsigned (0-255) with 128-center
            final_samples = []
            for s in resampled:
                val = int(round(s + 128))
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
            
            print(f"Successfully processed: {file_path}")
            print(f" -> Output File: {out_file}")
            print(f" -> C Array:     {name}[{len(final_samples)}] PROGMEM")
            if length_ms > 0:
                print(f" -> Length:      {length_ms}ms")
            if fade_ms > 0:
                print(f" -> Fade Out:    {fade_ms}ms tail smoothed")
            print()
            
    except Exception as e:
        print(f"Error processing {file_path}: {e}")

if __name__ == "__main__":
    desc = "Convert WAV audio files to C header array files formatted for midipops PROGMEM."
    epilog = """
Examples:
  Convert a single file:
    python wav2c.py kick.wav
    
  Convert multiple files, limiting their length to 500ms:
    python wav2c.py snare.wav hat.wav --length 500
    
  Convert a file and apply a 50ms fade out to prevent clicking:
    python wav2c.py loop.wav --fade 50
    
  Extract only the left channel from a stereo audio file:
    python wav2c.py stereo_drums.wav --channel left
"""
    parser = argparse.ArgumentParser(description=desc, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("files", nargs="+", help="Input WAV files")
    parser.add_argument("--length", type=int, default=0, help="Resulting max file length in milliseconds (0 to disable)")
    parser.add_argument("--fade", type=int, default=0, help="Linear fade out length in milliseconds applied to the very end (0 to disable)")
    parser.add_argument("--channel", type=str, choices=['left', 'right', 'average'], default='average', help="Which channel to use for stereo files (default: average)")
    
    args = parser.parse_args()
    
    for f in args.files:
        wav_to_c(f, args.length, args.fade, args.channel)
