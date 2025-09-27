#!/usr/bin/env bash
set -euo pipefail
PORT=${1:-8080}
URL="http://localhost:${PORT}/stocks"
echo "== GET ${URL}" >&2
status=$(curl -s -o /tmp/resp.$$ -w '%{http_code}' "$URL" || echo '000')
echo "HTTP ${status}" >&2
if [[ $status == 200 ]]; then
  jq '.' </tmp/resp.$$ || cat /tmp/resp.$$
else
  echo "Raw body:" >&2
  cat /tmp/resp.$$ >&2
fi
rm -f /tmp/resp.$$