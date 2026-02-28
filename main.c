#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>

// --- ENGINE CONSTANTS ---
[[maybe_unused]] static constexpr int SAMPLE_RATE = 44100;
[[maybe_unused]] static constexpr float PI_VAL = 3.1415926535f;
[[maybe_unused]] static constexpr float VOLUME = 0.4f;

// --- THE SCALE ---
[[maybe_unused]] static constexpr float C4 = 261.63f, D4 = 293.66f, E4 = 329.63f, 
                                         F4 = 349.23f, G4 = 392.00f, A4 = 440.00f, 
                                         B4 = 493.88f, C5 = 523.25f, E5 = 659.25f;

// --- DURATIONS ---
[[maybe_unused]] static constexpr float W = 1.0f, H = 0.5f, Q = 0.25f, E = 0.125f;

// --- SYNTH TYPES ---
typedef enum { WAVE_SINE, WAVE_SQUARE, WAVE_SAW } WaveType;

typedef struct {
  float attack_time;  
  float decay_time;   
  float sustain_lvl;  
  float release_time; 
} ADSR;

[[maybe_unused]] static constexpr ADSR ADSR_ORGAN = {0.01f, 0.00f, 1.0f, 0.05f};
[[maybe_unused]] static constexpr ADSR ADSR_PIANO = {0.01f, 0.30f, 0.2f, 0.40f};
[[maybe_unused]] static constexpr ADSR ADSR_PLUCK = {0.01f, 0.15f, 0.0f, 0.10f};
[[maybe_unused]] static constexpr ADSR ADSR_PAD   = {0.40f, 0.20f, 0.8f, 0.60f};

// --- DELAY LINE ---
[[maybe_unused]] static constexpr uint32_t DELAY_MAX = 44100; // 1 second memory

typedef struct {
    float buffer[DELAY_MAX];
    uint32_t pos;
    float feedback;  // How much signal goes back in
    float mix;       // How loud the echo is
    uint32_t length; // Delay time in samples
} DelayLine;

// --- DSL STATE ---
typedef struct {
  FILE *fp;
  uint32_t samples;
  bool error;
  WaveType wave;
  ADSR adsr;
  float vibrato_depth; 
  float vibrato_rate;  
  DelayLine delay;
} SoundContext;

static SoundContext synth_ctx = {
    .fp = nullptr, 
    .samples = 0, 
    .error = false,
    .wave = WAVE_SINE,
    .adsr = ADSR_ORGAN,
    .vibrato_depth = 0.0f,
    .vibrato_rate = 5.0f
    // Delay buffer is automatically zero-initialized by standard C
};

typedef struct { const float *freqs; size_t count; float dur; } Chord;

// --- CORE FUNCTIONS ---

[[nodiscard]]
static bool write_wav_header(FILE *f, uint32_t sample_count) {
  if (f == nullptr) return false;
  if (fseek(f, 0, SEEK_SET) != 0) return false;

  const uint32_t data_size = sample_count * sizeof(int16_t);
  const uint32_t file_size = 36 + data_size;
  const uint32_t byte_rate = (uint32_t)SAMPLE_RATE * sizeof(int16_t);

#define WRITE_RAW(ptr, sz) (fwrite(ptr, (sz), 1, f) == 1)

  static constexpr uint32_t sub1_size = 16;
  static constexpr uint16_t audio_format = 1;
  static constexpr uint16_t num_channels = 1;
  static constexpr uint32_t s_rate = (uint32_t)SAMPLE_RATE;
  static constexpr uint16_t block_align = 2;
  static constexpr uint16_t bits_per_sample = 16;

  const bool ok = WRITE_RAW("RIFF", 4) && WRITE_RAW(&file_size, 4) && WRITE_RAW("WAVEfmt ", 8)
               && WRITE_RAW(&sub1_size, 4) && WRITE_RAW(&audio_format, 2) && WRITE_RAW(&num_channels, 2)
               && WRITE_RAW(&s_rate, 4) && WRITE_RAW(&byte_rate, 4) && WRITE_RAW(&block_align, 2)
               && WRITE_RAW(&bits_per_sample, 2) && WRITE_RAW("data", 4) && WRITE_RAW(&data_size, 4);

#undef WRITE_RAW
  return ok;
}

