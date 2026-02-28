# Synth-DSL (Sound-DSL)

A high-performance, multi-threaded polyphonic synthesizer and musical Domain-Specific Language (DSL) written in **pure C23**. 

Synth-DSL allows you to bridge the gap between MIDI composition and low-level DSP. Whether you are compiling MIDI files into optimized binary assets or composing directly in C, the engine renders high-fidelity `16-bit 44.1kHz WAV` files using modern parallel processing.

## Key Features

*   **C23 Powered**: Built using modern C23 features (`static constexpr`, `[[nodiscard]]`, `[[maybe_unused]]`).
*   **Multi-Threaded Rendering**: Massive polyphony is handled by a worker-thread architecture. It automatically scales to your CPU core count (e.g., rendering thousands of notes across 12+ threads).
*   **MIDI Orchestration**: A Python-based compiler transforms standard MIDI files into optimized C loaders and binary data blobs.
*   **Zero Dependencies**: The core engine requires only a standard C compiler (Clang 18+ or GCC 13+) and a math library. No external audio APIs needed.
*   **Stacked DSP Engine**: 
    *   3 Stackable Oscillators per voice (`SINE`, `SQUARE`, `SAW`, `NOISE`).
    *   **ADSR Envelopes**: Precise control over Attack, Decay, Sustain, and Release.
    *   **Vibrato & LFO**: Pitch modulation for rich, organic textures.
    *   **Bitcrusher**: Real-time quantization for Lo-Fi and Chiptune aesthetics.

## Prerequisites

*   **Compiler**: Clang 18+ or GCC 13+ (Required for C23 support).
*   **Build System**: CMake 3.25+ and [Ninja](https://ninja-build.org/).
*   **Python**: 3.10+ (Tested up to 3.14-alpha) with `mido`.
*   **Package Manager**: [uv](https://github.com/astral-sh/uv) (Recommended for seamless Python orchestration).

## Installation & Build

The project uses CMake to orchestrate the Python MIDI compilation and the C build process in one step.

```powershell
# 1. Configure with Ninja and Clang
cmake -G Ninja -DCMAKE_C_COMPILER=clang -B build

# 2. Compile the MIDI and Build the Engine
cmake --build build

# 3. Render the Audio (Orchestrated Execution)
cmake --build build --target render
```

*This generates `theme_song.wav` in the `build/` directory.*

## Orchestration Workflow

### 1. MIDI to C (The Compiler)
The `midi_compiler.py` script parses MIDI ticks, calculates timings, and generates a C entry point. It automatically maps MIDI channels to instrument patches:
*   **Channel 10**: Automatically assigned to `PATCH_SNARE` (Noise).
*   **Melodic Channels**: Assigned to `PATCH_BRASS` (Multi-SAW).

### 2. Direct C Composition
You can also compose directly in C using the constants defined in `synth_scale.h`.

```c
#include "synth_dsl.h"
#include "synth_scale.h"

int main() {
    // Set a track patch
    synth_seq_set_patch(0, PATCH_BRASS);

    // Add notes: track, frequency (Hz), start (s), duration (s)
    synth_seq_add_note(0, A4, 0.0f, Q); 
    synth_seq_add_note(0, E5, 0.5f, Q);
    
    // Render across all available CPU cores
    synth_seq_render("composition.wav");
    return 0;
}
```

## Scientific Analysis

Includes a Python-based DSP analyzer to visualize the results of your synthesis.

```bash
# Check for clipping, harmonics, and view the spectrogram
python analyze_wav.py build/theme_song.wav
```

## Compatibility Note
This project utilizes **C23-isms** that are currently incompatible with MSVC (`cl.exe`). Please use **Clang** or **GCC**. On Windows, it is highly recommended to run via the **Ninja** generator to avoid default MSVC project behavior.



### Project Structure
*   `synth_dsl.c/h`: The multi-threaded synthesis engine.
*   `synth_scale.h`: Frequency constants (C0-E9) and rhythmic durations.
*   `midi_compiler.py`: Transmutes MIDI files into C/Binary assets.
*   `CMakeLists.txt`: The master orchestrator.