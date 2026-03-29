import wave
import aifc
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
    target_sr = 20000 
    
    ext = os.path.splitext(file_path)[1].lower()
    is_aiff = ext in ['.aif', '.aiff']
    
    try:
        # Choose the right module
        if is_aiff:
            wf = aifc.open(file_path, 'rb')
            endian = '>' # AIFF is Big-Endian
        else:
            wf = wave.open(file_path, 'rb')
            endian = '<' # WAV is Little-Endian

        with wf:
            channels = wf.getnchannels()
            width = wf.getsampwidth()
            rate = wf.getframerate()
            n_frames = wf.getnframes()
            frames = wf.readframes(n_frames)
            
            samples: List[float] = []
            if width == 1:
                # 8-bit WAV is unsigned (B), AIFF is signed (b)
                fmt = endian + f"{n_frames * channels}" + ("b" if is_aiff else "B")
                raw = struct.unpack(fmt, frames)
                samples = [float(s) if is_aiff else float(s - 128) for s in raw]
            elif width == 2:
                # 16-bit is signed
                fmt = endian + f"{n_frames * channels}h"
                raw = struct.unpack(fmt, frames)
                samples = [s / 256.0 for s in raw]
            elif width == 3:
                # 24-bit is signed
                for i in range(0, len(frames), 3):
                    # Access bytes individually to avoid slice-related type checker issues
                    b1, b2, b3 = frames[i], frames[i+1], frames[i+2]
                    
                    if is_aiff: # Big endian
                        val = (b1 << 16) | (b2 << 8) | b3
                    else: # Little endian
                        val = b1 | (b2 << 8) | (b3 << 16)
                        
                    # Handle signedness (sign extend 24-bit to 32-bit int)
                    if val & 0x800000:
                        val -= 0x1000000
                        
                    # Scale to float range
                    samples.append(val / 65536.0)
            else:
                print(f"Unsupported bit depth: {width*8} bits. Please use 8, 16, or 24-bit.")
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
                final_samples.append(max(0, min(255, val)))
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
    desc = "Convert WAV/AIFF audio files to C header array files formatted for midipops PROGMEM."
    epilog = """
Examples:
  Convert a single file:
    python wav2c.py kick.wav
    python wav2c.py clap.aif
    
  Convert multiple files, limiting their length to 500ms:
    python wav2c.py snare.wav hat.wav --length 500
    
  Convert a file and apply a 50ms fade out to prevent clicking:
    python wav2c.py loop.wav --fade 50
    
  Extract only the left channel from a stereo audio file:
    python wav2c.py stereo_drums.wav --channel left
"""
    parser = argparse.ArgumentParser(description=desc, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("files", nargs="+", help="Input WAV/AIFF files")
    parser.add_argument("--length", type=int, default=0, help="Resulting max file length in milliseconds (0 to disable)")
    parser.add_argument("--fade", type=int, default=0, help="Linear fade out length in milliseconds applied to the very end (0 to disable)")
    parser.add_argument("--channel", type=str, choices=['left', 'right', 'average'], default='average', help="Which channel to use for stereo files (default: average)")
    
    args = parser.parse_args()
    
    for f in args.files:
        wav_to_c(f, args.length, args.fade, args.channel)
