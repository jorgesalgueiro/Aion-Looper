#!/usr/bin/env bash
# Output stays muted. Physical input is opt-in; captured audio is discarded.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
mkdir -p verification
mode=--self-test
name=linux-desktop-ui
case "${1:-}" in
  "") ;;
  --physical-input) mode=--input-smoke-test; name=linux-physical-input ;;
  *) echo 'Usage: bash scripts/test-desktop-linux.sh [--physical-input]' >&2; exit 2 ;;
esac
report="$PWD/verification/$name.json"
rm -f -- "$report"
status=0
timeout 65s ./dist/Aion-Looper-Linux/Aion-Looper "$mode" "$report" \
  >"verification/$name-stdout.log" 2>"verification/$name-stderr.log" || status=$?
if [[ -f "$report" ]]; then
  cat "$report"
  python3 - "$report" <<'PY'
import json, sys
with open(sys.argv[1]) as source:
    report = json.load(source)
if report.get("ok") is not True:
    sys.exit(1)
PY
else
  echo 'Desktop exited without a report; inspect verification/linux-desktop-stderr.log.' >&2
  exit 1
fi
exit "$status"
