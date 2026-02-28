import mido
import struct
import sys
import os
import argparse

def midi_to_hz(note):
    return 440.0 * (2.0 ** ((note - 69) / 12.0))

def compile_midi(input_file, output_prefix):
    try:
        mid = mido.MidiFile(input_file, clip=True)
    except Exception as e:
        print(f"Error: {e}")
        return

    # 1. Calculate Timing
    tempo = 500000 
    for track in mid.tracks:
        for msg in track:
            if msg.type == 'set_tempo':
                tempo = msg.tempo
                break
    seconds_per_tick = (tempo / 1000000.0) / mid.ticks_per_beat

    # 2. Extract Data
    bin_data = bytearray()
    track_info = {} # Track ID -> {name, is_drum}
    note_count = 0

    for i, track in enumerate(mid.tracks):
        track_name = f"Track {i}"
        is_drum = False
        current_ticks = 0
        active_notes = {}
        track_note_count = 0

        for msg in track:
            current_ticks += msg.time
            curr_s = current_ticks * seconds_per_tick
            
            if msg.type == 'track_name':
                track_name = msg.name.strip()
            
            # Channel 9 (0-indexed) is percussion in MIDI standard
            if hasattr(msg, 'channel') and msg.channel == 9:
                is_drum = True
            
            if msg.type == 'note_on' and msg.velocity > 0:
                active_notes[msg.note] = curr_s
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                if msg.note in active_notes:
                    start = active_notes[msg.note]
                    dur = curr_s - start
                    if dur > 0.005:
                        # Pack as: int track, float freq, float start, float dur (16 bytes)
                        hz = midi_to_hz(msg.note)
                        bin_data.extend(struct.pack("ifff", i, hz, start, dur))
                        track_note_count += 1
                        note_count += 1
                    del active_notes[msg.note]
        
        if track_note_count > 0:
            track_info[i] = {"name": track_name, "drum": is_drum}

    # 3. Write Binary Blob
    bin_filename = f"{output_prefix}_data.bin"
    with open(bin_filename, "wb") as f:
        f.write(bin_data)

    # 4. Write C Loader File
    c_filename = f"{output_prefix}.c"
    with open(c_filename, "w", encoding="utf-8") as f:
        f.write(f'// AUTO-GENERATED MUSIC LOADER\n')
        f.write(f'#include "synth_dsl.h"\n#include <stdio.h>\n')
        f.write(f'int main() {{\n')
        f.write(f'    printf("Loading {note_count} notes from {bin_filename}...\\n");\n\n')
        
        # Write Patch Definitions
        for tid, info in track_info.items():
            patch = "PATCH_SNARE" if info['drum'] or "drum" in info['name'].lower() else "PATCH_BRASS"
            f.write(f'    synth_seq_set_patch({tid}, {patch}); // {info["name"]}\n')

        f.write(f'\n    FILE* f = fopen("{bin_filename}", "rb");\n')
        f.write(f'    if(!f) return 1;\n')
        f.write(f'    for(int i=0; i<{note_count}; i++) {{\n')
        f.write(f'        int t; float fr, s, d;\n')
        f.write(f'        if(fread(&t, 4, 1, f) && fread(&fr, 4, 1, f) && ')
        f.write(f'fread(&s, 4, 1, f) && fread(&d, 4, 1, f))\n')
        f.write(f'            synth_seq_add_note(t, fr, s, d);\n')
        f.write(f'    }}\n    fclose(f);\n\n')
        f.write(f'    printf("Rendering...\\n");\n')
        f.write(f'    synth_seq_render("{output_prefix}.wav");\n')
        f.write(f'    return 0;\n}}\n')

    print(f"✅ Success!")
    print(f"   Binary Asset: {bin_filename} ({len(bin_data)} bytes)")
    print(f"   C Source:     {c_filename}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Input MIDI file")
    parser.add_argument("output", help="Output prefix (e.g. 'march')")
    args = parser.parse_args()
    compile_midi(args.input, args.output)