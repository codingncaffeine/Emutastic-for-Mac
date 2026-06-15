#!/usr/bin/env bash
# Builds libvkpresent.dylib — the macOS Vulkan (MoltenVK) hardware-render backend for 3D libretro cores
# (ParaLLEl-RDP N64, Beetle-PSX-Vulkan, …). Links MoltenVK directly (no Vulkan loader / ICD JSON needed)
# and finds it at runtime via @loader_path (libMoltenVK.dylib is shipped next to it). macOS only.
set -euo pipefail
cd "$(dirname "$0")"
[ "$(uname)" = "Darwin" ] || { echo "[macvk] not macOS — skipping"; exit 0; }
CC=${CC:-clang}
BREW="$(brew --prefix 2>/dev/null || echo /opt/homebrew)"
VKINC="$(brew --prefix vulkan-headers 2>/dev/null || echo "$BREW/opt/vulkan-headers")/include"
echo "[macvk] compiling libvkpresent.dylib with $CC (headers: $VKINC)"
"$CC" -O2 -fPIC -Wall -Wno-deprecated-declarations -I"$VKINC" \
      -dynamiclib -install_name "@rpath/libvkpresent.dylib" \
      -o libvkpresent.dylib macvk.c \
      -L"$BREW/lib" -lMoltenVK -Wl,-rpath,@loader_path

# Homebrew's libMoltenVK bakes in an absolute install_name; rewrite our dependency on it to
# @rpath so the dylib is relocatable — at runtime it's found next to us (rpath = @loader_path),
# i.e. libMoltenVK.dylib shipped alongside libvkpresent.dylib. Keeps dev == bundle identical.
MVK_ABS="$(otool -L libvkpresent.dylib | awk '/libMoltenVK/{print $1; exit}')"
if [ -n "${MVK_ABS:-}" ] && [ "$MVK_ABS" != "@rpath/libMoltenVK.dylib" ]; then
  install_name_tool -change "$MVK_ABS" "@rpath/libMoltenVK.dylib" libvkpresent.dylib
fi
echo "[macvk] done -> $(pwd)/libvkpresent.dylib"
