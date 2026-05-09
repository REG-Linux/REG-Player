#pragma once

namespace fft {

// FFT window size (power of 2). 1024 @ 44.1 kHz = ~23 ms window, 43 Hz bin.
constexpr int N = 1024;

// Compute log-binned magnitude spectrum from N mono float samples and write
// `num_bars` normalised values [0..1] into `out`. Applies a Hann window and
// log-spaces bars from ~50 Hz to ~12 kHz at the given sample rate.
void compute_bars(const float* samples, int sample_rate, float* out, int num_bars);

} // namespace fft
