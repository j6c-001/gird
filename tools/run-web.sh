#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8000}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/cmake-build-web-release"
HTML="gird.html"

cd "$BUILD_DIR"

python3 - <<'PY' "$PORT" >/dev/null 2>&1 &
import http.server, socketserver, sys

port = int(sys.argv[1])

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, max-age=0")
        self.send_header("Pragma", "no-cache")
        super().end_headers()

with socketserver.TCPServer(("", port), Handler) as httpd:
    httpd.serve_forever()
PY
SERVER_PID=$!
SERVER_PID=$!

trap 'kill $SERVER_PID >/dev/null 2>&1 || true' EXIT

URL="http://localhost:${PORT}/${HTML}"

# Ubuntu: open default browser
if command -v xdg-open >/dev/null; then
  xdg-open "$URL" >/dev/null 2>&1 || true
fi

wait $SERVER_PID