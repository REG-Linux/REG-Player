#include "fft.h"
#include "kissfft/kiss_fftr.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace fft {

static kiss_fftr_cfg g_cfg = nullptr;
static std::vector<float> g_window;            // precomputed Hann
static std::vector<kiss_fft_cpx> g_freq;       // N/2 + 1 bins

static void ensure_init() {
    if (g_cfg) return;
    g_cfg = kiss_fftr_alloc(N, 0, nullptr, nullptr);
    g_window.resize(N);
    for (int i = 0; i < N; ++i) {
        g_window[i] = 0.5f - 0.5f * std::cos(2.0f * (float)M_PI * i / (N - 1));
    }
    g_freq.resize(N / 2 + 1);
}

void compute_bars(const float* samples, int sample_rate, float* out, int num_bars) {
    ensure_init();
    if (sample_rate <= 0) sample_rate = 44100;

    // Window the input.
    std::vector<float> windowed(N);
    for (int i = 0; i < N; ++i) windowed[i] = samples[i] * g_window[i];

    kiss_fftr(g_cfg, windowed.data(), g_freq.data());

    // Magnitude per bin.
    const int bins = N / 2 + 1;
    std::vector<float> mag(bins);
    for (int i = 0; i < bins; ++i) {
        float r = g_freq[i].r, im = g_freq[i].i;
        mag[i] = std::sqrt(r * r + im * im);
    }

    // Log-spaced bands: 50 Hz → 12 kHz.
    const float f_lo = 50.0f, f_hi = 12000.0f;
    const float log_lo = std::log10(f_lo), log_hi = std::log10(f_hi);
    const float bin_hz = (float)sample_rate / N;

    for (int b = 0; b < num_bars; ++b) {
        float t0 = (float)b / num_bars;
        float t1 = (float)(b + 1) / num_bars;
        float f0 = std::pow(10.0f, log_lo + t0 * (log_hi - log_lo));
        float f1 = std::pow(10.0f, log_lo + t1 * (log_hi - log_lo));
        int i0 = std::clamp((int)std::floor(f0 / bin_hz), 1, bins - 1);
        int i1 = std::clamp((int)std::ceil (f1 / bin_hz), i0 + 1, bins);

        // Peak within the band — sharper response than mean for music.
        float peak = 0;
        for (int i = i0; i < i1; ++i) if (mag[i] > peak) peak = mag[i];

        // Log-compress + 1/f tilt comp (high freqs naturally weaker).
        float tilt = 1.0f + 0.6f * t0;
        float v = std::log10(1.0f + peak * 8.0f * tilt) * 0.55f;
        out[b] = std::clamp(v, 0.0f, 1.0f);
    }
}

} // namespace fft
