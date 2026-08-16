#!/usr/bin/env python3
"""Serve graphify-out with a live agent-activity overlay.

The stock Graphify export is a self-contained file.  This companion server
leaves that file untouched on disk, injects the local overlay at request time,
and serves the small activity feed with no-cache headers so the browser can
poll it while Codex or another agent works.
"""

from __future__ import annotations

import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[1]
GRAPH_HTML = ROOT / "graphify-out" / "graph.html"
ACTIVITY_JSON = ROOT / "graphify-out" / ".agent-activity.json"
OVERLAY_JS = ROOT / "tools" / "graphify-live-overlay.js"


def overlay_html(document: str) -> bytes:
    """Inject the overlay and expose the vis Network instance to it."""
    document = document.replace(
        "const network = new vis.Network(",
        "const network = window.__graphifyNetwork = new vis.Network(",
        1,
    )
    overlay = OVERLAY_JS.read_text(encoding="utf-8")
    marker = "<!-- graphify-live-agent-overlay -->"
    if marker not in document:
        document = document.replace(
            "</body>",
            f"{marker}<script>{overlay}</script></body>",
            1,
        )
    return document.encode("utf-8")


class Handler(SimpleHTTPRequestHandler):
    server_version = "SudekiMPGraphifyLive/1.0"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def _send_bytes(self, payload: bytes, content_type: str, cache: str) -> None:
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", cache)
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self) -> None:  # noqa: N802 - stdlib handler API
        path = urlsplit(self.path).path
        if path == "/graphify-out/graph.html":
            try:
                payload = overlay_html(GRAPH_HTML.read_text(encoding="utf-8"))
            except OSError as error:
                self.send_error(503, f"graph.html unavailable: {error}")
                return
            self._send_bytes(payload, "text/html; charset=utf-8", "no-store")
            return
        if path == "/graphify-out/.agent-activity.json":
            try:
                payload = ACTIVITY_JSON.read_bytes()
            except FileNotFoundError:
                payload = b'{"version":1,"updated_at":null,"agents":[]}\n'
            except OSError as error:
                self.send_error(503, f"activity feed unavailable: {error}")
                return
            self._send_bytes(payload, "application/json; charset=utf-8", "no-store")
            return
        super().do_GET()

    def log_message(self, format: str, *args) -> None:
        print(f"[graphify-live] {self.address_string()} - {format % args}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"Graphify live view: http://{args.host}:{args.port}/graphify-out/graph.html")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[graphify-live] stopped")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
