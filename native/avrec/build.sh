#!/usr/bin/env bash
# Builds libavrec.dylib — a macOS-native (AVFoundation/VideoToolbox) gameplay-recording encoder that
# turns the session's BGRA frames + S16LE audio into an MP4/MOV. macOS only; no third-party deps
# (system frameworks), native arm64. Replaces the ffmpeg encode path on macOS (RecordingService).
# Manual retain/release (-fno-objc-arc) so the ObjC handles can live in a malloc'd C struct.
set -euo pipefail
cd "$(dirname "$0")"

[ "$(uname)" = "Darwin" ] || { echo "[avrec] not macOS — skipping"; exit 0; }

CC=${CC:-clang}
FRAMEWORKS=(-framework Foundation -framework AVFoundation -framework CoreMedia -framework CoreVideo -framework CoreGraphics -framework AudioToolbox)

echo "[avrec] compiling libavrec.dylib with $CC"
"$CC" -O2 -fobjc-exceptions -fno-objc-arc -Wall -Wno-deprecated-declarations \
      -dynamiclib -install_name "@rpath/libavrec.dylib" \
      -o libavrec.dylib avrec.m "${FRAMEWORKS[@]}"

echo "[avrec] done -> $(pwd)/libavrec.dylib"
