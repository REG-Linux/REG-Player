#include "opus_audio.h"
#include <opus/opusfile.h>
#include <algorithm>
#include <cctype>

namespace opus_audio {

static std::string lower_ext(const std::string& path) {
    auto p = path.find_last_of('.');
    if (p == std::string::npos) return "";
    std::string e = path.substr(p);
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

bool is_opus(const std::string& path) {
    return lower_ext(path) == ".opus";
}

int decode_to_pcm(const std::string& path,
                  std::vector<float>& stereo,
                  std::vector<float>& mono) {
    stereo.clear();
    mono.clear();

    int err = 0;
    OggOpusFile* of = op_open_file(path.c_str(), &err);
    if (!of || err != 0) {
        if (of) op_free(of);
        return 0;
    }

    // op_pcm_total returns total decoded sample count at 48 kHz, summed across
    // links. Negative on unseekable/error.
    ogg_int64_t total = op_pcm_total(of, -1);
    if (total > 0) {
        stereo.reserve((size_t)total * 2);
        mono.reserve((size_t)total);
    }

    constexpr int kBufFrames = 5760; // libopusfile recommended max per call
    float buf[kBufFrames * 2];

    for (;;) {
        int read = op_read_float_stereo(of, buf, kBufFrames * 2);
        if (read < 0) break;       // hard error
        if (read == 0) break;       // EOF
        stereo.insert(stereo.end(), buf, buf + read * 2);
        for (int i = 0; i < read; ++i) {
            mono.push_back((buf[i * 2] + buf[i * 2 + 1]) * 0.5f);
        }
    }

    op_free(of);
    if (mono.empty()) return 0;
    return 48000;
}

} // namespace opus_audio
