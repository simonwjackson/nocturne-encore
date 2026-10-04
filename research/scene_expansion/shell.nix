{ pkgs ? import (builtins.getFlake "nixpkgs").outPath { } }:
let
  runtime = with pkgs; [
    vulkan-loader libx11 libxcb libxext libxi libxrandr libxcursor libxfixes
    libxscrnsaver libxkbcommon wayland libdecor alsa-lib libpulseaudio pipewire
    dbus udev libxtst fribidi libthai openxr-loader curl stdenv.cc.cc.lib
    libGL mesa
  ];
in
pkgs.mkShell {
  packages = with pkgs; [
    llvmPackages_20.clang cmake ninja python3 xvfb-run xdotool imagemagick
    binutils pulseaudio
  ];
  LD_LIBRARY_PATH = "/tmp/nocturne-expand/sdk/lib:" + pkgs.lib.makeLibraryPath runtime;
}
