#!/usr/bin/env bash
# Builds librcheevos.<so|dylib> from the vendored rcheevos source (../rcheevos-src,
# pinned to v11.6.0) plus a checkabi harness that prints sizeof/offsetof for every
# struct the C# interop marshals. RcheevosInterop.VerifyAbi() holds the same numbers
# — if a future version bump shifts a layout, both sides scream instead of silently
# corrupting fields. Cross-platform: gcc/.so on Linux, clang/.dylib on macOS (the ABI
# numbers match — both targets are LP64 little-endian).
set -euo pipefail
cd "$(dirname "$0")"
SRC=../rcheevos-src

CFLAGS="-O2 -fPIC -Wall -Wno-unused-function -I$SRC/include -I$SRC/src -DRC_CLIENT_SUPPORTS_HASH -DRC_DISABLE_LUA"

# Everything except rc_client_raintegration.c (Windows RAIntegration.dll bridge) and
# rc_libretro.c (RetroArch glue; we do our own memory routing).
SOURCES=$(ls "$SRC"/src/*.c "$SRC"/src/rcheevos/*.c "$SRC"/src/rapi/*.c \
             "$SRC"/src/rhash/*.c "$SRC"/src/rurl/*.c 2>/dev/null \
          | grep -v raintegration | grep -v rc_libretro)

if [ "$(uname)" = "Darwin" ]; then
  EXT=dylib; CC=${CC:-cc}; SHARED=(-dynamiclib -install_name "@rpath/librcheevos.dylib")
else
  EXT=so; CC=${CC:-gcc}; SHARED=(-shared)
fi

echo "[rcheevos] compiling librcheevos.$EXT with $CC"
# shellcheck disable=SC2086
"$CC" $CFLAGS "${SHARED[@]}" -o "librcheevos.$EXT" $SOURCES -lm

echo "[rcheevos] building + running checkabi"
"$CC" $CFLAGS -o checkabi checkabi.c -lm
./checkabi | tee rcheevos-abi.txt
echo "[rcheevos] done"
