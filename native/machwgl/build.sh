#!/usr/bin/env bash
# Builds libwlpresent.dylib — the macOS (CGL) offscreen GL hardware-render context for 3D libretro
# cores. Exports the same wlp_hw_* C ABI as the Linux libwlpresent.so so HwGlContext.cs is unchanged.
# macOS only; system frameworks only (OpenGL).
set -euo pipefail
cd "$(dirname "$0")"
[ "$(uname)" = "Darwin" ] || { echo "[machwgl] not macOS — skipping"; exit 0; }
CC=${CC:-clang}
echo "[machwgl] compiling libwlpresent.dylib (CGL HW-render) with $CC"
# macOS OpenGL is deprecated-but-present → silence the deprecation noise.
"$CC" -O2 -fPIC -Wall -Wno-deprecated-declarations \
      -dynamiclib -install_name "@rpath/libwlpresent.dylib" \
      -o libwlpresent.dylib machwgl.c -framework OpenGL
echo "[machwgl] done -> $(pwd)/libwlpresent.dylib"
