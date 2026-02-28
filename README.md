# Synth-DSL (Sound-DSL)

A lightweight, zero-dependency polyphonic synthesizer and musical Domain-Specific Language (DSL) written in pure C23. 

Synth-DSL allows you to compose music directly in C code using an intuitive macro-based syntax. The engine processes your composition and renders it directly to a high-quality `16-bit 44.1kHz WAV` file—no external audio libraries required!

## Features

* **Zero Dependencies**: The core synthesis engine requires only the standard C library (`<stdio.h>`, `<math.h>`). It writes binary RIFF/WAVE headers from scratch.
* **The Macro DSL**: Compose using beautiful, readable macros like `PLAY()`, `NOTE()`, `CHORD()`, and `REST()`.
* **Polyphonic Synthesis**: 
  * Play massive chords dynamically.
  * 3 Stackable Oscillators per voice.
  * Waveforms: `SINE`, `SQUARE`, `SAW`, `NOISE`.
* **Audio Effects (FX)**:
  * **ADSR Envelopes**: Control Attack, Decay, Sustain, and Release times.
  * **Delay Line**: Built-in feedback delay/echo.
  * **LFO Vibrato**: Modulate pitch for rich, thick sounds.
  * **Bitcrusher**: Dynamically drop the bit-depth (e.g., to 6-bit) mid-song for chiptune/synthwave breakdowns.
* **Scientific Python Analyzer**: Includes a Python tool to visualize the waveform, spectrogram, and harmonic FFT of your generated tracks.


## Installation

### 1. Build and Run the Synthesizer
Compile the C code using your compiler (GCC, Clang, or MSVC).

**Linux / macOS:**
```bash
gcc -O3 fur_elise.c synth_dsl.c -lm -o synth
./synth
```

**Windows (MSVC):**
```cmd
cl /O2 fur_elise.c synth_dsl.c
synth.exe
```

*This will output a file named `fur_elise.wav` in your directory.*

### 2. Analyze the Audio (Optional)
We provide a Python script to visualize the DSP data, check for clipping, and view the spectrogram.

**Requirements:** `numpy`, `matplotlib`, `scipy`

```bash
# Using uv, pip, or your preferred python environment
python analyze_wav.py fur_elise.wav
```

## Composition

The DSL maps standard musical notation (A0 to E9) to exact frequencies. You can change synthesizer parameters instantly between notes.

```c
#include "synth_dsl.h"

int main() {
    synth_start_track("my_song.wav");

    // Define an instrument
    const ADSR PIANO = {0.02f, 0.25f, 0.5f, 0.8f};
    
    // Configure the synth
    PLAY(
        SET_OSC(0, WAVE_SQUARE, 0.5f, 0.0f),
        SET_OSC(1, WAVE_SINE, 0.8f, 0.05f), // Detuned for chorus
        SET_ADSR(PIANO),
        SET_DELAY(0.36f, 0.4f, 0.25f)       // 360ms echo
    );

    // Play a chord and a melody
    PLAY(
        CHORD(0.5f, C3, E3, G3), // C Major chord for 0.5 seconds
        NOTE(C4, 0.25f),         // Quarter note
        NOTE(E4, 0.25f),
        REST(0.5f)               // Silence (delay tails will still ring!)
    );

    synth_end_track();
    return 0;
}
```
