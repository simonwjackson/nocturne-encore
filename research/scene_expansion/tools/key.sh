#!/usr/bin/env nix-shell
#! nix-shell -i bash -p xdotool
# usage: key.sh <key> [hold_seconds] [repeat] [gap_seconds]
# Presses a key in the focused NocturneRecomp window on Xvfb :77.
set -euo pipefail
export DISPLAY=:77
KEY=$1; HOLD=${2:-0.15}; REPEAT=${3:-1}; GAP=${4:-0.4}
WIN=$(xdotool search --name NocturneRecomp | head -1)
xdotool windowfocus --sync "$WIN" 2>/dev/null || true
for _ in $(seq "$REPEAT"); do
  xdotool keydown "$KEY"
  sleep "$HOLD"
  xdotool keyup "$KEY"
  sleep "$GAP"
done
