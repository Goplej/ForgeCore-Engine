#pragma once
// ForgeCore input — keyboard + mouse state with edge detection.
// Window backends push raw events; systems poll state.

#include "math.hpp"
#include <functional>
#include <string>

namespace fc {

enum class Key {
  None,
  // letters
  A, B, C, D, E, F, G, H, I, J, K, L, M,
  N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
  // digits
  Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
  // punctuation
  Space, Enter, Escape, Tab, Backspace, Minus, Equal, LBracket, RBracket,
  Backslash, Semicolon, Quote, Comma, Period, Slash, Backquote,
  // arrows + numpad-ish
  Up, Down, Left, Right,
  Home, End, PageUp, PageDown, Insert, Delete,
  F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
  LShift, RShift, LCtrl, RCtrl, LAlt, RAlt,
  Count,
};

enum class MouseButton { None, Left, Right, Middle, Count };

struct KeyCallback {
  std::function<void(Key, bool pressed)> on_key;
};

class Input {
 public:
  // polled state ---------------------------------------------------------
  bool key_down(Key k) const { return kstate_[(int)k]; }
  bool key_pressed(Key k) const { return kstate_[(int)k] && !kprev_[(int)k]; }  // rising edge
  bool key_released(Key k) const { return !kstate_[(int)k] && kprev_[(int)k]; }

  Vec2 mouse() const { return mouse_; }
  Vec2 mouse_delta() const { return mouse_ - mouse_prev_; }
  bool mouse_down(MouseButton b) const { return bstate_[(int)b]; }
  bool mouse_pressed(MouseButton b) const { return bstate_[(int)b] && !bprev_[(int)b]; }
  bool mouse_released(MouseButton b) const { return !bstate_[(int)b] && bprev_[(int)b]; }
  float scroll() const { return scroll_; }  // accumulates until frame end

  // event hook (optional, for audio blips / debug logs) ---------------------
  KeyCallback callback;

  // injection (called by window backends / headless driver) ------------------
  void press_key(Key k, bool down) {
    if (k != Key::None && k != Key::Count) {
      kstate_[(int)k] = down;
      if (callback.on_key) callback.on_key(k, down);
    }
  }
  void set_mouse(const Vec2& p) { mouse_ = p; }
  void press_button(MouseButton b, bool down) {
    if (b != MouseButton::None && b != MouseButton::Count) bstate_[(int)b] = down;
  }
  void add_scroll(float d) { scroll_ += d; }

  // frame sync ---------------------------------------------------------------
  void begin_frame() {}
  void end_frame() {
    for (size_t i = 0; i < (size_t)Key::Count; ++i) kprev_[i] = kstate_[i];
    for (size_t i = 0; i < (size_t)MouseButton::Count; ++i) bprev_[i] = bstate_[(int)i];
    mouse_prev_ = mouse_;
    scroll_ = 0;
  }

  static const char* key_name(Key k);

 private:
  bool kstate_[(int)Key::Count] = {};
  bool kprev_[(int)Key::Count] = {};
  bool bstate_[(int)MouseButton::Count] = {};
  bool bprev_[(int)MouseButton::Count] = {};
  Vec2 mouse_{0, 0}, mouse_prev_{0, 0};
  float scroll_ = 0;
};

}  // namespace fc
