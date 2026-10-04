#!/usr/bin/env bash
# Write stage draw/display environments for both g_GpuBuffers through the scene_expansion mod.
# usage: envs.sh <clipx> <clipw> <ofsx> <dispx0> <dispx1> <dispw>
#   draw env clip x/w and ofs x are applied to both buffers;
#   display x is per buffer (buffer 0 shows dispx0, buffer 1 shows dispx1).
set -euo pipefail
CTL=${SCENE_PROBE_DIR:-/tmp/nocturne-expand/ctl}
B0=0x8318F660
B1=$((B0 + 0x1BAF8))
CX=$1; CW=$2; OX=$3; D0=$4; D1=$5; DW=$6
{
  for B in $B0 $B1; do
    printf 'poke16 %x %x\n' $((B + 4)) "$CX"
    printf 'poke16 %x %x\n' $((B + 8)) "$CW"
    printf 'poke16 %x %x\n' $((B + 12)) "$OX"
    printf 'poke16 %x %x\n' $((B + 104)) "$DW"
  done
  printf 'poke16 %x %x\n' $((B0 + 100)) "$D0"
  printf 'poke16 %x %x\n' $((B1 + 100)) "$D1"
} > "$CTL/cmd.tmp"
mv "$CTL/cmd.tmp" "$CTL/cmd"
