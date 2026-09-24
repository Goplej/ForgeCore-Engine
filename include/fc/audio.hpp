#pragma once
// ForgeCore audio — procedural synth + mixer with pluggable sinks.
//
// The mixer renders into a 44.1 kHz float buffer each frame and hands chunks
// to output sinks:
//   * WavSink  - writes a .wav file (great for headless verification)
//   * (OpenAL / device sinks plug in the same way on desktop builds)
//
// AudioEvent listeners receive every scheduled sound, which is how the
// headless preview relay drives browser-side WebAudio so the demo is audible
// even when the engine has no sound device.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fc {

enum class Wave { Sine, Square, Saw, Noise };

struct SoundParams {
  float freq0 = 440.0f;
  float freq1 = 440.0f;  // sweep target
  float duration = 0.1f;
  float volume = 0.5f;
  Wave wave = Wave::Sine;
  float decay = 0.008f;   // exponential decay constant
  float attack = 0.004f;  // ADSR attack
};

struct AudioEvent {
  std::string name;
  SoundParams params;
};

class ISink {
 public:
  virtual ~ISink() = default;
  virtual const char* name() const = 0;
  virtual void push(const float* samples, size_t count) = 0;  // mono, 44100 Hz
  virtual void flush() = 0;
};

class WavSink : public ISink {
 public:
  explicit WavSink(std::string path);
  const char* name() const override { return "wav"; }
  void push(const float* samples, size_t count) override;
  void flush() override;

 private:
  std::string path_;
  void* file_ = nullptr;  // FILE*
  size_t written_ = 0;
  bool header_done_ = false;
};

class Audio {
 public:
  static constexpr int kSampleRate = 44100;
  static constexpr int kVoices = 16;

  void init();
  void shutdown();

  // Schedule a procedural sound. Returns the voice id or -1 if full.
  int play(const SoundParams& p, const std::string& name = "fx");

  // Advance the mixer by dt seconds (call once per rendered frame).
  void update(float dt);

  // Sink / listener wiring
  void set_sink(std::unique_ptr<ISink> sink);
  void set_listener(std::function<void(const AudioEvent&)> fn);

  size_t sounds_played() const { return sounds_played_; }

 private:
  struct Voice {
    SoundParams p;
    float t = 0;
    float phase = 0;
    int noise_seed = 0;
    bool active = false;
  };

  void render_sample(float out);
  void step_voice(Voice& v, float dt);

  std::vector<Voice> voices_;
  std::unique_ptr<ISink> sink_;
  std::function<void(const AudioEvent&)> listener_;
  bool inited_ = false;
  size_t sounds_played_ = 0;
  double sample_carry_ = 0;
};

}  // namespace fc
