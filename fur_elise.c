#include "synth_dsl.h"
#include <stdio.h>

// --- TEMPO CONFIGURATION (166 BPM, 3/8 Time) ---
#undef S
#undef E
#undef Q
#undef Qd
#undef H
#undef W

#define S   0.18f   // Sixteenth Note (1/6th of a measure)
#define E   0.36f   // Eighth Note (1/3rd of a measure)
#define Q   0.72f   // Quarter Note (2/3rds of a measure)
#define Qd  1.08f   // Dotted Quarter (1 full measure)
#define H   1.44f   // Half Note 
#define W   2.88f   // Whole Note

// ==========================================
// MUSICAL MACROS (6 Sixteenths per Measure)
// ==========================================

// --- Section A ---
#define M_PICKUP      REST(S), REST(S), REST(S), REST(S), NOTE(E5, S), NOTE(Ds5, S)
#define M_THEME_0     NOTE(E5, S), NOTE(Ds5, S), NOTE(E5, S), NOTE(B4, S), NOTE(D5, S), NOTE(C5, S)
#define M_ARPG_AMIN   CHORD(S, A2, A4), NOTE(E3, S), NOTE(A3, S), NOTE(C4, S), NOTE(E4, S), NOTE(A4, S)
#define M_ARPG_EMAJ   CHORD(S, E2, B4), NOTE(E3, S), NOTE(Gs3, S), NOTE(E4, S), NOTE(Gs4, S), NOTE(B4, S)
#define M_THEME_1     CHORD(S, A2, C5), NOTE(E3, S), NOTE(A3, S), REST(S), NOTE(E5, S), NOTE(Ds5, S)
#define M_ARPG_EMAJ_2 CHORD(S, E2, B4), NOTE(E3, S), NOTE(Gs3, S), NOTE(E4, S), NOTE(C5, S), NOTE(B4, S)
#define M_RESOLVE     CHORD(S, A2, A4), NOTE(E3, S), NOTE(A3, S)

// --- Section B ---
#define M_B_TRANS     REST(S), NOTE(B4, S), NOTE(C5, S), NOTE(D5, S)
#define M_B_CMAJ      CHORD(S, C3, E5), NOTE(G3, S), NOTE(C4, S), NOTE(G4, S), NOTE(F5, S), NOTE(E5, S)
#define M_B_GMAJ      CHORD(S, G2, D5), NOTE(G3, S), NOTE(B3, S), NOTE(F4, S), NOTE(E5, S), NOTE(D5, S)
#define M_B_AMIN      CHORD(S, A2, C5), NOTE(E3, S), NOTE(A3, S), NOTE(E4, S), NOTE(D5, S), NOTE(C5, S)
#define M_B_EMAJ      CHORD(S, E2, B4), NOTE(E3, S), NOTE(E4, S), NOTE(E5, S), REST(S), NOTE(E5, S)
#define M_B_CLIMAX    CHORD(S, E2, E5), NOTE(E6, S), NOTE(Ds5, S), NOTE(E5, S), NOTE(Ds5, S), REST(S)

// --- Section C (Synthwave Breakdown) ---
#define M_C_PEDAL_A   CHORD(S, A1, A2, E3), CHORD(S, A1, A2, E3), CHORD(S, A1, A2, E3), CHORD(S, A1, A2, E3), CHORD(S, A1, A2, E3), CHORD(S, A1, A2, E3)
#define M_C_PEDAL_B   CHORD(S, Bb1, Bb2, D3), CHORD(S, Bb1, Bb2, D3), CHORD(S, Bb1, Bb2, D3), CHORD(S, Bb1, Bb2, D3), CHORD(S, Bb1, Bb2, D3), CHORD(S, Bb1, Bb2, D3)


