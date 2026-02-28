#ifdef _WIN32
#pragma warning(disable : 4996) // Disable 'fopen' deprecation warning on MSVC
#endif

#include "synth_dsl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h> // For rand()

// --- INTERNAL CONSTANTS ---
constexpr int SAMPLE_RATE = 44100;
constexpr double PI_VAL = 3.14159265358979323846;
constexpr double MASTER_VOLUME = 0.3; // Safely scaled to accommodate 3 oscillators + chords
constexpr uint32_t DELAY_MAX = 44100; // 1 second of delay buffer at 44.1kHz

// --- INTERNAL STATE STRUCTURES ---
typedef struct {
    float buffer[DELAY_MAX];
    uint32_t pos;
    float feedback;
    float mix;
    uint32_t length;
} DelayLine;

typedef struct {
    WaveType type;
    float volume;
    float detune; // In semitones
} SynthOsc;

typedef struct {
    FILE *fp;
    uint32_t samples;
    bool error;
    SynthOsc voices[3];
    ADSR adsr;
    float vibrato_depth; 
    float vibrato_rate;  
    DelayLine delay;
    int bit_depth;
} SoundContext;

// Global internal context (zero-initialized automatically)
static SoundContext ctx = {
    .fp = nullptr, 
    .samples = 0, 
    .error = false,
    .voices = {
        {WAVE_SINE, 1.0f, 0.0f}, // Voice 0: On by default
        {WAVE_SINE, 0.0f, 0.0f}, // Voice 1: Off
        {WAVE_SINE, 0.0f, 0.0f}  // Voice 2: Off
    },
    .adsr = {0.01f, 0.00f, 1.0f, 0.05f}, // Basic Organ ADSR
    .vibrato_depth = 0.0f, 
    .vibrato_rate = 5.0f,
    .bit_depth = 16 // Full quality
};

// --- WAV HEADER LOGIC ---
[[nodiscard]]
static bool write_wav_header(FILE *f, uint32_t sample_count) {
    if (f == nullptr) return false;
    if (fseek(f, 0, SEEK_SET) != 0) return false;

    const uint32_t data_size = sample_count * (uint32_t)sizeof(int16_t);
    const uint32_t file_size = 36u + data_size;
    const uint32_t byte_rate = (uint32_t)SAMPLE_RATE * (uint32_t)sizeof(int16_t);

    // Lambda-like macro for clean logical AND chaining
    #define WRITE_RAW(ptr, sz) (fwrite((ptr), (sz), 1, f) == 1)

    static constexpr uint32_t sub1_size = 16;
    static constexpr uint16_t audio_format = 1;
    static constexpr uint16_t num_channels = 1;
    static constexpr uint32_t s_rate = (uint32_t)SAMPLE_RATE;
    static constexpr uint16_t block_align = 2;
    static constexpr uint16_t bits_per_sample = 16;

    const bool ok = WRITE_RAW("RIFF", 4)
                 && WRITE_RAW(&file_size, 4)
                 && WRITE_RAW("WAVEfmt ", 8)
                 && WRITE_RAW(&sub1_size, 4)
                 && WRITE_RAW(&audio_format, 2)
                 && WRITE_RAW(&num_channels, 2)
                 && WRITE_RAW(&s_rate, 4)
                 && WRITE_RAW(&byte_rate, 4)
                 && WRITE_RAW(&block_align, 2)
                 && WRITE_RAW(&bits_per_sample, 2)
                 && WRITE_RAW("data", 4)
                 && WRITE_RAW(&data_size, 4);

    #undef WRITE_RAW
    return ok;
}

// --- PUBLIC C CONFIGURATION API ---

// Resets the synth to a single oscillator (Voice 0) and mutes the others.
bool synth_set_wave(WaveType wave) {
    // Voice 0: The selected wave, full volume, centered tuning
    ctx.voices[0].type = wave;
    ctx.voices[0].volume = 1.0f;
    ctx.voices[0].detune = 0.0f;

    // Voice 1 & 2: Muted
    ctx.voices[1].volume = 0.0f;
    ctx.voices[2].volume = 0.0f;
    
    return true;
}

bool synth_set_osc(int index, WaveType type, float volume, float detune) {
    if (index < 0 || index > 2) return false;
    ctx.voices[index].type = type;
    ctx.voices[index].volume = volume;
    ctx.voices[index].detune = detune;
    return true;
}

bool synth_set_adsr(ADSR adsr) { 
    ctx.adsr = adsr; 
    return true; 
}

bool synth_set_vibrato(float depth, float rate) { 
    ctx.vibrato_depth = depth; 
    ctx.vibrato_rate = rate; 
    return true; 
}

bool synth_set_bit_depth(int bits) { 
    ctx.bit_depth = bits; 
    return true; 
}

bool synth_set_delay(float time, float feedback, float mix) {
    float safe_time = (time > 1.0f) ? 1.0f : ((time < 0.0f) ? 0.0f : time);
    ctx.delay.length = (uint32_t)(safe_time * (float)SAMPLE_RATE);
    ctx.delay.feedback = feedback;
    ctx.delay.mix = mix;
    return true;
}

// --- PUBLIC C LIFECYCLE API ---

bool synth_start_track(const char *filename) {
    ctx = (SoundContext){
        .fp = fopen(filename, "wb"), 
        .samples = 0, 
        .error = false, 
        .voices = {{WAVE_SINE, 1.0f, 0.0f}, {WAVE_SINE, 0.0f, 0.0f}, {WAVE_SINE, 0.0f, 0.0f}},
        .adsr = {0.01f, 0.00f, 1.0f, 0.05f}, 
        .bit_depth = 16
    };
    if (ctx.fp == nullptr) return false;
    return write_wav_header(ctx.fp, 0);
}

