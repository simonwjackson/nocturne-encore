#!/usr/bin/env bash
# Kill every scratch NocturneRecomp process started from /tmp/nocturne-expand.
pkill -9 -f 'nocturnerecomp --game_data_root=/tmp/nocturne-expand' || true
pkill -9 -f 'pulseaudio.*nocturne-expand' || true
sleep 1
pgrep -af 'nocturnerecomp --game_data_root=/tmp/nocturne-expand' || echo "no game running"
