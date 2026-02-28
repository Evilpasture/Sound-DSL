#pragma once

#include <stddef.h>
#include <stdint.h>

// --- TYPES & ENUMS ---
typedef enum { WAVE_SINE, WAVE_SQUARE, WAVE_SAW } WaveType;

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
[[nodiscard]] bool synth_set_wave(WaveType wave);
[[nodiscard]] bool synth_set_adsr(ADSR adsr);
[[nodiscard]] bool synth_set_vibrato(float depth, float rate);
[[nodiscard]] bool synth_set_delay(float time, float feedback, float mix);

// --- THE MUSICAL CONSTANTS ---
[[maybe_unused]] constexpr float C4 = 261.63f, D4 = 293.66f, E4 = 329.63f, 
                                 F4 = 349.23f, G4 = 392.00f, A4 = 440.00f, 
                                 B4 = 493.88f, C5 = 523.25f, E5 = 659.25f;

[[maybe_unused]] constexpr float W = 1.0f, H = 0.5f, Q = 0.25f, E = 0.125f;

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

#define SET_WAVE(w) synth_set_wave(w)
#define SET_ADSR(a) synth_set_adsr(a)
#define SET_VIBRATO(d, r) synth_set_vibrato((d), (r))
#define SET_DELAY(time, fb, m) synth_set_delay((time), (fb), (m))

#define NOTE(f, d) synth_render_chord((const float[]){f}, 1, (d))
#define CHORD(d, ...) synth_render_chord((const float[]){__VA_ARGS__}, sizeof((float[]){__VA_ARGS__}) / sizeof(float), (d))
#define REST(d) synth_render_chord((const float[]){0.0f}, 1, (d))