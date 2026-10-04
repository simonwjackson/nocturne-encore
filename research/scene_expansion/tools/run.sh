#!/usr/bin/env bash
# Run NocturneRecomp v1.4.5 from a private scratch directory under Xvfb.
# usage: run.sh <rundir> <seconds> [extra args...]
# Must be run inside nix-shell /tmp/nocturne-expand/shell.nix.
set -euo pipefail
RUN=$1; SECS=$2; shift 2
PKG=/nix/store/acnmwnfmwli9r99njbmgbg6pg1ihsyiw-nocturnerecomp-1.4.5/libexec/nocturnerecomp
mkdir -p "$RUN"
cd "$RUN"
[ -e nocturnerecomp ] || install -m700 "$PKG/nocturnerecomp" nocturnerecomp
for n in librexruntime.so librexgpu-xenos.so libTracyClient.so shaders; do
  [ -e "$n" ] || ln -s "$PKG/$n" "$n"
done
[ -d assets ] || cp -r /tmp/nocturne-real-assets assets
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/run/opengl-driver/lib"
exec timeout --signal=KILL "$SECS" ./nocturnerecomp \
  --game_data_root="$RUN/assets" --user_data_root="$RUN" \
  --mods_data_root="$RUN/mods" --update_data_root="$RUN/update" \
  --auto_update_enabled=false "$@"
