#!/usr/bin/env bash
# Builds libemusurface.dylib — the macOS cross-process IOSurface helper for the embedded game-render
# path (the native single-window EmuTV design; see emusurface.c). macOS only. Pure system frameworks
# (IOSurface + CoreFoundation), no Homebrew deps, so it builds anywhere clang is present.
set -euo pipefail
cd "$(dirname "$0")"
[ "$(uname)" = "Darwin" ] || { echo "[emusurface] not macOS — skipping"; exit 0; }
CC=${CC:-clang}
echo "[emusurface] compiling libemusurface.dylib with $CC"
"$CC" -O2 -fPIC -Wall -Wno-deprecated-declarations \
      -dynamiclib -install_name "@rpath/libemusurface.dylib" \
      -o libemusurface.dylib emusurface.c \
      -framework IOSurface -framework CoreFoundation
echo "[emusurface] done -> $(pwd)/libemusurface.dylib"
