#!/usr/bin/env bash
# Hold controller buttons through the scene_expansion scripted pad.
# usage: pad.sh <name>[+<name>...] [hold_seconds=0.2] [repeat=1] [gap=0.5]
# names: up down left right start back a b x y lb rb
set -euo pipefail
CTL=${SCENE_PROBE_DIR:-/tmp/nocturne-expand/ctl}
declare -A BIT=([up]=0x0001 [down]=0x0002 [left]=0x0004 [right]=0x0008 [start]=0x0010
  [back]=0x0020 [lb]=0x0100 [rb]=0x0200 [a]=0x1000 [b]=0x2000 [x]=0x4000 [y]=0x8000)
MASK=0
IFS=+ read -ra NAMES <<< "$1"
for n in "${NAMES[@]}"; do MASK=$((MASK | BIT[$n])); done
HOLD=${2:-0.2}; REPEAT=${3:-1}; GAP=${4:-0.5}
for _ in $(seq "$REPEAT"); do
  printf '%x\n' "$MASK" > "$CTL/buttons"
  sleep "$HOLD"
  rm -f "$CTL/buttons"
  sleep "$GAP"
done
