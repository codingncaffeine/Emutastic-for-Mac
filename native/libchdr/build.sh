#!/usr/bin/env bash
# Builds libchdr.<so|dylib> from the vendored source (../libchdr-src, master @ a369a70).
# Deps (lzma/miniz/zstd) are bundled in the source tree; cmake builds them in.
# Cross-platform: gcc/.so on Linux, clang/.dylib on macOS.
set -euo pipefail
cd "$(dirname "$0")"
SRC=../libchdr-src

if [ "$(uname)" = "Darwin" ]; then
  EXT=dylib; NPROC=$(sysctl -n hw.ncpu); LIBGLOB="libchdr*.dylib"
else
  EXT=so; NPROC=$(nproc); LIBGLOB="libchdr.so*"
fi

cmake -S "$SRC" -B build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON -DWITH_SYSTEM_ZLIB=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON > /dev/null
cmake --build build -j"$NPROC" > /dev/null
# shellcheck disable=SC2086
cp build/$LIBGLOB . 2>/dev/null || cp build/src/$LIBGLOB . 2>/dev/null
# Normalize the SONAME / versioned variants to one loadable name.
[ -f "libchdr.$EXT" ] || cp "$(ls $LIBGLOB | head -1)" "libchdr.$EXT"
echo "[libchdr] built libchdr.$EXT"