[[nodiscard]]
static bool render_chord(Chord c) {
  if (synth_ctx.error || synth_ctx.fp == nullptr || c.count == 0) return false;

  const auto total_samples = (uint32_t)((float)SAMPLE_RATE * c.dur);
  const double A = (double)synth_ctx.adsr.attack_time;
  const double D = (double)synth_ctx.adsr.decay_time;
  const double S = (double)synth_ctx.adsr.sustain_lvl;
  const double R = (double)synth_ctx.adsr.release_time;
  
  const double act_r = (R < (double)c.dur) ? R : (double)c.dur;
  const double t_rel = (double)c.dur - act_r;

  double rel_lvl = S;
  if (t_rel < A) {
      rel_lvl = (A > 0.0) ? (t_rel / A) : 1.0;
  } else if (t_rel < A + D) {
      rel_lvl = (D > 0.0) ? (1.0 - (1.0 - S) * ((t_rel - A) / D)) : S;
  }

#pragma unroll(4)
  for (auto i = 0u; i < total_samples; i++) {
    const double time_s = (double)i / (double)SAMPLE_RATE;
    
    // 1. Calculate ADSR Envelope
    double env = 0.0;
    if (time_s >= t_rel) { 
        env = (act_r > 0.0) ? rel_lvl * (1.0 - ((time_s - t_rel) / act_r)) : 0.0;
    } else if (time_s < A) { 
        env = (A > 0.0) ? (time_s / A) : 1.0;
    } else if (time_s < A + D) { 
        env = (D > 0.0) ? (1.0 - (1.0 - S) * ((time_s - A) / D)) : S;
    } else { 
        env = S;
    }
    if (env < 0.0) env = 0.0;

    // 2. Synthesize Oscillators
    double mixed_val = 0;
    for (size_t n = 0; n < c.count; n++) {
      const double f0 = (double)c.freqs[n];
      if (f0 <= 0.0) continue; // Skip math for rests, but keep looping for Delay!

      const double rate = (double)synth_ctx.vibrato_rate;
      const double depth = (double)synth_ctx.vibrato_depth * 0.01;
      
      double phase_cycles;
      if (rate > 0.0 && depth > 0.0) {
         const double w = 2.0 * (double)PI_VAL * rate;
         const double lfo_int = (1.0 - cos(w * time_s)) / w;
         phase_cycles = f0 * time_s + f0 * depth * lfo_int;
      } else {
         phase_cycles = f0 * time_s;
      }
      
      const double phase_rads = 2.0 * (double)PI_VAL * phase_cycles;
      double sample_val = 0;
      switch (synth_ctx.wave) {
        case WAVE_SQUARE: sample_val = (sin(phase_rads) >= 0) ? 1.0 : -1.0; break;
        case WAVE_SAW:    sample_val = 2.0 * (fmod(phase_cycles, 1.0)) - 1.0; break; 
        case WAVE_SINE:
        default:          sample_val = sin(phase_rads); break;
      }
      mixed_val += sample_val;
    }

    // 3. Apply Envelope (Dry Signal)
    const double dry = (mixed_val / (double)c.count) * env;
    
    // 4. Temporal DSP: Delay Line
    double delayed = 0.0;
    if (synth_ctx.delay.length > 0) {
        // Read past
        const uint32_t read_pos = (synth_ctx.delay.pos + DELAY_MAX - synth_ctx.delay.length) % DELAY_MAX;
        delayed = (double)synth_ctx.delay.buffer[read_pos];
        
        // Write present + decaying past
        synth_ctx.delay.buffer[synth_ctx.delay.pos] = (float)(dry + delayed * (double)synth_ctx.delay.feedback);
        
        // Advance buffer time
        synth_ctx.delay.pos = (synth_ctx.delay.pos + 1) % DELAY_MAX;
    }

    // 5. Final Master Mix and Hard Clipper
    double out_val = (dry + delayed * (double)synth_ctx.delay.mix) * 32767.0 * (double)VOLUME;
    if (out_val > 32767.0) out_val = 32767.0;   // Hard limit to prevent wrap-around
    if (out_val < -32768.0) out_val = -32768.0;

    const int16_t sample = (int16_t)out_val;

    if (fwrite(&sample, sizeof(int16_t), 1, synth_ctx.fp) != 1) {
      synth_ctx.error = true;
      return false;
    }
  }
  synth_ctx.samples += total_samples;
  return true;
}

