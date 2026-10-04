#!/usr/bin/env bash
# Build a code mod and install it into a run directory's mods folder.
# usage (inside nix-shell shell.nix): build-mod.sh <srcdir> <name> <rundir>
set -euo pipefail
SRC=$1; NAME=$2; RUN=$3
B=/tmp/nocturne-expand/build/$NAME
cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/tmp/nocturne-expand/sdk -DCMAKE_CXX_COMPILER=clang++ >/dev/null
cmake --build "$B"
mkdir -p "$RUN/mods/$NAME/code"
install -m644 "$B/lib$NAME.so" "$RUN/mods/$NAME/code/lib$NAME.so"
install -m644 "$SRC/mod.toml" "$RUN/mods/$NAME/mod.toml"
echo "installed $RUN/mods/$NAME"
