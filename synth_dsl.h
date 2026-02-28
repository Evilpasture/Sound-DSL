#pragma once

#include <stddef.h>
#include <stdint.h>

// ==========================================
// TYPES & CORE STRUCTURES
// ==========================================

typedef enum { WAVE_SINE, WAVE_SQUARE, WAVE_SAW, WAVE_NOISE } WaveType;

typedef struct {
    float attack_time;
    float decay_time;
    float sustain_lvl;
    float release_time;
} ADSR;

typedef struct {
    WaveType type;
    float volume;
    float detune;
} OscConfig;

// A complete instrument definition (Patch)
typedef struct {
    OscConfig oscs[3];
    ADSR adsr;
    float vibrato_depth;
    float vibrato_rate;
    float delay_time;
    float delay_feedback;
    float delay_mix;
    int bit_depth;
} SynthPatch;

// ==========================================
// PRESETS
// ==========================================

[[maybe_unused]] static constexpr ADSR ADSR_ORGAN = {0.01f, 0.00f, 1.0f, 0.05f};
[[maybe_unused]] static constexpr ADSR ADSR_PIANO = {0.01f, 0.30f, 0.2f, 0.40f};
[[maybe_unused]] static constexpr ADSR ADSR_PLUCK = {0.01f, 0.15f, 0.0f, 0.10f};
[[maybe_unused]] static constexpr ADSR ADSR_PAD   = {0.40f, 0.20f, 0.8f, 0.60f};

[[maybe_unused]] static constexpr SynthPatch PATCH_BRASS = {
    .oscs = {{WAVE_SAW, 0.5f, 0.0f}, {WAVE_SQUARE, 0.3f, -12.0f}, {WAVE_SAW, 0.2f, 0.05f}},
    .adsr = {0.02f, 0.1f, 0.6f, 0.15f},
    .vibrato_depth = 0.04f, .vibrato_rate = 5.0f,
    .delay_time = 0.25f, .delay_feedback = 0.2f, .delay_mix = 0.15f,
    .bit_depth = 16
};

[[maybe_unused]] static constexpr SynthPatch PATCH_SNARE = {
    .oscs = {{WAVE_NOISE, 0.5f, 0.0f}, {WAVE_SINE, 0.0f, 0.0f}, {WAVE_SINE, 0.0f, 0.0f}},
    .adsr = {0.001f, 0.05f, 0.0f, 0.05f},
    .bit_depth = 16
};

// ==========================================
// MULTI-TRACK SEQUENCER API
// ==========================================
// The modern way to compose: load notes into tracks, then render once.

void synth_seq_set_patch(int track_id, SynthPatch patch);
void synth_seq_add_note(int track_id, float freq, float start_time, float duration);
[[nodiscard]] bool synth_seq_render(const char *filename);