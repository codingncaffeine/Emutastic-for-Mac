#!/usr/bin/env bash
# Builds libvkpresent.dylib — the macOS Vulkan hardware-render backend for 3D libretro cores (ParaLLEl-RDP
# N64, Dolphin GameCube, Azahar 3DS, …). Links the Vulkan LOADER (libvulkan), NOT MoltenVK directly — the
# loader's vkGetInstanceProcAddr resolves promoted KHR/EXT function aliases that cores (e.g. Azahar via
# Vulkan-Hpp) load through our interface; MoltenVK-direct does not, which crashed them. The loader reaches
# MoltenVK through the ICD (VK_ICD_FILENAMES, set in macvk at runtime). All three dylibs ship together
# (@loader_path): libvkpresent + libvulkan + libMoltenVK + the MoltenVK ICD JSON. macOS only.
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
      -L"$BREW/lib" -lvulkan -Wl,-rpath,@loader_path \
      -framework QuartzCore -framework Foundation -lobjc   # CAMetalLayer for the offscreen VkSurfaceKHR

# Make the loader dependency relocatable (@rpath) so libvulkan.*.dylib ships next to us.
VK_ABS="$(otool -L libvkpresent.dylib | awk '/libvulkan/{print $1; exit}')"
if [ -n "${VK_ABS:-}" ]; then
  VK_BASE="$(basename "$VK_ABS")"
  [ "$VK_ABS" != "@rpath/$VK_BASE" ] && install_name_tool -change "$VK_ABS" "@rpath/$VK_BASE" libvkpresent.dylib
  echo "[macvk] loader dep: $VK_BASE"
fi
echo "[macvk] done -> $(pwd)/libvkpresent.dylib"
