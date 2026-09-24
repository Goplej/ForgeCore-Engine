#include "glx_window.hpp"

#ifdef FC_PLATFORM_UNIX
#include <dlfcn.h>
#endif
#include <cstring>

namespace fc {

namespace {
using XOpenDisplayFn = x11::Display* (*)(const char*);
using XCloseDisplayFn = int (*)(x11::Display*);
using XDefaultScreenFn = int (*)(x11::Display*);
using XDefaultRootWindowFn = x11::Window (*)(x11::Display*);
using XCreateWindowFn = x11::Window (*)(x11::Display*, x11::Window, int, int, int, int, int, int,
                                         int, void*, unsigned long, const void*);
using XDestroyWindowFn = void (*)(x11::Display*, x11::Window);
using XMapWindowFn = int (*)(x11::Display*, x11::Window);
using XStoreNameFn = int (*)(x11::Display*, x11::Window, const char*);
using XSelectInputFn = long (*)(x11::Display*, x11::Window, long);
using XPendingFn = int (*)(x11::Display*);
using XNextEventFn = void (*)(x11::Display*, x11::XEvent*);
using XInternAtomFn = x11::Atom (*)(x11::Display*, const char*, x11::Bool);
using XSetWMProtocolsFn = int (*)(x11::Display*, x11::Window, x11::Atom*, int);
using XLookupKeysymFn = unsigned long (*)(const x11::XKeyEvent*);
using XFreeVisualInfoFn = void (*)(x11::XVisualInfo*);
using XFlushFn = int (*)(x11::Display*);

using glXChooseVisualFn = x11::XVisualInfo* (*)(x11::Display*, int, const int*, int*);
using glXCreateContextFn = x11::GLXContext (*)(x11::Display*, const x11::XVisualInfo*, x11::GLXContext);
using glXMakeCurrentFn = int (*)(x11::Display*, x11::GLXDrawable, x11::GLXContext);
using glXSwapBuffersFn = int (*)(x11::Display*, x11::GLXDrawable);
using glXDestroyContextFn = void (*)(x11::Display*, x11::GLXContext);

struct XFns {
  XOpenDisplayFn XOpenDisplay = nullptr;
  XCloseDisplayFn XCloseDisplay = nullptr;
  XDefaultScreenFn XDefaultScreen = nullptr;
  XDefaultRootWindowFn XDefaultRootWindow = nullptr;
  XCreateWindowFn XCreateWindow = nullptr;
  XDestroyWindowFn XDestroyWindow = nullptr;
  XMapWindowFn XMapWindow = nullptr;
  XStoreNameFn XStoreName = nullptr;
  XSelectInputFn XSelectInput = nullptr;
  XPendingFn XPending = nullptr;
  XNextEventFn XNextEvent = nullptr;
  XInternAtomFn XInternAtom = nullptr;
  XSetWMProtocolsFn XSetWMProtocols = nullptr;
  XLookupKeysymFn XLookupKeysym = nullptr;
  XFreeVisualInfoFn XFreeVisualInfo = nullptr;
  XFlushFn XFlush = nullptr;
};

void* xlib_handle_ = nullptr;
XFns xfns_;

bool load_xlib(XFns& f) {
  void* h = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
  if (!h) h = dlopen("libX11.so", RTLD_NOW | RTLD_GLOBAL);
  if (!h) return false;
  xlib_handle_ = h;
  f.XOpenDisplay = (XOpenDisplayFn)dlsym(h, "XOpenDisplay");
  f.XCloseDisplay = (XCloseDisplayFn)dlsym(h, "XCloseDisplay");
  f.XDefaultScreen = (XDefaultScreenFn)dlsym(h, "XDefaultScreen");
  f.XDefaultRootWindow = (XDefaultRootWindowFn)dlsym(h, "XDefaultRootWindow");
  f.XCreateWindow = (XCreateWindowFn)dlsym(h, "XCreateWindow");
  f.XDestroyWindow = (XDestroyWindowFn)dlsym(h, "XDestroyWindow");
  f.XMapWindow = (XMapWindowFn)dlsym(h, "XMapWindow");
  f.XStoreName = (XStoreNameFn)dlsym(h, "XStoreName");
  f.XSelectInput = (XSelectInputFn)dlsym(h, "XSelectInput");
  f.XPending = (XPendingFn)dlsym(h, "XPending");
  f.XNextEvent = (XNextEventFn)dlsym(h, "XNextEvent");
  f.XInternAtom = (XInternAtomFn)dlsym(h, "XInternAtom");
  f.XSetWMProtocols = (XSetWMProtocolsFn)dlsym(h, "XSetWMProtocols");
  f.XLookupKeysym = (XLookupKeysymFn)dlsym(h, "XLookupKeysym");
  f.XFreeVisualInfo = (XFreeVisualInfoFn)dlsym(h, "XFreeVisualInfo");
  f.XFlush = (XFlushFn)dlsym(h, "XFlush");
  return f.XOpenDisplay && f.XCreateWindow && f.XNextEvent && f.XLookupKeysym;
}
}  // namespace

bool GLXWindow::open() {
  if (!xfns_.XOpenDisplay && !load_xlib(xfns_)) {
    error_ = "libX11 not available";
    return false;
  }
  // glXChooseVisual may live in libGLX (libglvnd) rather than libGL.
  void* p_choose = nullptr;
#ifdef FC_PLATFORM_UNIX
  void* hx = dlopen("libGLX.so.1", RTLD_NOW | RTLD_GLOBAL);
  if (hx) p_choose = dlsym(hx, "glXChooseVisual");
  if (!p_choose) {
    void* hg = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (hg) p_choose = dlsym(hg, "glXChooseVisual");
  }
#endif
  if (!p_choose) {
    error_ = "glXChooseVisual not available";
    return false;
  }
  auto choose = (glXChooseVisualFn)p_choose;

  dpy_ = xfns_.XOpenDisplay(nullptr);
  if (!dpy_) {
    error_ = "cannot open X display (set DISPLAY on a desktop)";
    return false;
  }
  const int attrib[] = {
      x11::GLX_RENDER_TYPE, x11::GLX_RGBA_BIT, x11::GLX_DRAWABLE_TYPE, x11::GLX_WINDOW_BIT,
      x11::GLX_DOUBLEBUFFER, x11::GLX_X_RENDERABLE, x11::GLX_DEPTH_SIZE, 24, x11::GLX_RED_SIZE,
      8, 0};
  int n = 0;
  x11::XVisualInfo* vi = choose(dpy_, xfns_.XDefaultScreen(dpy_), attrib, &n);
  if (!vi || n < 1) {
    error_ = "no suitable GLX visual";
    xfns_.XCloseDisplay(dpy_);
    dpy_ = nullptr;
    return false;
  }
  vis_ = *vi;
  if (xfns_.XFreeVisualInfo) xfns_.XFreeVisualInfo(vi);

  root_ = xfns_.XDefaultRootWindow(dpy_);
  w_ = cfg_.width;
  h_ = cfg_.height;
  constexpr int InputOutput = 1;
  win_ = xfns_.XCreateWindow(dpy_, root_, 0, 0, w_, h_, 0, vis_.depth, InputOutput,
                             (void*)vis_.visual, 0, nullptr);
  if (!win_) {
    error_ = "XCreateWindow failed";
    return false;
  }
  xfns_.XStoreName(dpy_, win_, cfg_.title.c_str());
  constexpr long kMasks = x11::KeyPressMask | x11::ButtonPressMask | x11::ButtonReleaseMask |
                          x11::PointerMotionMask | x11::StructureNotifyMask | x11::Expose;
  xfns_.XSelectInput(dpy_, win_, kMasks);
  x11::Atom wm_delete = xfns_.XInternAtom(dpy_, "WM_DELETE_WINDOW", x11::False);
  xfns_.XSetWMProtocols(dpy_, win_, &wm_delete, 1);

  auto glx_create = (glXCreateContextFn)gl_.glXCreateContext_p;
  auto glx_make = (glXMakeCurrentFn)gl_.glXMakeCurrent_p;
  ctx_ = glx_create ? glx_create(dpy_, &vis_, nullptr) : nullptr;
  if (!ctx_) {
    error_ = "glXCreateContext failed";
    return false;
  }
  if (glx_make) glx_make(dpy_, win_, ctx_);
  xfns_.XMapWindow(dpy_, win_);
  return true;
}

void GLXWindow::close() {
  if (ctx_ && gl_.glXDestroyContext_p)
    ((glXDestroyContextFn)gl_.glXDestroyContext_p)(dpy_, ctx_);
  ctx_ = nullptr;
  if (dpy_) {
    if (win_) xfns_.XDestroyWindow(dpy_, win_);
    win_ = 0;
    xfns_.XCloseDisplay(dpy_);
    dpy_ = nullptr;
  }
}

void GLXWindow::poll_events(Input& input) {
  if (!dpy_) return;
  if (!xfns_.XPending || xfns_.XPending(dpy_) == 0) {
    // No events: keep pending resize applied.
    if (pending_width_ > 0) {
      w_ = pending_width_;
      h_ = pending_height_;
      pending_width_ = 0;
      pending_height_ = 0;
    }
    return;
  }
  for (;;) {
    x11::XEvent ev{};
    xfns_.XNextEvent(dpy_, &ev);
    handle_event(ev, input);
    if (!xfns_.XPending(dpy_)) break;
  }
  if (pending_width_ > 0) {
    w_ = pending_width_;
    h_ = pending_height_;
    pending_width_ = 0;
    pending_height_ = 0;
  }
}

namespace {
// XK_* constants from X11/keysymdef.h (stable, documented).
Key keysym_to_key(unsigned long ks) {
  if (ks >= 'a' && ks <= 'z') return (Key)((int)Key::A + (ks - 'a'));
  if (ks >= '0' && ks <= '9') return (Key)((int)Key::Num0 + (ks - '0'));
  switch (ks) {
    case 0x20: return Key::Space;
    case 0xFF0D: return Key::Enter;
    case 0xFF1B: return Key::Escape;
    case 0xFF09: return Key::Tab;
    case 0xFF08: return Key::Backspace;
    case 0xFF50: return Key::Home;
    case 0xFF57: return Key::End;
    case 0xFF55: return Key::PageUp;
    case 0xFF56: return Key::PageDown;
    case 0xFF63: return Key::Insert;
    case 0xFF97: return Key::Delete;
    case 0xFF51: return Key::Left;
    case 0xFF52: return Key::Up;
    case 0xFF53: return Key::Right;
    case 0xFF54: return Key::Down;
    case 0xFFBE: return Key::F1;  case 0xFFBF: return Key::F2;  case 0xFFC0: return Key::F3;
    case 0xFFC1: return Key::F4;  case 0xFFC2: return Key::F5;  case 0xFFC3: return Key::F6;
    case 0xFFC4: return Key::F7;  case 0xFFC5: return Key::F8;  case 0xFFC6: return Key::F9;
    case 0xFFC7: return Key::F10; case 0xFFC8: return Key::F11; case 0xFFC9: return Key::F12;
    case 0xFFE1: return Key::LShift; case 0xFFE2: return Key::RShift;
    case 0xFFE3: return Key::LCtrl; case 0xFFE4: return Key::RCtrl;
    case 0xFFE9: return Key::LAlt;  case 0xFFEA: return Key::RAlt;
    default: return Key::None;
  }
}
}  // namespace

void GLXWindow::handle_event(x11::XEvent& ev, Input& input) {
  switch (ev.type) {
    case x11::KeyPress:
    case x11::KeyRelease: {
      bool down = ev.type == x11::KeyPress;
      if (xfns_.XLookupKeysym) {
        Key k = keysym_to_key(xfns_.XLookupKeysym(&ev.xkey));
        if (k != Key::None) input.press_key(k, down);
      }
      break;
    }
    case x11::ButtonPress:
    case x11::ButtonRelease: {
      bool down = ev.type == x11::ButtonPress;
      MouseButton b = ev.xbutton.button == 1 ? MouseButton::Left
                    : ev.xbutton.button == 2 ? MouseButton::Middle
                                             : MouseButton::Right;
      input.set_mouse({(float)ev.xbutton.x, (float)ev.xbutton.y});
      input.press_button(b, down);
      break;
    }
    case x11::MotionNotify:
      input.set_mouse({(float)ev.xmotion.x, (float)ev.xmotion.y});
      break;
    case x11::ConfigureNotify:
      pending_width_ = ev.xconfigure.width;
      pending_height_ = ev.xconfigure.height;
      break;
    case x11::ClientMessage:
      close_requested_ = true;
      break;
    default:
      break;
  }
}

void GLXWindow::present() {
  if (dpy_ && gl_.glXSwapBuffers_p)
    ((glXSwapBuffersFn)gl_.glXSwapBuffers_p)(dpy_, win_);
}

}  // namespace fc
