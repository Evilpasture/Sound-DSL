#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

#include "synth_dsl.h"
#include <math.h>
#include <stdio.h>

// --- INTERNAL CONSTANTS ---
constexpr int SAMPLE_RATE = 44100;
constexpr float PI_VAL = 3.1415926535f;
constexpr float VOLUME = 0.4f;
constexpr uint32_t DELAY_MAX = 44100;

// --- INTERNAL STATE ---
typedef struct {
    float buffer[DELAY_MAX];
    uint32_t pos;
    float feedback;
    float mix;
    uint32_t length;
} DelayLine;

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

// Global internal state
static SoundContext ctx = {
    .fp = nullptr, .samples = 0, .error = false,
    .wave = WAVE_SINE, .adsr = {0.01f, 0.00f, 1.0f, 0.05f},
    .vibrato_depth = 0.0f, .vibrato_rate = 5.0f
};

// --- INTERNAL HELPERS ---
[[nodiscard]]
static bool write_wav_header(FILE *f, uint32_t sample_count) {
  if (f == nullptr) return false;
  if (fseek(f, 0, SEEK_SET) != 0) return false;

  const uint32_t data_size = sample_count * sizeof(int16_t);
  const uint32_t file_size = 36 + data_size;
  const uint32_t byte_rate = (uint32_t)SAMPLE_RATE * sizeof(int16_t);

#define WRITE_RAW(ptr, sz) (fwrite(ptr, (sz), 1, f) == 1)
  static constexpr uint32_t sub1_size = 16;
  static constexpr uint16_t audio_format = 1, num_channels = 1, block_align = 2, bits_per_sample = 16;
  static constexpr uint32_t s_rate = (uint32_t)SAMPLE_RATE;

  const bool ok = WRITE_RAW("RIFF", 4) && WRITE_RAW(&file_size, 4) && WRITE_RAW("WAVEfmt ", 8)
               && WRITE_RAW(&sub1_size, 4) && WRITE_RAW(&audio_format, 2) && WRITE_RAW(&num_channels, 2)
               && WRITE_RAW(&s_rate, 4) && WRITE_RAW(&byte_rate, 4) && WRITE_RAW(&block_align, 2)
               && WRITE_RAW(&bits_per_sample, 2) && WRITE_RAW("data", 4) && WRITE_RAW(&data_size, 4);
#undef WRITE_RAW
  return ok;
}

// --- PUBLIC C API ---

bool synth_set_wave(WaveType wave) { ctx.wave = wave; return true; }
bool synth_set_adsr(ADSR adsr) { ctx.adsr = adsr; return true; }
bool synth_set_vibrato(float depth, float rate) { ctx.vibrato_depth = depth; ctx.vibrato_rate = rate; return true; }
bool synth_set_delay(float time, float feedback, float mix) {
    ctx.delay.length = (uint32_t)(((time) > 1.0f ? 1.0f : (time)) * SAMPLE_RATE);
    ctx.delay.feedback = feedback;
    ctx.delay.mix = mix;
    return true;
}

bool synth_start_track(const char *filename) {
  ctx = (SoundContext){.fp = fopen(filename, "wb"), .samples = 0, .error = false, .wave = WAVE_SINE, .adsr = ADSR_ORGAN};
  if (ctx.fp == nullptr) return false;
  return write_wav_header(ctx.fp, 0);
}

bool synth_end_track(void) {
  bool success = !ctx.error;
  if (success && ctx.fp != nullptr) success = write_wav_header(ctx.fp, ctx.samples);
  if (ctx.fp != nullptr) {
      if (fclose(ctx.fp) != 0) success = false;
  }
  ctx = (SoundContext){.fp = nullptr, .samples = 0, .error = false};
  return success;
}

bool synth_render_chord(const float *freqs, size_t count, float dur) {
  if (ctx.error || ctx.fp == nullptr || count == 0) return false;

  const auto total_samples = (uint32_t)((float)SAMPLE_RATE * dur);
  const double A = (double)ctx.adsr.attack_time, D = (double)ctx.adsr.decay_time;
  const double S = (double)ctx.adsr.sustain_lvl, R = (double)ctx.adsr.release_time;
  const double act_r = (R < (double)dur) ? R : (double)dur;
  const double t_rel = (double)dur - act_r;

  double rel_lvl = S;
  if (t_rel < A) rel_lvl = (A > 0.0) ? (t_rel / A) : 1.0;
  else if (t_rel < A + D) rel_lvl = (D > 0.0) ? (1.0 - (1.0 - S) * ((t_rel - A) / D)) : S;

#pragma unroll(4)
  for (auto i = 0u; i < total_samples; i++) {
    const double time_s = (double)i / (double)SAMPLE_RATE;
    
    // 1. Envelope
    double env = 0.0;
    if (time_s >= t_rel) env = (act_r > 0.0) ? rel_lvl * (1.0 - ((time_s - t_rel) / act_r)) : 0.0;
    else if (time_s < A) env = (A > 0.0) ? (time_s / A) : 1.0;
    else if (time_s < A + D) env = (D > 0.0) ? (1.0 - (1.0 - S) * ((time_s - A) / D)) : S;
    else env = S;
    if (env < 0.0) env = 0.0;

    // 2. Oscillators
    double mixed_val = 0;
    for (size_t n = 0; n < count; n++) {
      const double f0 = (double)freqs[n];
      if (f0 <= 0.0) continue; 

      const double rate = (double)ctx.vibrato_rate, depth = (double)ctx.vibrato_depth * 0.01;
      double phase_cycles;
      if (rate > 0.0 && depth > 0.0) {
         const double w = 2.0 * (double)PI_VAL * rate;
         phase_cycles = f0 * time_s + f0 * depth * ((1.0 - cos(w * time_s)) / w);
      } else {
         phase_cycles = f0 * time_s;
      }
      
      const double phase_rads = 2.0 * (double)PI_VAL * phase_cycles;
      double sample_val = 0;
      switch (ctx.wave) {
        case WAVE_SQUARE: sample_val = (sin(phase_rads) >= 0) ? 1.0 : -1.0; break;
        case WAVE_SAW:    sample_val = 2.0 * (fmod(phase_cycles, 1.0)) - 1.0; break; 
        case WAVE_SINE:
        default:          sample_val = sin(phase_rads); break;
      }
      mixed_val += sample_val;
    }

    // 3. Dry Mix
    const double dry = (mixed_val / (double)count) * env;
    
    // 4. Delay Line
    double delayed = 0.0;
    if (ctx.delay.length > 0) {
        const uint32_t read_pos = (ctx.delay.pos + DELAY_MAX - ctx.delay.length) % DELAY_MAX;
        delayed = (double)ctx.delay.buffer[read_pos];
        ctx.delay.buffer[ctx.delay.pos] = (float)(dry + delayed * (double)ctx.delay.feedback);
        ctx.delay.pos = (ctx.delay.pos + 1) % DELAY_MAX;
    }

    // 5. Clipper & Output
    double out_val = (dry + delayed * (double)ctx.delay.mix) * 32767.0 * (double)VOLUME;
    if (out_val > 32767.0) out_val = 32767.0;  
    if (out_val < -32768.0) out_val = -32768.0;

    const int16_t sample = (int16_t)out_val;
    if (fwrite(&sample, sizeof(int16_t), 1, ctx.fp) != 1) {
      ctx.error = true;
      return false;
    }
  }
  ctx.samples += total_samples;
  return true;
}