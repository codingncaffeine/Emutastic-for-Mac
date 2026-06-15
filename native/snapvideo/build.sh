#!/usr/bin/env bash
# Builds libsnapvideo.dylib — a macOS-native (AVFoundation) decoder that turns a snap .mp4 into
# BGRA frames for the game-card preview. macOS only; no third-party deps (system frameworks).
# Manual retain/release (-fno-objc-arc) so ObjC handles can live in a malloc'd C struct.
set -euo pipefail
cd "$(dirname "$0")"

[ "$(uname)" = "Darwin" ] || { echo "[snapvideo] not macOS — skipping"; exit 0; }

CC=${CC:-clang}
FRAMEWORKS=(-framework Foundation -framework AVFoundation -framework CoreMedia -framework CoreVideo -framework CoreGraphics)

echo "[snapvideo] compiling libsnapvideo.dylib with $CC"
"$CC" -O2 -fobjc-exceptions -fno-objc-arc -Wall -Wno-deprecated-declarations \
      -dynamiclib -install_name "@rpath/libsnapvideo.dylib" \
      -o libsnapvideo.dylib snapvideo.m "${FRAMEWORKS[@]}"

echo "[snapvideo] done -> $(pwd)/libsnapvideo.dylib"
