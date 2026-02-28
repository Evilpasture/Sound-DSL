#ifdef _WIN32
    #pragma warning(disable : 4996)
    #include <windows.h>
    #define THREAD_FUNC DWORD WINAPI
    #define THREAD_RET DWORD
    typedef HANDLE ThreadHandle;
    // Get CPU core count on Windows
    int get_core_count() {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
    }
#else
    #include <pthread.h>
    #include <unistd.h>
    #define THREAD_FUNC void*
    #define THREAD_RET void*
    typedef pthread_t ThreadHandle;
    // Get CPU core count on Linux/Mac
    int get_core_count() {
        return (int)sysconf(_SC_NPROCESSORS_ONLN);
    }
#endif

#include "synth_dsl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static constexpr int SAMPLE_RATE = 44100;
static constexpr double PI_VAL = 3.14159265358979323846;

// ==========================================
// SHARED DSP ENGINE
// ==========================================

static float sample_patch(const SynthPatch* p, float freq, float local_t, float total_dur) {
    if (local_t < 0.0f) return 0.0f;

    // 1. ADSR
    float amp = 0.0f;
    float rel_start = total_dur; // Note off time
    
    // Release Phase
    if (local_t > rel_start) {
        float rel_prog = (local_t - rel_start) / p->adsr.release_time;
        if (rel_prog >= 1.0f) return 0.0f;
        amp = p->adsr.sustain_lvl * (1.0f - rel_prog);
    } 
    // Attack Phase
    else if (local_t < p->adsr.attack_time) {
        // Safe div check
        amp = (p->adsr.attack_time > 0.0f) ? (local_t / p->adsr.attack_time) : 1.0f;
    } 
    // Decay Phase
    else if (local_t < p->adsr.attack_time + p->adsr.decay_time) {
        float decay_prog = (local_t - p->adsr.attack_time) / p->adsr.decay_time;
        amp = 1.0f - (1.0f - p->adsr.sustain_lvl) * decay_prog;
    } 
    // Sustain Phase
    else {
        amp = p->adsr.sustain_lvl;
    }

    if (amp <= 0.001f) return 0.0f;

    // 2. LFO
    double lfo_mod = 0.0;
    if (p->vibrato_rate > 0.0f && p->vibrato_depth > 0.0f) {
        double w = 2.0 * PI_VAL * p->vibrato_rate;
        lfo_mod = (p->vibrato_depth * 0.01) * ((1.0 - cos(w * local_t)) / w);
    }

    // 3. Multi-Oscillator Mix
    double mix = 0.0;
    for (int v = 0; v < 3; v++) {
        if (p->oscs[v].volume <= 0.001f) continue;
        
        double f_final = freq * pow(2.0, p->oscs[v].detune / 12.0);
        double phase_cycles = f_final * local_t + f_final * lfo_mod;
        
        double sig = 0.0;
        switch (p->oscs[v].type) {
            case WAVE_SQUARE: {
                double phase = fmod(phase_cycles, 1.0);
                sig = (phase < 0.5) ? 1.0 : -1.0;
                break;
            }
            case WAVE_SAW: {
                double phase = fmod(phase_cycles, 1.0);
                sig = 2.0 * phase - 1.0;
                break;
            }
            case WAVE_NOISE:
                sig = ((double)rand() / (double)RAND_MAX) * 2.0 - 1.0;
                break;
            case WAVE_SINE:
            default:
                sig = sin(2.0 * PI_VAL * phase_cycles);
                break;
        }
        mix += sig * p->oscs[v].volume;
    }
    
    // 4. Bitcrusher (Quantization)
    if (p->bit_depth > 0 && p->bit_depth < 16) {
        double steps = pow(2.0, p->bit_depth);
        mix = floor(mix * steps) / steps;
    }

    return (float)(mix * amp);
}

// ==========================================
// SEQUENCER DATA & THREADING
// ==========================================

typedef struct { int track; float freq; float start; float dur; } NoteEvent;

// Global Sequencer State
static NoteEvent* seq_events = NULL;
static int seq_event_count = 0;
static int seq_event_capacity = 0;
static SynthPatch seq_tracks[32] = {0};

// Render Buffer (The entire song in RAM)
static float* master_buffer = NULL;
static uint32_t total_frames = 0;

// Thread Context
typedef struct {
    uint32_t start_frame;
    uint32_t end_frame;
    int thread_id;
} RenderJob;

// --- WORKER THREAD FUNCTION ---
// Each thread takes a slice of time (start_frame to end_frame)
// and calculates the audio for that slice.
// Worker thread logic - optimized to O(Active Notes)
THREAD_FUNC render_worker(void* arg) {
    RenderJob* job = (RenderJob*)arg;
    
    // We need a local way to track which notes are active in this thread's time slice
    // A simple bitmask or small array for polyphony (e.g., max 128 notes at once)
    int active_indices[256]; 
    int active_count = 0;
    int next_note_to_trigger = 0;

    // Fast-forward next_note_to_trigger to the start of this thread's time slice
    float thread_start_time = (float)job->start_frame / (float)SAMPLE_RATE;
    while (next_note_to_trigger < seq_event_count && 
           seq_events[next_note_to_trigger].start < thread_start_time) {
        // If the note is already playing (or releasing) when the thread starts, add it
        float release = seq_tracks[seq_events[next_note_to_trigger].track].adsr.release_time;
        if (seq_events[next_note_to_trigger].start + seq_events[next_note_to_trigger].dur + release > thread_start_time) {
            if (active_count < 256) active_indices[active_count++] = next_note_to_trigger;
        }
        next_note_to_trigger++;
    }

    for (uint32_t i = job->start_frame; i < job->end_frame; i++) {
        float t = (float)i / (float)SAMPLE_RATE;
        float mix = 0.0f;

        // 1. Check if we need to trigger new notes
        while (next_note_to_trigger < seq_event_count && seq_events[next_note_to_trigger].start <= t) {
            if (active_count < 256) {
                active_indices[active_count++] = next_note_to_trigger;
            }
            next_note_to_trigger++;
        }

        // 2. Process active notes and remove dead ones
        for (int a = 0; a < active_count; ) {
            int note_idx = active_indices[a];
            NoteEvent* ev = &seq_events[note_idx];
            SynthPatch* p = &seq_tracks[ev->track];
            float end_time = ev->start + ev->dur + p->adsr.release_time;

            if (t < end_time) {
                mix += sample_patch(p, ev->freq, t - ev->start, ev->dur);
                a++; // Note is still alive
            } else {
                // Note is dead, swap with last and shrink (O(1) removal)
                active_indices[a] = active_indices[--active_count];
            }
        }
        
        mix *= 0.15f; 
        master_buffer[i] = mix;
    }
    return 0;
}

