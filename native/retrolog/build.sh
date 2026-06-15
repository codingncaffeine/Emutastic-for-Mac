#!/usr/bin/env bash
# Builds libretrolog.dylib — a native bridge that vsnprintf's the libretro variadic log callback
# and forwards finished strings to a managed sink (so core logs work correctly on arm64). macOS;
# plain C, no deps.
set -euo pipefail
cd "$(dirname "$0")"
[ "$(uname)" = "Darwin" ] || { echo "[retrolog] not macOS — skipping"; exit 0; }
CC=${CC:-clang}
echo "[retrolog] compiling libretrolog.dylib with $CC"
"$CC" -O2 -fPIC -Wall -dynamiclib -install_name "@rpath/libretrolog.dylib" -o libretrolog.dylib retrolog.c
echo "[retrolog] done -> $(pwd)/libretrolog.dylib"
