#pragma once

#include <stddef.h>
#include <stdint.h>

// --- TYPES & ENUMS ---
typedef enum { WAVE_SINE, WAVE_SQUARE, WAVE_SAW, WAVE_NOISE } WaveType;

typedef struct {
  float attack_time;
  float decay_time;
  float sustain_lvl;
  float release_time;
} ADSR;

// --- THE C API ---
[[nodiscard]] bool synth_start_track(const char *filename);
[[nodiscard]] bool synth_end_track(void);
[[nodiscard]] bool synth_render_chord(const float *freqs, size_t count, float dur);

// Configuration API (These return true to allow chaining in the DSL)
[[nodiscard]] bool synth_set_osc(int index, WaveType type, float volume, float detune);
[[nodiscard]] bool synth_set_wave(WaveType wave);
[[nodiscard]] bool synth_set_adsr(ADSR adsr);
[[nodiscard]] bool synth_set_vibrato(float depth, float rate);
[[nodiscard]] bool synth_set_delay(float time, float feedback, float mix);
[[nodiscard]] bool synth_set_bit_depth(int bits);

// --- THE COMPLETE MUSICAL SCALE (A4 = 440Hz) ---

// Octave 0 (Sub-Bass / Rumble)
[[maybe_unused]] constexpr float 
    C0 = 16.35f,  Cs0 = 17.32f,  Db0 = 17.32f,  D0 = 18.35f,  Ds0 = 19.45f,  Eb0 = 19.45f, 
    E0 = 20.60f,  F0 = 21.83f,   Fs0 = 23.12f,  Gb0 = 23.12f, G0 = 24.50f,   Gs0 = 25.96f, 
    Ab0 = 25.96f, A0 = 27.50f,   As0 = 29.14f,  Bb0 = 29.14f, B0 = 30.87f;

// Octave 1 (Heavy Bass)
[[maybe_unused]] constexpr float 
    C1 = 32.70f,  Cs1 = 34.65f,  Db1 = 34.65f,  D1 = 36.71f,  Ds1 = 38.89f,  Eb1 = 38.89f, 
    E1 = 41.20f,  F1 = 43.65f,   Fs1 = 46.25f,  Gb1 = 46.25f, G1 = 49.00f,   Gs1 = 51.91f, 
    Ab1 = 51.91f, A1 = 55.00f,   As1 = 58.27f,  Bb1 = 58.27f, B1 = 61.74f;

// Octave 2 (Bass / Cello Low)
[[maybe_unused]] constexpr float 
    C2 = 65.41f,  Cs2 = 69.30f,  Db2 = 69.30f,  D2 = 73.42f,  Ds2 = 77.78f,  Eb2 = 77.78f, 
    E2 = 82.41f,  F2 = 87.31f,   Fs2 = 92.50f,  Gb2 = 92.50f, G2 = 98.00f,   Gs2 = 103.83f, 
    Ab2 = 103.83f, A2 = 110.00f,  As2 = 116.54f, Bb2 = 116.54f, B2 = 123.47f;

// Octave 3 (Tenor / Guitar Low)
[[maybe_unused]] constexpr float 
    C3 = 130.81f, Cs3 = 138.59f, Db3 = 138.59f, D3 = 146.83f, Ds3 = 155.56f, Eb3 = 155.56f, 
    E3 = 164.81f, F3 = 174.61f,  Fs3 = 185.00f, Gb3 = 185.00f, G3 = 196.00f,  Gs3 = 207.65f, 
    Ab3 = 207.65f, A3 = 220.00f,  As3 = 233.08f, Bb3 = 233.08f, B3 = 246.94f;

// Octave 4 (Middle C Range)
[[maybe_unused]] constexpr float 
    C4 = 261.63f, Cs4 = 277.18f, Db4 = 277.18f, D4 = 293.66f, Ds4 = 311.13f, Eb4 = 311.13f, 
    E4 = 329.63f, F4 = 349.23f,  Fs4 = 369.99f, Gb4 = 369.99f, G4 = 392.00f,  Gs4 = 415.30f, 
    Ab4 = 415.30f, A4 = 440.00f,  As4 = 466.16f, Bb4 = 466.16f, B4 = 493.88f;

// Octave 5 (Treble / Soprano)
[[maybe_unused]] constexpr float 
    C5 = 523.25f, Cs5 = 554.37f, Db5 = 554.37f, D5 = 587.33f, Ds5 = 622.25f, Eb5 = 622.25f, 
    E5 = 659.25f, F5 = 698.46f,  Fs5 = 739.99f, Gb5 = 739.99f, G5 = 783.99f,  Gs5 = 830.61f, 
    Ab5 = 830.61f, A5 = 880.00f,  As5 = 932.33f, Bb5 = 932.33f, B5 = 987.77f;

