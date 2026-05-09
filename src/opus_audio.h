#pragma once
#include <string>
#include <vector>

namespace opus_audio {

bool is_opus(const std::string& path);

// Decode entire .opus file. Returns sample rate (always 48000 — opusfile
// resamples internally), 0 on failure. Fills `stereo` interleaved L,R,L,R
// float PCM and `mono` per-frame averaged L+R.
int decode_to_pcm(const std::string& path,
                  std::vector<float>& stereo,
                  std::vector<float>& mono);

} // namespace opus_audio
