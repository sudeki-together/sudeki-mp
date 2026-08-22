#!/usr/bin/env python3
"""Serve the private SudekiMP handoff over the NetBird interface."""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

DEFAULT_BIND = "100.95.93.91"
DEFAULT_PORT = 18731
DEFAULT_PEER = "100.95.174.52"


def make_handler(instructions: Path, peer: str, local_bind: str):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:  # noqa: N802 - stdlib protocol name
            if self.client_address[0] not in {peer, local_bind, "127.0.0.1", "::1"}:
                self.send_error(403, "NetBird peer not allowed")
                return
            if self.path == "/healthz":
                body = b"ok\n"
                self.send_response(200)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
            elif self.path in {"/", "/SYNC_INSTRUCTIONS.txt"}:
                body = instructions.read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
            else:
                self.send_error(404, "not found")
                return
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt: str, *args: object) -> None:
            print(f"sync-http {self.address_string()} - {fmt % args}")

    return Handler


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default=DEFAULT_BIND)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--peer", default=DEFAULT_PEER)
    parser.add_argument(
        "--instructions",
        type=Path,
        default=Path(__file__).with_name("SYNC_INSTRUCTIONS.txt"),
    )
    args = parser.parse_args()
    if not args.instructions.is_file():
        parser.error(f"instructions file does not exist: {args.instructions}")
    server = ThreadingHTTPServer((args.bind, args.port), make_handler(args.instructions, args.peer, args.bind))
    server.daemon_threads = True
    print(f"serving {args.instructions} on http://{args.bind}:{args.port}/")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
