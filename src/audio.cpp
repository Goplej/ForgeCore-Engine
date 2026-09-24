#include "fc/audio.hpp"

#include <cmath>
#include <cstdio>

namespace fc {

// ----------------------------------------------------------------- WavSink

WavSink::WavSink(std::string path) : path_(std::move(path)) {}

void WavSink::push(const float* samples, size_t count) {
  if (!file_) {
    file_ = std::fopen(path_.c_str(), "wb");
    if (!file_) return;
  }
  for (size_t i = 0; i < count; ++i) {
    int v = (int)std::clamp(samples[i] * 32000.0f, -32768.0f, 32767.0f);
    uint8_t lo = (uint8_t)(v & 0xFF), hi = (uint8_t)((v >> 8) & 0xFF);
    std::fputc(lo, (FILE*)file_);
    std::fputc(hi, (FILE*)file_);
    ++written_;
  }
}

void WavSink::flush() {
  if (!file_) return;
  std::fflush((FILE*)file_);
  // Rewrite a proper RIFF header (16-bit mono).
  long pos = std::ftell((FILE*)file_);
  std::fseek((FILE*)file_, 0, SEEK_SET);
  uint32_t datalen = (uint32_t)(written_ * 2);
  uint32_t chunk = 36 + datalen;
  const char riff[4] = {'R', 'I', 'F', 'F'};
  const char wave[4] = {'W', 'A', 'V', 'E'};
  const char fmt[4] = {'f', 'm', 't', ' '};
  const char data[4] = {'d', 'a', 't', 'a'};
  uint16_t audio_format = 1, channels = 1, bits = 16;
  uint32_t rate = 44100;
  uint16_t blockalign = 2, byte_rate = rate * 2;
  std::fwrite(riff, 1, 4, (FILE*)file_);
  std::fwrite(&chunk, 1, 4, (FILE*)file_);
  std::fwrite(wave, 1, 4, (FILE*)file_);
  std::fwrite(fmt, 1, 4, (FILE*)file_);
  uint32_t fmtlen = 16;
  std::fwrite(&fmtlen, 1, 4, (FILE*)file_);
  std::fwrite(&audio_format, 1, 2, (FILE*)file_);
  std::fwrite(&channels, 1, 2, (FILE*)file_);
  std::fwrite(&rate, 1, 4, (FILE*)file_);
  std::fwrite(&byte_rate, 1, 4, (FILE*)file_);
  std::fwrite(&blockalign, 1, 2, (FILE*)file_);
  std::fwrite(&bits, 1, 2, (FILE*)file_);
  std::fwrite(data, 1, 4, (FILE*)file_);
  std::fwrite(&datalen, 1, 4, (FILE*)file_);
  std::fseek((FILE*)file_, pos, SEEK_SET);
}

// ------------------------------------------------------------------- Audio

void Audio::init() {
  voices_.resize(kVoices);
  inited_ = true;
}

void Audio::shutdown() {
  if (sink_) sink_->flush();
  inited_ = false;
}

int Audio::play(const SoundParams& p, const std::string& name) {
  if (!inited_) return -1;
  int slot = -1;
  for (int i = 0; i < kVoices; ++i)
    if (!voices_[i].active) { slot = i; break; }
  if (slot < 0) {
    slot = 0;  // steal the oldest-free voice
    for (int i = 1; i < kVoices; ++i)
      if (voices_[i].t > voices_[slot].t) slot = i;
  }
  voices_[slot].p = p;
  voices_[slot].t = 0;
  voices_[slot].phase = 0;
  voices_[slot].active = true;
  ++sounds_played_;
  if (listener_) {
    AudioEvent ev;
    ev.name = name;
    ev.params = p;
    listener_(ev);
  }
  return slot;
}

void Audio::step_voice(Voice& v, float dt) {
  v.t += dt;
  float f = v.p.freq0 + (v.p.freq1 - v.p.freq0) * (v.t / std::max(v.p.duration, 1e-4f));
  v.phase += f * dt;
  if (v.t >= v.p.duration) v.active = false;
}

void Audio::render_sample(float out) {
  out = 0;
  for (auto& v : voices_) {
    if (!v.active) continue;
    float t = v.t;
    float env = 1.0f;
    if (t < v.p.attack) env = t / std::max(v.p.attack, 1e-4f);
    env *= std::exp(-v.p.decay * (t / std::max(v.p.duration, 1e-4f)));
    float x = fmod(v.phase, 1.0f);
    float s;
    switch (v.p.wave) {
      case Wave::Sine: s = std::sin(x * 2.0f * 3.14159265f); break;
      case Wave::Square: s = x < 0.5f ? 1.0f : -1.0f; break;
      case Wave::Saw: s = x * 2.0f - 1.0f; break;
      case Wave::Noise: {
        v.noise_seed = (v.noise_seed * 1103515245 + 12345) & 0x7fffffff;
        s = (float)v.noise_seed / 2147483647.0f * 2.0f - 1.0f;
        break;
      }
    }
    out += s * v.p.volume * env;
  }
  if (out > 1.0f) out = 1.0f;
  if (out < -1.0f) out = -1.0f;
}

void Audio::update(float dt) {
  if (!inited_ || !sink_) return;
  double samples = std::floor(dt * (double)kSampleRate + sample_carry_);
  if (samples < 1) { sample_carry_ += dt * (double)kSampleRate; return; }
  sample_carry_ = dt * (double)kSampleRate - samples;
  int n = (int)std::min(samples, (double)(kSampleRate / 2));
  std::vector<float> buf(n);
  float sdt = 1.0f / kSampleRate;
  for (int i = 0; i < n; ++i) {
    for (auto& v : voices_)
      if (v.active) step_voice(v, sdt);
    render_sample(buf[i]);
  }
  sink_->push(buf.data(), buf.size());
}

void Audio::set_sink(std::unique_ptr<ISink> sink) { sink_ = std::move(sink); }
void Audio::set_listener(std::function<void(const AudioEvent&)> fn) { listener_ = std::move(fn); }

}  // namespace fc
