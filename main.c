#include "synth_dsl.h"
#include <stdio.h>

int main() {
  if (!synth_start_track("masterpiece.wav")) {
      return 1;
  }

  const bool ok = PLAY(
    // Track 1: An Ethereal Pad
    SET_WAVE(WAVE_SAW),
    SET_ADSR(ADSR_PAD),
    SET_VIBRATO(0.5f, 3.0f),
    SET_DELAY(0.5f, 0.4f, 0.4f), 
    
    CHORD(W, C4, E4, G4, C5),
    CHORD(W, F4, A4, C5, E5),
    
    // Track 2: Retro Arpeggio (Overwriting settings mid-sequence)
    SET_WAVE(WAVE_SQUARE),
    SET_ADSR(ADSR_PLUCK),
    SET_VIBRATO(0.0f, 0.0f), // Turn off vibrato
    SET_DELAY(0.375f, 0.5f, 0.5f), // Dotted-eighth delay
    
    NOTE(C4, E), NOTE(E4, E), NOTE(G4, E), NOTE(C5, E),
    NOTE(B4, E), NOTE(G4, E), NOTE(E4, E), NOTE(D4, E),

    // Outro: Let the delay decay into silence
    REST(W), REST(W)
  );

  if (!ok || !synth_end_track()) {
    (void)fprintf(stderr, "Synthesis failed!\n");
    return 1;
  }

  (void)printf("Rendered masterpiece.wav\n");
  return 0;
}