// --- DSL INTERFACE ---

[[nodiscard]]
static bool start_track(const char *filename) {
  // Reset everything including the 176KB Delay buffer
  synth_ctx = (SoundContext){.fp = fopen(filename, "wb"), .samples = 0, .error = false, .wave = WAVE_SINE, .adsr = ADSR_ORGAN};
  if (synth_ctx.fp == nullptr) return false;
  return write_wav_header(synth_ctx.fp, 0);
}

[[nodiscard]]
static bool end_track() {
  bool success = !synth_ctx.error;
  if (success && synth_ctx.fp != nullptr) {
    success = write_wav_header(synth_ctx.fp, synth_ctx.samples);
  }
  if (synth_ctx.fp != nullptr) {
    if (fclose(synth_ctx.fp) != 0) success = false;
  }
  synth_ctx = (SoundContext){.fp = nullptr, .samples = 0, .error = false};
  return success;
}

#define PLAY(...) ({ \
    bool synth_final_ok = true; \
    const bool synth_results[] = { __VA_ARGS__ }; \
    for (size_t synth_i = 0; synth_i < sizeof(synth_results)/sizeof(bool); ++synth_i) { \
        if (!synth_results[synth_i]) { synth_final_ok = false; break; } \
    } \
    synth_final_ok; \
})

#define SET_WAVE(w) (synth_ctx.wave = (w), true)
#define SET_ADSR(a) (synth_ctx.adsr = (a), true)
#define SET_VIBRATO(d, r) (synth_ctx.vibrato_depth = (d), synth_ctx.vibrato_rate = (r), true)
#define SET_DELAY(time, fb, m) ( \
    synth_ctx.delay.length = (uint32_t)(((time) > 1.0f ? 1.0f : (time)) * SAMPLE_RATE), \
    synth_ctx.delay.feedback = (fb), \
    synth_ctx.delay.mix = (m), \
    true \
)

#define NOTE(f, d) render_chord((Chord){ .freqs = (float[]){f}, .count = 1, .dur = d })
#define CHORD(d, ...) render_chord((Chord){ \
    .freqs = (float[]){ __VA_ARGS__ }, \
    .count = sizeof((float[]){ __VA_ARGS__ }) / sizeof(float), \
    .dur = d \
})
#define REST(d) render_chord((Chord){ .freqs = (float[]){0.0f}, .count = 1, .dur = d })

// --- MAIN ---

int main() {
  if (!start_track("delay_synth.wav")) return 1;

  const bool ok = PLAY(
    // Setup a Pluck synth with a "Dotted Eighth" delay
    // 0.375s echo time, 40% feedback, 50% wet volume
    SET_WAVE(WAVE_SQUARE),
    SET_ADSR(ADSR_PLUCK),
    SET_DELAY(0.375f, 0.4f, 0.5f), 
    
    // Play a fast sequence (Arpeggio)
    NOTE(C4, E), NOTE(G4, E), NOTE(E5, E), NOTE(C5, E),
    
    // Switch to a lush Pad with the same delay settings
    SET_WAVE(WAVE_SAW),
    SET_ADSR(ADSR_PAD),
    SET_VIBRATO(0.5f, 4.0f),
    CHORD(H, F4, A4, C5),

    // Let the echo ring out perfectly into silence
    REST(W), REST(H)
  );

  if (!ok || !end_track()) {
    (void)fprintf(stderr, "Synthesis error.\n");
    return 1;
  }

  return 0;
}