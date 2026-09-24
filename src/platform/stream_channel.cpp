#include "stream_channel.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fc {

namespace {
Key key_from_name(const std::string& n) {
  auto one = [](char c) -> Key {
    if (c >= 'a' && c <= 'z') return (Key)((int)Key::A + (c - 'a'));
    if (c >= '0' && c <= '9') return (Key)((int)Key::Num0 + (c - '0'));
    return Key::None;
  };
  if (n.size() == 1) {
    Key k = one((char)std::tolower((unsigned char)n[0]));
    if (k != Key::None) return k;
  }
  if (n.rfind("f", 0) == 0 && n.size() <= 3) {
    int v = std::atoi(n.c_str() + 1);
    if (v >= 1 && v <= 12) return (Key)((int)Key::F1 + (v - 1));
  }
  static const struct {
    const char* name;
    Key key;
  } table[] = {
      {"space", Key::Space},     {"enter", Key::Enter},   {"return", Key::Enter},
      {"escape", Key::Escape},   {"esc", Key::Escape},    {"tab", Key::Tab},
      {"backspace", Key::Backspace}, {"up", Key::Up},     {"down", Key::Down},
      {"left", Key::Left},       {"right", Key::Right},   {"home", Key::Home},
      {"end", Key::End},         {"insert", Key::Insert}, {"delete", Key::Delete},
      {"minus", Key::Minus},     {"equal", Key::Equal},   {"comma", Key::Comma},
      {"period", Key::Period},   {"slash", Key::Slash},   {"backquote", Key::Backquote},
      {"lbracket", Key::LBracket}, {"rbracket", Key::RBracket},
      {"backslash", Key::Backslash}, {"semicolon", Key::Semicolon},
      {"quote", Key::Quote},     {"lshift", Key::LShift}, {"rshift", Key::RShift},
      {"lctrl", Key::LCtrl},     {"rctrl", Key::RCtrl},   {"lalt", Key::LAlt},
      {"ralt", Key::RAlt},
  };
  for (const auto& e : table)
    if (n == e.name) return e.key;
  return Key::None;
}

// Minimal JSON field extraction for the flat, single-line messages we use.
std::string jstr(const std::string& s, const std::string& key) {
  size_t p = s.find("\"" + key + "\"");
  if (p == std::string::npos) return "";
  p = s.find('"', p + key.size() + 2);
  if (p == std::string::npos) return "";
  size_t q = s.find('"', p + 1);
  return s.substr(p + 1, q - p - 1);
}
double jnum(const std::string& s, const std::string& key, double dflt = 0.0) {
  size_t p = s.find("\"" + key + "\"");
  if (p == std::string::npos) return dflt;
  p = s.find(':', p);
  if (p == std::string::npos) return dflt;
  return std::atof(s.c_str() + p + 1);
}
}  // namespace

bool StreamChannel::init(const std::string& dir, int width, int height) {
  frame_path_ = dir + "/fc_frame.rgb";
  input_path_ = dir + "/fc_input";
  events_path_ = dir + "/fc_events";
  w_ = width;
  h_ = height;
  int fd = open(frame_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return false;
  close(fd);
  // Reset input / events state for this run.
  input_off_ = 0;
  int fdi = open(input_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fdi >= 0) close(fdi);
  int fde = open(events_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fde >= 0) close(fde);
  return true;
}

void StreamChannel::shutdown() {
  // Leave the files; the preview server treats missing/stale frames as
  // "engine stopped".
}

void StreamChannel::publish_frame(const uint8_t* rgb8) {
  int fd = open(frame_path_.c_str(), O_WRONLY);
  if (fd < 0) return;
  const size_t total = (size_t)w_ * h_ * 3;
  const size_t chunk = 1u << 20;
  size_t off = 0;
  while (off < total) {
    size_t n = total - off < chunk ? total - off : chunk;
    ssize_t r = write(fd, rgb8 + off, n);
    if (r <= 0) break;
    off += (size_t)r;
  }
  close(fd);
}

void StreamChannel::drain_input(Input& input) {
  FILE* f = fopen(input_path_.c_str(), "rb");
  if (!f) return;
  fseek(f, input_off_, SEEK_SET);
  char buf[4096];
  for (;;) {
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    if (n == 0) break;
    buf[n] = 0;
    input_off_ = ftell(f);
    char* save = nullptr;
    for (char* line = strtok_r(buf, "\n", &save); line; line = strtok_r(nullptr, "\n", &save)) {
      if (!line[0]) continue;
      std::string s(line);
      std::string t = jstr(s, "t");
      if (t == "key") {
        Key k = key_from_name(jstr(s, "k"));
        if (k != Key::None) input.press_key(k, jnum(s, "d") != 0);
      } else if (t == "mouse") {
        float x = (float)jnum(s, "x"), y = (float)jnum(s, "y");
        input.set_mouse({x, y});
        std::string b = jstr(s, "b");
        if (!b.empty())
          input.press_button(b[0] == 'l' ? MouseButton::Left
                            : b[0] == 'm' ? MouseButton::Middle
                                          : MouseButton::Right,
                             jnum(s, "d") != 0);
      } else if (t == "scroll") {
        input.add_scroll((float)jnum(s, "dy"));
      }
    }
  }
  fclose(f);
}

void StreamChannel::append_event(const std::string& json) {
  FILE* f = fopen(events_path_.c_str(), "a");
  if (!f) return;
  fputs(json.c_str(), f);
  fputc('\n', f);
  fclose(f);
}

}  // namespace fc