// Octave 6 (High Whistle)
[[maybe_unused]] constexpr float 
    C6 = 1046.50f, Cs6 = 1108.73f, Db6 = 1108.73f, D6 = 1174.66f, Ds6 = 1244.51f, Eb6 = 1244.51f, 
    E6 = 1318.51f, F6 = 1396.91f,  Fs6 = 1479.98f, Gb6 = 1479.98f, G6 = 1567.98f,  Gs6 = 1661.22f, 
    Ab6 = 1661.22f, A6 = 1760.00f,  As6 = 1864.66f, Bb6 = 1864.66f, B6 = 1975.53f;

// Octave 7 (Extreme High)
[[maybe_unused]] constexpr float 
    C7 = 2093.00f, Cs7 = 2217.46f, Db7 = 2217.46f, D7 = 2349.32f, Ds7 = 2489.02f, Eb7 = 2489.02f, 
    E7 = 2637.02f, F7 = 2793.83f,  Fs7 = 2959.96f, Gb7 = 2959.96f, G7 = 3135.96f,  Gs7 = 3322.44f, 
    Ab7 = 3322.44f, A7 = 3520.00f,  As7 = 3729.31f, Bb7 = 3729.31f, B7 = 3951.07f;

// Octave 8 (Piercing / Hearing Test)
[[maybe_unused]] constexpr float 
    C8 = 4186.01f, Cs8 = 4434.92f, Db8 = 4434.92f, D8 = 4698.63f, Ds8 = 4978.03f, Eb8 = 4978.03f;

// Octave 8 (The "Biological Limit" / 4k-8k Range)
[[maybe_unused]] constexpr float 
    E8 = 5274.04f, F8 = 5587.65f, Fs8 = 5919.91f, Gb8 = 5919.91f, 
    G8 = 6271.93f, Gs8 = 6644.88f, Ab8 = 6644.88f, A8 = 7040.00f, 
    As8 = 7458.62f, Bb8 = 7458.62f, B8 = 7902.13f;

// Octave 9 (The "High Frequency Buzz")
[[maybe_unused]] constexpr float 
    C9 = 8372.01f, D9 = 9397.27f, E9 = 10548.08f;

// --- RHYTHM DURATIONS (120 BPM) ---
// 60 seconds / 120 beats = 0.5s per beat
[[maybe_unused]] constexpr float W   = 2.0f;     // Whole Note (4 beats)
[[maybe_unused]] constexpr float H   = 1.0f;     // Half Note (2 beats)
[[maybe_unused]] constexpr float Hd  = 1.5f;     // Dotted Half (3 beats)
[[maybe_unused]] constexpr float Q   = 0.5f;     // Quarter Note (1 beat)
[[maybe_unused]] constexpr float Qd  = 0.75f;    // Dotted Quarter (1.5 beats)
[[maybe_unused]] constexpr float E   = 0.25f;    // Eighth Note (0.5 beats)
[[maybe_unused]] constexpr float Ed  = 0.375f;   // Dotted Eighth (0.75 beats)
[[maybe_unused]] constexpr float S   = 0.125f;   // Sixteenth Note (0.25 beats)
[[maybe_unused]] constexpr float T32 = 0.0625f;  // Thirty-Second Note

// --- PRESET INSTRUMENTS ---
[[maybe_unused]] constexpr ADSR ADSR_ORGAN = {0.01f, 0.00f, 1.0f, 0.05f};
[[maybe_unused]] constexpr ADSR ADSR_PIANO = {0.01f, 0.30f, 0.2f, 0.40f};
[[maybe_unused]] constexpr ADSR ADSR_PLUCK = {0.01f, 0.15f, 0.0f, 0.10f};
[[maybe_unused]] constexpr ADSR ADSR_PAD   = {0.40f, 0.20f, 0.8f, 0.60f};

// --- THE DSL MACROS ---
#define PLAY(...) ({ \
    bool synth_final_ok = true; \
    const bool synth_results[] = { __VA_ARGS__ }; \
    for (size_t synth_i = 0; synth_i < sizeof(synth_results)/sizeof(bool); ++synth_i) { \
        if (!synth_results[synth_i]) { synth_final_ok = false; break; } \
    } \
    synth_final_ok; \
})

// Configure a specific oscillator (0, 1, or 2)
// Detune is in Semitones (e.g., 0.1 for fatness, 7.0 for a perfect 5th)
#define SET_OSC(idx, wave, vol, tune) synth_set_osc((idx), (wave), (vol), (tune))

#define SET_WAVE(w) synth_set_wave(w)
#define SET_ADSR(...) synth_set_adsr((ADSR)__VA_ARGS__)
#define SET_VIBRATO(d, r) synth_set_vibrato((d), (r))
#define SET_DELAY(time, fb, m) synth_set_delay((time), (fb), (m))
#define SET_CRUSH(b) synth_set_bit_depth(b)

#define NOTE(f, d) synth_render_chord((const float[]){f}, 1, (d))
#define CHORD(d, ...) synth_render_chord((const float[]){__VA_ARGS__}, sizeof((float[]){__VA_ARGS__}) / sizeof(float), (d))
#define REST(d) synth_render_chord((const float[]){0.0f}, 1, (d))