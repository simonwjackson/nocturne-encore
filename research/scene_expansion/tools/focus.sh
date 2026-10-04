#!/usr/bin/env nix-shell
#! nix-shell -i bash -p xdotool
# Give the NocturneRecomp window on Xvfb :77 a real FocusIn event.
set -euo pipefail
export DISPLAY=:77
WIN=$(xdotool search --name NocturneRecomp | tail -1)
xdotool windowfocus --sync "$(xdotool getactivewindow 2>/dev/null || echo "$WIN")" 2>/dev/null || true
xdotool windowunmap --sync "$WIN"
xdotool windowmap --sync "$WIN"
xdotool windowfocus --sync "$WIN"
xdotool mousemove --window "$WIN" 640 360
echo "focused $WIN"
