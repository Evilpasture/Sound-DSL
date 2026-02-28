import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile
from scipy.fft import rfft, rfftfreq
import sys

def analyze_music_c_binary(filename):
    try:
        sample_rate, data = wavfile.read(filename)
    except FileNotFoundError:
        print(f"❌ Error: {filename} not found. Build your C code first!")
        return

    # Handle Stereo/Mono
    if len(data.shape) > 1:
        channels = data.shape[1]
        data_float = data.astype(np.float32)[:, 0] / 32768.0
    else:
        channels = 1
        data_float = data.astype(np.float32) / 32768.0

    duration = len(data) / sample_rate

    # --- 1. INTELLIGENT PEAK FINDER ---
    # Instead of the middle, find the 100ms window with the most energy
    window_size = int(sample_rate * 0.1) 
    num_windows = len(data_float) // window_size
    energies = [np.sum(data_float[i*window_size:(i+1)*window_size]**2) for i in range(num_windows)]
    max_energy_idx = np.argmax(energies) * window_size
    
    # Analyze frequency at the peak energy point
    fft_size = 8192
    sample_slice = data_float[max_energy_idx : max_energy_idx + fft_size]
    if len(sample_slice) < fft_size: # Pad if track is too short
        sample_slice = np.pad(sample_slice, (0, fft_size - len(sample_slice)))
        
    yf = rfft(sample_slice)
    xf = rfftfreq(fft_size, 1 / sample_rate)
    mag = np.abs(yf)
    peak_freq = xf[np.argmax(mag)]

    # --- 2. SIGNAL STATS ---
    peak_amp = np.max(np.abs(data_float))
    rms_amp = np.sqrt(np.mean(data_float**2))
    crest_factor = peak_amp / rms_amp if rms_amp > 0 else 0
    db_peak = 20 * np.log10(peak_amp) if peak_amp > 0 else -100

    print("="*50)
    print(f"🎼 SYNTH-DSL MASTER DEBUGGER: {filename}")
    print("="*50)
    print(f"⏱️  File Duration:   {duration:.2f} seconds")
    print(f"🔊 Channels:        {'Stereo' if channels > 1 else 'Mono'}")
    print(f"⚡ Peak Note Freq:  {peak_freq:.2f} Hz")
    print(f"📈 Peak Amplitude:  {peak_amp:.4f} ({db_peak:.2f} dB)")
    print(f"📊 Crest Factor:    {crest_factor:.2f} (Punchiness)")
    
    # Diagnostic Logic
    if peak_amp >= 0.99:
        print("🔴 STATUS: CLIPPING! Lower your oscillator volumes in C.")
    elif peak_amp < 0.2:
        print("🟡 STATUS: QUIET. You have plenty of headroom to boost.")
    else:
        print("🟢 STATUS: HEALTHY. Levels are optimal.")
    print("-" * 50)

    # --- 3. THE VISUALS ---
    fig = plt.figure(figsize=(14, 10), facecolor='#121212')
    plt.rcParams['text.color'] = 'white'
    plt.rcParams['axes.labelcolor'] = 'white'
    plt.rcParams['xtick.color'] = 'white'
    plt.rcParams['ytick.color'] = 'white'

    # Subplot 1: Full Waveform
    ax1 = plt.subplot(3, 1, 1)
    time = np.linspace(0, duration, len(data_float))
    ax1.plot(time, data_float, color='#00ffcc', linewidth=0.3)
    ax1.set_title("Time Domain: Waveform Envelope", fontweight='bold')
    ax1.set_facecolor('#1a1a1a')

    # Subplot 2: Spectrogram (The "Heatmap")
    ax2 = plt.subplot(3, 1, 2)
    # Filter out zeros for log10 warning
    spec_data = data_float + 1e-10 
    Pxx, freqs, bins, im = ax2.specgram(spec_data, Fs=sample_rate, NFFT=2048, cmap='magma')
    ax2.set_title("Frequency Over Time (Spectrogram)", fontweight='bold')
    ax2.set_ylim(0, 8000) # Musical range
    ax2.set_ylabel("Hz")
    plt.colorbar(im, ax=ax2, label="dB")

    # Subplot 3: Harmonic Snapshot (FFT)
    ax3 = plt.subplot(3, 1, 3)
    ax3.plot(xf, 20 * np.log10(mag + 1e-10), color='#ff007f')
    ax3.set_title(f"Harmonic Snapshot at {max_energy_idx/sample_rate:.2f}s", fontweight='bold')
    ax3.set_xlim(0, 5000)
    ax3.set_ylabel("Magnitude (dB)")
    ax3.set_xlabel("Frequency (Hz)")
    ax3.set_facecolor('#1a1a1a')
    ax3.grid(True, alpha=0.1)

    plt.tight_layout()
    print("🎨 Rendering plots... (Close window to exit)")
    plt.show()

if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "fur_elise.wav"
    analyze_music_c_binary(target)