#!/usr/bin/env python3
"""Serves the Godot web export for local play and testing. Private: it binds to 127.0.0.1 only and refuses any other bind
address, so the build can never be reached from the network or published by accident.

Sends the cross-origin isolation headers (COOP same-origin, COEP require-corp) that a threaded or SharedArrayBuffer
build needs, and the right MIME type for .wasm / .pck so streaming instantiation works.

Caching (M1 webparity): content-hashed packs (packs/<stem>.<12 hex>.pck, from hash_web_packs.py) are sent with
"Cache-Control: public, max-age=31536000, immutable"; everything else with "no-cache", so the browser revalidates and gets
a 304 with no body when the file is unchanged. A return visit therefore downloads nothing. Where a .gz sidecar exists and
the browser accepts gzip, the sidecar is sent with Content-Encoding: gzip. Not under packs/: Godot's web HTTPRequest fails
(result 8, request failed) on a content-encoded body saved with download_file, so packs go out as they are.

Host simulation (F1): --host-sim serves like a plain static host under a sub-path, so the build can be proven before upload:
no COOP/COEP headers (the build needs none), no .gz sidecars, no cache headers except what the host adds, gzip on the fly for
wasm/js/json/html (what a host with "gzip on" does), everything under /botf/ (--mount) and a 404 outside it. --throttle-kbps N
limits the send rate per connection (a coarse remote-player link; the browser-side CDP throttle is the primary tool).

  python serve_web.py [--dir remaster/godot/build/web] [--port 8060] [--host-sim [--mount /botf/]]
Prints the URL, then serves until Ctrl+C. web_capture.py starts it on a free port (--port 0) for the browser capture.
"""
import argparse
import email.utils
import functools
import http.server
import os
import re
import sys

HASHED = re.compile(r"\.[0-9a-f]{12}\.pck$")


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".wasm": "application/wasm", ".pck": "application/octet-stream", ".js": "text/javascript",
                      ".mjs": "text/javascript", ".json": "application/json"}

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        path = self.path.split("?", 1)[0]
        self.send_header("Cache-Control", "public, max-age=31536000, immutable" if HASHED.search(path) else "no-cache")
        super().end_headers()

    def send_head(self):
        # A .gz sidecar for a browser that accepts gzip: same Last-Modified / 304 handling as the plain file.
        path = self.translate_path(self.path)
        gz = path + ".gz"
        if "/packs/" in self.path.split("?")[0] or "gzip" not in self.headers.get("Accept-Encoding", "") or not os.path.isfile(path) or not os.path.isfile(gz):
            return super().send_head()
        st = os.stat(path)
        ims = self.headers.get("If-Modified-Since")
        if ims:
            try:
                if int(st.st_mtime) <= email.utils.parsedate_to_datetime(ims).timestamp():
                    self.send_response(304)
                    self.end_headers()
                    return None
            except (TypeError, ValueError, IndexError, OverflowError):
                pass
        f = open(gz, "rb")
        self.send_response(200)
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Content-Encoding", "gzip")
        self.send_header("Vary", "Accept-Encoding")
        self.send_header("Content-Length", str(os.fstat(f.fileno()).st_size))
        self.send_header("Last-Modified", self.date_time_string(int(st.st_mtime)))
        self.end_headers()
        return f

    def log_message(self, fmt, *args):
        if os.environ.get("SERVE_QUIET") != "1":
            super().log_message(fmt, *args)


class HostSimHandler(http.server.SimpleHTTPRequestHandler):
    """A plain static host: sub-path mount, MIME types, on-the-fly gzip for text-like types, no isolation headers."""
    extensions_map = Handler.extensions_map
    mount = "/botf/"
    throttle_kbps = 0
    GZIP_TYPES = (".wasm", ".js", ".mjs", ".json", ".html", ".css", ".svg")
    gzip_all = False   # --gzip-all: also compress packs/ (a host or CDN that compresses everything)

    def translate_path(self, path):
        path = path.split("?", 1)[0].split("#", 1)[0]
        if not path.startswith(self.mount):
            return os.path.join(self.directory, "__outside_mount__")
        return super().translate_path("/" + path[len(self.mount):])

    def send_head(self):
        path = self.translate_path(self.path)
        if not self.path.split("?")[0].startswith(self.mount):
            self.send_error(404, "outside the mount")
            return None
        if os.path.isdir(path):
            if not self.path.split("?")[0].endswith("/"):
                self.send_response(301)
                self.send_header("Location", self.path.split("?")[0] + "/")
                self.end_headers()
                return None
            path = os.path.join(path, "index.html")
        if not os.path.isfile(path):
            self.send_error(404)
            return None
        ext = os.path.splitext(path)[1].lower()
        data = open(path, "rb").read()
        self.send_response(200)
        self.send_header("Content-Type", self.guess_type(path))
        in_packs = "/packs/" in self.path.split("?")[0]
        if (ext in self.GZIP_TYPES or (self.gzip_all and ext == ".pck")) and (self.gzip_all or not in_packs) and "gzip" in self.headers.get("Accept-Encoding", ""):
            import gzip
            data = gzip.compress(data, 6)
            self.send_header("Content-Encoding", "gzip")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Last-Modified", self.date_time_string(int(os.stat(path).st_mtime)))
        self.end_headers()
        import io
        return io.BytesIO(data)

    def copyfile(self, source, outputfile):
        if self.throttle_kbps <= 0:
            return super().copyfile(source, outputfile)
        import time
        chunk = 16384
        while True:
            b = source.read(chunk)
            if not b:
                break
            outputfile.write(b)
            time.sleep(len(b) / (self.throttle_kbps * 1024.0))

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
    ap.add_argument("--host-sim", action="store_true", help="behave like a plain static host under --mount (see above)")
    ap.add_argument("--mount", default="/botf/")
    ap.add_argument("--gzip-all", action="store_true", help="with --host-sim: gzip packs/ too (worst-case host)")
    ap.add_argument("--throttle-kbps", type=int, default=0)
    a = ap.parse_args()
    if a.bind not in ("127.0.0.1", "localhost"):
        sys.exit("refused: this server binds to localhost only (the build is private and never published)")
    if not os.path.isfile(os.path.join(a.dir, "index.html")):
        sys.exit("no index.html in %s: export the Web preset first (verify.ps1 -Target web)" % a.dir)
    if a.host_sim:
        HostSimHandler.mount = "/" + a.mount.strip("/") + "/"
        HostSimHandler.throttle_kbps = a.throttle_kbps
        HostSimHandler.gzip_all = a.gzip_all
        server = http.server.ThreadingHTTPServer(("127.0.0.1", a.port), functools.partial(HostSimHandler, directory=a.dir))
        print("serving %s at http://127.0.0.1:%d%s (localhost only, host simulation: no COOP/COEP)" % (a.dir, server.server_address[1], HostSimHandler.mount), flush=True)
    else:
        server = http.server.ThreadingHTTPServer(("127.0.0.1", a.port), functools.partial(Handler, directory=a.dir))
        print("serving %s at http://127.0.0.1:%d/ (localhost only, COOP/COEP on)" % (a.dir, server.server_address[1]), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
