#pragma once
// Minimal, ABI-compatible subset of the X11 client API (X11R6, LP64).
//
// The desktop backend loads libX11 at runtime via dlopen, so the engine
// builds without X11 development headers. The declarations below mirror the
// official X11R6 layouts (X.h / Xlib.h); they are exercised on real X11
// desktops, where layout mismatches would be caught immediately at runtime.

#include <cstdint>

namespace fc::x11 {

using XID = unsigned long;
using Bool = unsigned char;
using Time = unsigned long;
using Atom = XID;
using VisualID = XID;
using Window = XID;
using Cursor = XID;
using Status = int;
constexpr Bool False = 0;
constexpr Bool True = 1;

struct _XDisplay;
using Display = struct _XDisplay;
struct _XScreen;
using Screen = struct _XScreen;

// XVisualInfo (Xutil.h) — we rely only on the stable early fields.
struct XVisualInfo {
  long valuemask;
  Display* display;
  int screen;
  VisualID visual;
  int depth;
  int cls;
  unsigned long white_pixel;
  unsigned long black_pixel;
  int bits_per_pixel;
  int colormap_size;
  int bytes_per_line;
  int width;
  int height;
};

// Event types
constexpr int KeyPress = 2;
constexpr int KeyRelease = 3;
constexpr int ButtonPress = 4;
constexpr int ButtonRelease = 5;
constexpr int MotionNotify = 6;
constexpr int Expose = 12;
constexpr int ConfigureNotify = 22;
constexpr int ClientMessage = 33;

// Event masks
constexpr long KeyPressMask = 1L << 0;
constexpr long ExposureMask = 1L << 1;
constexpr long ButtonPressMask = 1L << 2;
constexpr long ButtonReleaseMask = 1L << 3;
constexpr long PointerMotionMask = 1L << 4;
constexpr long LeaveWindowMask = 1L << 6;
constexpr long StructureNotifyMask = 1L << 12;

// Common event structs (X11R6 layouts, LP64).
struct XAnyEvent {
  int type;
  unsigned long serial;
  Bool send_event;
  Display* display;
  Window window;
};

struct XKeyEvent {
  int type;
  unsigned long serial;
  Bool send_event;
  Display* display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
  unsigned int state;
  int keycode;
  Bool same_screen;
};

struct XButtonEvent {
  int type;
  unsigned long serial;
  Bool send_event;
  Display* display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
  unsigned int state;
  int button;
  Bool same_screen;
};

struct XMotionEvent {
  int type;
  unsigned long serial;
  Bool send_event;
  Display* display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
  unsigned int state;
  int is_hint;
  int buttons;
};

struct XConfigureEvent {
  int type;
  unsigned long serial;
  Bool send_event;
  Display* display;
  Window event;
  Window window;
  Window parent;
  int x, y;
  int width, height;
  int border_width;
  Bool above;
  Bool override_redirect;
};

struct XClientMessageEvent {
  int type;
  unsigned long serial;
  Bool send_event;
  Display* display;
  Window window;
  Atom message_type;
  int format;
  union {
    char b[20];
    short s[10];
    long l[5];
  } data;
};

// XEvent union: canonical X11R6 definition (int type + long pad[24]).
union XEvent {
  int type;
  long pad[24];
  XAnyEvent xany;
  XKeyEvent xkey;
  XButtonEvent xbutton;
  XMotionEvent xmotion;
  XConfigureEvent xconfigure;
  XClientMessageEvent xclient;
};

// --- GLX app-side types -----------------------------------------------------
using GLXContext = void*;
using GLXDrawable = unsigned long;
using GLXFBConfig = struct {
  int num_attribs;
  int attrib_list[32];
};

// GLX attrib tokens
constexpr int GLX_RENDER_TYPE = 0x800C;
constexpr int GLX_RGBA_BIT = 0x00001;
constexpr int GLX_DRAWABLE_TYPE = 0x8010;
constexpr int GLX_WINDOW_BIT = 0x00001;
constexpr int GLX_DOUBLEBUFFER = 0x2011;
constexpr int GLX_X_RENDERABLE = 0x8011;
constexpr int GLX_DEPTH_SIZE = 0x2022;
constexpr int GLX_RED_SIZE = 0x2021;

}  // namespace fc::x11