bool synth_end_track(void) {
    bool success = !ctx.error;
    if (success && ctx.fp != nullptr) {
        success = write_wav_header(ctx.fp, ctx.samples);
    }
    if (ctx.fp != nullptr) {
        if (fclose(ctx.fp) != 0) {
            success = false;
        }
    }
    // Fully clear context to prevent accidental writes after close
    ctx = (SoundContext){.fp = nullptr, .samples = 0, .error = false};
    return success;
}

// --- THE RENDER ENGINE ---

bool synth_render_chord(const float *freqs, size_t count, float dur) {
    if (ctx.error || ctx.fp == nullptr || count == 0 || dur <= 0.0f) return false;

    const auto total_samples = (uint32_t)((double)SAMPLE_RATE * (double)dur);
    
    // --- PRE-CALCULATIONS ---
    
    // 1. ADSR
    const double A = (double)ctx.adsr.attack_time;
    const double D = (double)ctx.adsr.decay_time;
    const double S = (double)ctx.adsr.sustain_lvl;
    const double R = (double)ctx.adsr.release_time;
    
    // Limit the release tail to a maximum of 50% of the note's duration.
    // This ensures short notes aren't completely swallowed by their own release phase (preventing t_rel = 0).
    const double max_r = (double)dur * 0.5;
    const double act_r = (R < max_r) ? R : max_r;
    const double t_rel = (double)dur - act_r;

    double rel_lvl = S;
    if (t_rel < A) {
        rel_lvl = (A > 0.0) ? (t_rel / A) : 1.0;
    } else if (t_rel < A + D) {
        rel_lvl = (D > 0.0) ? (1.0 - (1.0 - S) * ((t_rel - A) / D)) : S;
    }

    // 2. Bitcrusher
    const bool crushing = (ctx.bit_depth < 16 && ctx.bit_depth > 0);
    const double crush_scale = crushing ? pow(2.0, (double)ctx.bit_depth - 1.0) : 1.0;

    // 3. Oscillator Tuning Multipliers (Semitones to Frequency ratios)
    double osc_mult[3];
    for (int v = 0; v < 3; ++v) {
        osc_mult[v] = pow(2.0, (double)ctx.voices[v].detune / 12.0);
    }

    // --- THE MASTER DSP LOOP ---

    #pragma unroll(4)
    for (auto i = 0u; i < total_samples; i++) {
        const double time_s = (double)i / (double)SAMPLE_RATE;
        
        // --- ADSR ENVELOPE ---
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

        // --- POLYPHONIC OSCILLATORS ---
        double mixed_val = 0.0;
        for (size_t n = 0; n < count; n++) {
            const double f0 = (double)freqs[n];
            if (f0 <= 0.0) continue; // Skip math for rests

            // LFO / Vibrato (Calculated via Integral to preserve FM phase)
            const double rate = (double)ctx.vibrato_rate;
            const double depth = (double)ctx.vibrato_depth * 0.01;
            double lfo_mod = 0.0;
            if (rate > 0.0 && depth > 0.0) {
                const double w = 2.0 * PI_VAL * rate;
                lfo_mod = depth * ((1.0 - cos(w * time_s)) / w); 
            }

            // Sum the 3 Voices
            double voices_sum = 0.0;
            for (int v = 0; v < 3; v++) {
                if (ctx.voices[v].volume <= 0.001f) continue;

                const double f_final = f0 * osc_mult[v]; 
                const double phase_cycles = f_final * time_s + f_final * lfo_mod;
                const double phase_rads = 2.0 * PI_VAL * phase_cycles;
                
                double sample_val = 0.0;
                switch (ctx.voices[v].type) {
                    case WAVE_SQUARE: 
                        sample_val = (sin(phase_rads) >= 0.0) ? 1.0 : -1.0; 
                        break;
                    case WAVE_SAW:    
                        sample_val = 2.0 * (fmod(phase_cycles, 1.0)) - 1.0; 
                        break;
                    case WAVE_NOISE:  
                        sample_val = ((double)rand() / (double)RAND_MAX) * 2.0 - 1.0; 
                        break;
                    case WAVE_SINE:
                    default:          
                        sample_val = sin(phase_rads); 
                        break;
                }
                voices_sum += sample_val * (double)ctx.voices[v].volume;
            }
            mixed_val += voices_sum;
        }

        // Apply Envelope and Average by Chord size
        const double dry = (mixed_val / (double)count) * env;
        
        // --- DELAY LINE (ECHO) ---
        double delayed = 0.0;
        if (ctx.delay.length > 0) {
            const uint32_t read_pos = (ctx.delay.pos + DELAY_MAX - ctx.delay.length) % DELAY_MAX;
            delayed = (double)ctx.delay.buffer[read_pos];
            
            // Feed back into the buffer
            ctx.delay.buffer[ctx.delay.pos] = (float)(dry + delayed * (double)ctx.delay.feedback);
            
            // Advance circular buffer
            ctx.delay.pos = (ctx.delay.pos + 1) % DELAY_MAX;
        }

        // --- MASTER MIX ---
        double out_val = dry + (delayed * (double)ctx.delay.mix);
        
        // --- BITCRUSHER ---
        if (crushing) {
            out_val = round(out_val * crush_scale) / crush_scale;
        }
        
        // --- HARD CLIPPER & INT16 CONVERSION ---
        out_val *= 32767.0 * MASTER_VOLUME;
        if (out_val > 32767.0) out_val = 32767.0;  
        if (out_val < -32768.0) out_val = -32768.0;

        const int16_t out_sample = (int16_t)out_val;
        
        if (fwrite(&out_sample, sizeof(int16_t), 1, ctx.fp) != 1) { 
            ctx.error = true; 
            return false; 
        }
    }
    ctx.samples += total_samples;
    return true;
}