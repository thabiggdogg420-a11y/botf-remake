#!/usr/bin/env python3
"""Serves the Godot web export for local play and testing. Private: it binds to 127.0.0.1 only and refuses any other bind
address, so the build can never be reached from the network or published by accident.

Sends the cross-origin isolation headers (COOP same-origin, COEP require-corp) that a threaded or SharedArrayBuffer
build needs, and the right MIME type for .wasm / .pck so streaming instantiation works.

  python serve_web.py [--dir remaster/godot/build/web] [--port 8060]
Prints the URL, then serves until Ctrl+C. web_capture.py starts it on a free port (--port 0) for the browser capture.
"""
import argparse
import functools
import http.server
import os
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".wasm": "application/wasm", ".pck": "application/octet-stream", ".js": "text/javascript",
                      ".mjs": "text/javascript", ".json": "application/json"}

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, fmt, *args):
        if os.environ.get("SERVE_QUIET") != "1":
            super().log_message(fmt, *args)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default_dir = os.path.normpath(os.path.join(here, "..", "..", "godot", "build", "web"))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", default=default_dir)
    ap.add_argument("--port", type=int, default=8060, help="0 picks a free port")
    ap.add_argument("--bind", default="127.0.0.1", help="only 127.0.0.1 or localhost is accepted")
    a = ap.parse_args()
    if a.bind not in ("127.0.0.1", "localhost"):
        sys.exit("refused: this server binds to localhost only (the build is private and never published)")
    if not os.path.isfile(os.path.join(a.dir, "index.html")):
        sys.exit("no index.html in %s: export the Web preset first (verify.ps1 -Target web)" % a.dir)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", a.port), functools.partial(Handler, directory=a.dir))
    print("serving %s at http://127.0.0.1:%d/ (localhost only, COOP/COEP on)" % (a.dir, server.server_address[1]), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
