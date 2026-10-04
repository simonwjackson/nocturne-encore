#!/usr/bin/env bash
# Start a private PulseAudio null sink, then NocturneRecomp on Xvfb :77.
# usage (inside nix-shell shell.nix): session.sh <rundir> <seconds> [game args...]
set -euo pipefail
RUN=$1; shift
mkdir -p "$RUN"
SOCK="$RUN/pulse.sock"
rm -f "$SOCK"
pulseaudio --daemonize=no --exit-idle-time=-1 --use-pid-file=no --disable-shm -n \
  "--load=module-native-protocol-unix socket=$SOCK auth-anonymous=1" \
  "--load=module-null-sink sink_name=nocturne_dev" > "$RUN/pulse.log" 2>&1 &
PULSE=$!
trap 'kill $PULSE 2>/dev/null || true' EXIT
for _ in $(seq 100); do [ -S "$SOCK" ] && break; sleep 0.1; done
export VK_ICD_FILENAMES=/run/opengl-driver/share/vulkan/icd.d/lvp_icd.x86_64.json DISPLAY=:77 SDL_AUDIODRIVER=pulseaudio SDL_VIDEODRIVER=x11 PULSE_SERVER="unix:$SOCK"
"$(dirname "$(realpath "$0")")/run.sh" "$RUN" "$@"
