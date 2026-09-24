#!/usr/bin/env python3
"""ForgeCore preview server.

Streams the headless engine's framebuffer (shared memory RGB) to the browser
as JPEG, relays browser input to the engine, and relays engine audio/stats
events over SSE so the demo is audible via WebAudio.

Usage:  python3 tools/stream_server.py [--port 8000] [--width 1280] [--height 720]
"""

import argparse
import io
import json
import os
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    print("Pillow is required: pip install pillow", file=sys.stderr)
    raise

SHM_DIR = os.environ.get("FC_SHM_DIR", "/dev/shm")
FRAME = os.path.join(SHM_DIR, "fc_frame.rgb")
INPUT = os.path.join(SHM_DIR, "fc_input")
EVENTS = os.path.join(SHM_DIR, "fc_events")

WIDTH = 1280
HEIGHT = 720

_jpeg_lock = threading.Lock()
_jpeg_cache = None
_frame_seen = 0


def latest_jpeg():
    global _jpeg_cache, _frame_seen
    try:
        size = os.path.getsize(FRAME)
    except OSError:
        return None
    if size != WIDTH * HEIGHT * 3:
        return None
    # tmpfs: read is a copy; cheap enough at preview frame rates
    try:
        with open(FRAME, "rb") as f:
            raw = f.read()
    except OSError:
        return None
    if len(raw) != WIDTH * HEIGHT * 3:
        return None
    try:
        img = Image.frombytes("RGB", (WIDTH, HEIGHT), raw)
    except Exception:
        return None
    buf = io.BytesIO()
    img.save(buf, "JPEG", quality=72, optimize=False)
    _jpeg_cache = buf.getvalue()
    _frame_seen += 1
    return _jpeg_cache


