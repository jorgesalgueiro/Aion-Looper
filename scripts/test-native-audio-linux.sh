#!/usr/bin/env bash
# Synthetic native duplex only: no audio device is opened and no take is saved.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
mkdir -p verification
report="$PWD/verification/native-audio-linux.json"
rm -f -- "$report"
status=0
timeout 65s ./dist/Aion-Looper-Linux/Aion-Looper --native-self-test "$report" \
  >verification/native-audio-linux-stdout.log 2>verification/native-audio-linux-stderr.log || status=$?
if [[ ! -f "$report" ]]; then
  echo 'Native desktop test exited without a report.' >&2
  exit 1
fi
cat "$report"
python3 - "$report" <<'PY'
import json, sys
with open(sys.argv[1]) as source:
    report = json.load(source)
assert report.get("ok") is True, report.get("error")
assert report["status"]["web_audio"] is False
assert report["status"]["transport"] == "native-duplex"
PY
exit "$status"