// ==========================================
// PUBLIC API IMPLEMENTATION
// ==========================================

void synth_seq_set_patch(int track_id, SynthPatch patch) {
    if (track_id >= 0 && track_id < 32) seq_tracks[track_id] = patch;
}

void synth_seq_add_note(int track_id, float freq, float start_time, float duration) {
    if (seq_event_count >= seq_event_capacity) {
        seq_event_capacity = (seq_event_capacity == 0) ? 4096 : seq_event_capacity * 2;
        seq_events = realloc(seq_events, seq_event_capacity * sizeof(NoteEvent));
    }
    seq_events[seq_event_count++] = (NoteEvent){track_id, freq, start_time, duration};
}

// --- MAIN RENDER ENTRY POINT ---
bool synth_seq_render(const char *filename) {
    // 1. Calculate Song Duration
    float max_t = 0;
    for(int i=0; i<seq_event_count; i++) {
        float end = seq_events[i].start + seq_events[i].dur + seq_tracks[seq_events[i].track].adsr.release_time;
        if (end > max_t) max_t = end;
    }
    max_t += 1.0f; // Tail padding
    
    total_frames = (uint32_t)(SAMPLE_RATE * max_t);
    printf(" Allocating %.2f MB for %.2fs of audio...\n", (total_frames * sizeof(float)) / (1024.0*1024.0), max_t);
    
    // 2. Allocate Master Buffer
    master_buffer = (float*)calloc(total_frames, sizeof(float));
    if (!master_buffer) return false;

    // 3. Prepare Threads
    int num_cores = get_core_count();
    // Cap cores to avoid overhead on small songs
    if (num_cores > 16) num_cores = 16; 
    if (num_cores < 1) num_cores = 1;
    
    printf(" Launching %d render threads...\n", num_cores);

    ThreadHandle* threads = malloc(sizeof(ThreadHandle) * num_cores);
    RenderJob* jobs = malloc(sizeof(RenderJob) * num_cores);
    
    uint32_t samples_per_thread = total_frames / num_cores;

    // 4. Dispatch Threads
    for (int i = 0; i < num_cores; i++) {
        jobs[i].thread_id = i;
        jobs[i].start_frame = i * samples_per_thread;
        jobs[i].end_frame = (i == num_cores - 1) ? total_frames : (i + 1) * samples_per_thread;

        #ifdef _WIN32
            threads[i] = CreateThread(NULL, 0, render_worker, &jobs[i], 0, NULL);
        #else
            pthread_create(&threads[i], NULL, render_worker, &jobs[i]);
        #endif
    }

    // 5. Join Threads (Wait)
    for (int i = 0; i < num_cores; i++) {
        #ifdef _WIN32
            WaitForSingleObject(threads[i], INFINITE);
            CloseHandle(threads[i]);
        #else
            pthread_join(threads[i], NULL);
        #endif
    }
    
    free(threads);
    free(jobs);
    printf(" Render complete. Writing to disk...\n");

    // 6. Write WAV File
    FILE* fp = fopen(filename, "wb");
    if (!fp) { free(master_buffer); return false; }

    // Write Header
    uint32_t data_size = total_frames * sizeof(int16_t);
    uint32_t file_size = 36 + data_size;
    uint32_t byte_rate = SAMPLE_RATE * 2;
    fwrite("RIFF", 4, 1, fp); fwrite(&file_size, 4, 1, fp); fwrite("WAVEfmt ", 8, 1, fp);
    uint32_t sub1 = 16; uint16_t fmt = 1, chan = 1; uint32_t sr = SAMPLE_RATE;
    uint16_t ba = 2, bps = 16;
    fwrite(&sub1, 4, 1, fp); fwrite(&fmt, 2, 1, fp); fwrite(&chan, 2, 1, fp);
    fwrite(&sr, 4, 1, fp); fwrite(&byte_rate, 4, 1, fp); fwrite(&ba, 2, 1, fp);
    fwrite(&bps, 2, 1, fp); fwrite("data", 4, 1, fp); fwrite(&data_size, 4, 1, fp);

    // Convert float buffer to int16 buffer for bulk write speed
    int16_t* disk_buffer = malloc(total_frames * sizeof(int16_t));
    for (uint32_t i = 0; i < total_frames; i++) {
        float s = master_buffer[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        disk_buffer[i] = (int16_t)(s * 32767.0f);
    }
    
    fwrite(disk_buffer, sizeof(int16_t), total_frames, fp);
    fclose(fp);

    free(disk_buffer);
    free(master_buffer);
    free(seq_events);
    seq_events = NULL;
    seq_event_count = 0;
    
    return true;
}