PAGE = r"""<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>ForgeCore Engine — Tech Demo</title>
<style>
  html, body { margin:0; padding:0; background:#05060a; height:100%; overflow:hidden;
               font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace; }
  #wrap { position:fixed; inset:0; display:flex; align-items:center; justify-content:center; }
  canvas { image-rendering: pixelated; background:#05060a; box-shadow:0 0 60px #000a;
           cursor: crosshair; }
  #toast { position:fixed; top:10px; left:50%; transform:translateX(-50%);
           background:#111827ee; color:#e5e7eb; border:1px solid #334155;
           padding:8px 14px; border-radius:8px; font-size:12px; z-index:5;
           user-select:none; }
  #sound { position:fixed; bottom:10px; right:12px; z-index:6; background:#1f2937;
           color:#e5e7eb; border:1px solid #3b82f6; padding:6px 10px; border-radius:6px;
           font-size:11px; cursor:pointer; font-family:inherit; }
  #sound.on { border-color:#22c55e; }
</style>
</head>
<body>
<div id="wrap"><canvas id="cv" width="__W__" height="__H__"></canvas></div>
<div id="toast">FORGECORE ENGINE — TECH DEMO (live preview)
 &nbsp;|&nbsp; drag: orbit &nbsp; wheel: zoom &nbsp; WASD: move &nbsp; SPACE: jump
 &nbsp; click right side: ball &nbsp; F1/F2/F3: toggles &nbsp; R: reset</div>
<button id="sound">&#128263; enable sound</button>
<script>
const W = __W__, H = __H__;
const cv = document.getElementById('cv');
const cx = cv.getContext('2d');

function fit() {
  const s = Math.min(innerWidth / W, innerHeight / H);
  cv.style.width = (W * s) + 'px';
  cv.style.height = (H * s) + 'px';
}
addEventListener('resize', fit); fit();

// ---- frame polling ----------------------------------------------------------
const img = new Image();
let busy = false, stalled = 0;
img.onload = () => {
  busy = false;
  stalled = 0;
  cx.drawImage(img, 0, 0, W, H);
};
function poll() {
  if (!busy) {
    busy = true;
    if (stalled++ > 240) {  // engine not publishing frames yet
      cx.fillStyle = '#0a0f1e'; cx.fillRect(0, 0, W, H);
      cx.fillStyle = '#64748b'; cx.font = '16px monospace';
      cx.fillText('waiting for engine frames...', 24, H / 2);
      stalled = 0;
    }
    img.src = '/frame?r=' + Math.random();
  }
  requestAnimationFrame(poll);
}
poll();

// ---- input relay --------------------------------------------------------------
function post(obj) {
  try {
    fetch('/input', { method: 'POST', body: JSON.stringify(obj) + '\n',
                      headers: { 'content-type': 'application/x-ndjson' } });
  } catch (e) {}
}
function toLocal(ev) {
  const r = cv.getBoundingClientRect();
  return [ (ev.clientX - r.left) * W / r.width, (ev.clientY - r.top) * H / r.height ];
}
let lastMouse = null;
cv.addEventListener('mousemove', ev => {
  const [x, y] = toLocal(ev);
  lastMouse = [x, y];
  post({ t: 'mouse', x: +x.toFixed(1), y: +y.toFixed(1) });
});
cv.addEventListener('mousedown', ev => {
  const [x, y] = toLocal(ev);
  post({ t: 'mouse', x: +x.toFixed(1), y: +y.toFixed(1), b: 'l', d: 1 });
});
addEventListener('mouseup', ev => {
  if (ev.button === 0) post({ t: 'mouse', b: 'l', d: 0 });
});
cv.addEventListener('wheel', ev => {
  ev.preventDefault();
  post({ t: 'scroll', dy: ev.deltaY < 0 ? 1 : -1 });
}, { passive: false });

const KEYMAP = { ' ': 'space', 'ArrowUp': 'up', 'ArrowDown': 'down', 'ArrowLeft': 'left',
                 'ArrowRight': 'right', 'Enter': 'enter', 'Escape': 'escape', 'Tab': 'tab' };
function keyName(ev) {
  if (KEYMAP[ev.key]) return KEYMAP[ev.key];
  const k = ev.key.toLowerCase();
  if (/^[a-z0-9f]$/.test(k)) return k;
  if (k.startsWith('f') && /^[f][1-9]$|^[f]1[0-2]$/.test(k)) return k;
  if (k === '-' ) return 'minus';
  if (k === '=' ) return 'equal';
  if (k === ',' ) return 'comma';
  if (k === '.' ) return 'period';
  if (k === '/' ) return 'slash';
  return null;
}
addEventListener('keydown', ev => {
  if (ev.repeat) return;
  const k = keyName(ev);
  if (k) { post({ t: 'key', k, d: 1 }); if (k !== 'l') ev.preventDefault(); }
});
addEventListener('keyup', ev => {
  const k = keyName(ev);
  if (k) { post({ t: 'key', k, d: 0 }); ev.preventDefault(); }
});

// ---- events: WebAudio + stats ---------------------------------------------------
let ac = null;
const sndBtn = document.getElementById('sound');
sndBtn.onclick = () => {
  if (!ac) ac = new (window.AudioContext || window.webkitAudioContext)();
  ac.resume();
  sndBtn.classList.add('on');
  sndBtn.textContent = 'sound on';
};
function blip(ev) {
  if (!ac) return;
  const t0 = ac.currentTime;
  const o = ac.createOscillator();
  o.type = { sine: 'sine', square: 'square', saw: 'sawtooth', noise: 'square' }[ev.wave] || 'sine';
  o.frequency.setValueAtTime(Math.max(30, ev.f0), t0);
  o.frequency.exponentialRampToValueAtTime(Math.max(30, ev.f1), t0 + ev.dur);
  const g = ac.createGain();
  g.gain.setValueAtTime(0.0001, t0);
  g.gain.exponentialRampToValueAtTime(Math.max(0.0002, ev.vol * 0.4), t0 + 0.008);
  g.gain.exponentialRampToValueAtTime(0.0001, t0 + ev.dur);
  o.connect(g).connect(ac.destination);
  o.start(t0); o.stop(t0 + ev.dur + 0.02);
}
const es = new EventSource('/events');
es.onmessage = m => {
  try {
    const ev = JSON.parse(m.data);
    if (ev.t === 'audio') blip(ev);
  } catch (e) {}
};
</script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/":
            body = PAGE.replace("__W__", str(WIDTH)).replace("__H__", str(HEIGHT)).encode()
            self._send(200, body, "text/html; charset=utf-8")
        elif path == "/frame":
            data = latest_jpeg()
            if data:
                self._send(200, data, "image/jpeg")
            else:
                self._send(404, b"engine frame not ready", "text/plain")
        elif path == "/health":
            self._send(200, b"ok", "text/plain")
        elif path == "/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            offset = 0
            try:
                if os.path.exists(EVENTS):
                    offset = os.path.getsize(EVENTS)
            except OSError:
                pass
            while True:
                try:
                    if os.path.exists(EVENTS):
                        size = os.path.getsize(EVENTS)
                        if size > offset:
                            with open(EVENTS, "rb") as f:
                                f.seek(offset)
                                chunk = f.read(size - offset)
                                offset = size
                                for line in chunk.decode("utf-8", "replace").splitlines():
                                    if line.strip():
                                        self.wfile.write(f"data: {line}\n\n".encode())
                                        self.wfile.flush()
                        elif size < offset:
                            offset = size
                except (BrokenPipeError, ConnectionResetError):
                    return
                except OSError:
                    pass
                time.sleep(0.05)
        else:
            self._send(404, b"not found", "text/plain")

    def do_POST(self):
        path = self.path.split("?")[0]
        if path == "/input":
            n = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(n)
            try:
                with open(INPUT, "a") as f:
                    f.write(body.decode("utf-8", "replace"))
                self._send(200, b"ok", "text/plain")
            except OSError:
                self._send(500, b"input relay failed", "text/plain")
        else:
            self._send(404, b"not found", "text/plain")


def main():
    global WIDTH, HEIGHT
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--width", type=int, default=1280)
    ap.add_argument("--height", type=int, default=720)
    ap.add_argument("--shm-dir", default=SHM_DIR)
    args = ap.parse_args()
    WIDTH, HEIGHT = args.width, args.height

    srv = ThreadingHTTPServer(("0.0.0.0", args.port), Handler)
    srv.daemon_threads = True
    print(f"[preview] serving on 0.0.0.0:{args.port} (frame {WIDTH}x{HEIGHT}, shm {args.shm_dir})",
          flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