int main() {
    if (!synth_start_track("fur_elise.wav")) return 1;

    printf("Rendering 'Fur Elise (Cyber-Baroque Cut)'...\n");

    // --- INSTRUMENT DESIGN ---
    // A punchy synth piano with a strong sustain.
    // The ADSR bug is fixed, so notes will gracefully release!
    const ADSR SYNTH_PIANO = {0.02f, 0.25f, 0.5f, 0.8f};

    (void)PLAY(
        SET_OSC(0, WAVE_SQUARE, 0.5f, 0.0f),  // Main body
        SET_OSC(1, WAVE_SINE, 0.8f, 0.05f),   // Smooth chorus/thickness
        SET_OSC(2, WAVE_SAW, 0.35f, -12.0f),  // Warm sub-bass
        SET_ADSR(SYNTH_PIANO),
        SET_DELAY(0.36f, 0.4f, 0.25f),        // Echo matches exactly to an Eighth note
        SET_VIBRATO(0.06f, 5.0f),
        SET_CRUSH(16)                         // Clean 16-bit to start
    );

    // ==========================================
    // 1. SECTION A (INTRO & REPEAT)
    // ==========================================
    (void)PLAY(M_PICKUP);
    for (int i = 0; i < 2; i++) {
        (void)PLAY(M_THEME_0, M_ARPG_AMIN, M_ARPG_EMAJ, M_THEME_1);
        (void)PLAY(M_THEME_0, M_ARPG_AMIN, M_ARPG_EMAJ_2);
        if (i == 0) {
            (void)PLAY(M_RESOLVE, REST(S), NOTE(E5, S), NOTE(Ds5, S)); 
        } else {
            (void)PLAY(M_RESOLVE, M_B_TRANS);
        }
    }

    // ==========================================
    // 2. SECTION B (DEVELOPMENT)
    // ==========================================
    (void)PLAY(
        M_B_CMAJ, M_B_GMAJ, M_B_AMIN, M_B_EMAJ, 
        M_B_CLIMAX, M_THEME_0, M_ARPG_AMIN, M_ARPG_EMAJ, 
        M_THEME_1, M_THEME_0, M_ARPG_AMIN, M_ARPG_EMAJ_2,
        M_RESOLVE, REST(S), REST(S), REST(S)
    );

    // ==========================================
    // 3. SECTION C (SYNTHWAVE BREAKDOWN)
    // ==========================================
    (void)PLAY(
        SET_OSC(0, WAVE_SAW, 0.7f, 0.0f),
        SET_OSC(1, WAVE_SQUARE, 0.4f, 12.0f), // +1 octave for screaming highs
        SET_CRUSH(6),                         // Heavy Chiptune grit
        SET_VIBRATO(0.15f, 8.0f)              // Fast nervous vibrato
    );

    (void)PLAY(
        M_C_PEDAL_A, M_C_PEDAL_B, M_C_PEDAL_A,
        // Diminished tension block
        CHORD(E, Gs1, Gs2, D3), NOTE(E5, S), NOTE(D5, S), NOTE(C5, S), NOTE(B4, S),
        // Epic Ascending run (2 measures of 16ths)
        NOTE(A2, S), NOTE(C3, S), NOTE(E3, S), NOTE(A3, S), NOTE(C4, S), NOTE(E4, S), 
        NOTE(A4, S), NOTE(C5, S), NOTE(E5, S), NOTE(A5, S), NOTE(C6, S), NOTE(E6, S)
    );

    // ==========================================
    // 4. RETURN TO A & OUTRO
    // ==========================================
    (void)PLAY(
        SET_OSC(0, WAVE_SQUARE, 0.5f, 0.0f),
        SET_OSC(1, WAVE_SINE, 0.8f, 0.05f),
        SET_CRUSH(16),                        // Clean sound restored
        SET_VIBRATO(0.06f, 5.0f),
        REST(E), NOTE(E5, S), NOTE(Ds5, S)    // Dramatic pause & pickup
    );

    (void)PLAY(
        M_THEME_0, M_ARPG_AMIN, M_ARPG_EMAJ, M_THEME_1,
        M_THEME_0, M_ARPG_AMIN, M_ARPG_EMAJ_2
    );

    // Final majestic chords
    (void)PLAY(
        CHORD(Q, A1, A2, A3, C4, E4, A4),
        CHORD(Q, E1, E2, Gs3, B3, E4, Gs4),
        CHORD(W, A0, A1, A2, E3, A3, C4, E4, A4)
    );

    // Important: Ring out the delay tails and release phase
    (void)PLAY(REST(W), REST(W), REST(W));

    if (!synth_end_track()) return 1;
    
    printf("Track saved! You can now analyze or play the complete masterpiece.\n");
    return 0;
}