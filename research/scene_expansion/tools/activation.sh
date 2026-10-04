#!/usr/bin/env bash
# Activation A/B: from one frozen game-state snapshot, replay the same
# frame-indexed input script twice with expansion off (control) and once
# with expansion on. Each run writes a per-frame entity trace. The snapshot
# load, the expansion switch and the script start are one command batch, so
# they take effect in the same frame.
# usage: activation.sh <label> <script>   (SNAP="<start> <end>" optional)
set -euo pipefail
L=$1; S=$(realpath "$2")
D=/tmp/nocturne-expand
CTL=${SCENE_PROBE_DIR:-$D/ctl}
OUT=$D/act/$L
mkdir -p "$OUT"
send() { printf '%b\n' "$1" > "$CTL/cmd.tmp"; mv "$CTL/cmd.tmp" "$CTL/cmd"; sleep 0.5; }
run() {  # <name> <expand on|off>
  local before
  before=$(grep -c 'script done' "$CTL/log" || true)
  send "freeze on"
  send "snapshot load\nexpand $2\nscript $S $OUT/$1.trace"
  for _ in $(seq 600); do
    [ "$(grep -c 'script done' "$CTL/log" || true)" -gt "$before" ] && break
    sleep 0.5
  done
  sleep 1
  import -display :77 -window root "$OUT/$1-end.png"
  send "vram $OUT/$1-end.raw"
  send "dump 8314EEC0 64512 $OUT/$1-entities.bin"
}
send "freeze on"
send "expand off"
sleep 1
send "snapshot save ${SNAP:-82880000 831DA600}"
run off1 off
run off2 off
run on1 on
send "freeze on\nexpand off\nsnapshot load\nfreeze off"
"$(dirname "$(realpath "$0")")/trace-compare.py" "$OUT/off1.trace" "$OUT/off2.trace" "$OUT/on1.trace"
