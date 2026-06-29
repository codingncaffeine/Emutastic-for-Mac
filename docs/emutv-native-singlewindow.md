# EmuTV on macOS — native single-window architecture

## Why a Mac-specific design

The Linux/Windows couch flow spawns a **fullscreen child game process** and hands focus back and
forth. macOS Sonoma+ broke that model on purpose:

- App **activation is a cooperative request** — `activate(ignoringOtherApps:)` is deprecated and its
  argument ignored, so a parent app **cannot reclaim focus** after a child process had it. Post-game,
  EmuTV came back non-key.
- `NSApplicationPresentationOptions` (HideDock/HideMenuBar) **only apply while the app is active**, so
  the chrome reappeared whenever EmuTV was non-key.
- Native fullscreen **Spaces** animate cross-process handoff → the "slide to the desktop" bug.

No amount of window-level / activation patching fixes this robustly; it fights the OS. So we use the
**native macOS pattern for exactly this problem — the one OpenEmu has shipped for a decade.**

## The architecture: one app, one window, shared surface

```
  ┌─ EmuTV process (Avalonia, owns its main thread) ──────────────┐
  │  • genuine native fullscreen Space  → OS hides Dock/menu/title │
  │  • NativeControlHost → NSView → CALayer.contents = IOSurface   │  ← shows the game
  │  • owns the controller/keyboard, forwards input over the pipe  │
  └───────────────▲───────────────────────────┬──────────────────┘
       surface id │ (uint32, over stdout pipe) │ input state (over stdin pipe)
  ┌───────────────┴───────────────────────────▼──────────────────┐
  │  child --game-host (SDL/libretro own THIS main thread)        │
  │  • hidden/offscreen SDL window (satisfies SDL, nothing shown) │
  │  • renders into a shared GLOBAL IOSurface (GL/MoltenVK FBO)   │
  │  • injects forwarded input via SDL virtual joystick           │
  └──────────────────────────────────────────────────────────────┘
```

**Why this dissolves the constraints**

- The child keeps its own process, so **SDL/libretro own a main thread uncontended** (the documented
  reason the separate process exists — Avalonia owns the parent's main thread).
- The child renders **headless** (no visible window, no second Space), so **nothing ever steals
  focus** and there is **no handoff**.
- EmuTV is a **real fullscreen Space**, so the **OS** hides the Dock/menu/title bar — robust,
  key-independent, no hacks. "Launch a game" = the layer starts showing frames; "quit" = it stops.

## Transport decision (Phase 1 — DONE, runtime-proven)

A **global `IOSurface` + `IOSurfaceLookup(id)`**, with the uint32 id sent over the existing
parent↔host pipe. Proven on this Sequoia M4:

- `native/emusurface/iosurf_spike.c` — a genuinely separate (exec'd) process looked the surface up by
  id and read the producer's pixels **byte-for-byte**.
- `Emutastic --selftest-iosurface` — the managed P/Invoke chain (create/lookup/lock + pattern
  round-trip) passes headless.

The "deprecated/insecure" note on global surfaces only means another **local** process could read the
frames — irrelevant for a self-signed local emulator, and it avoids all mach-port plumbing. (If ever
needed, the transport is isolated in `libemusurface` + `IOSurfaceInterop` and can move to
`IOSurfaceCreateMachPort` without touching callers.)

Components: `native/emusurface/{emusurface.c,build.sh}` → `libemusurface.dylib`;
`src/Emutastic/Platform/IOSurfaceInterop.cs`.

## Phases

1. **Cross-process IOSurface transport** — ✅ done, runtime-proven (above).
2. **Child renders the libretro frame into the IOSurface** — an "iosurface present" mode in the
   game-host: GL HW cores into an IOSurface-backed FBO (`CGLTexImageIOSurface2D` on
   `GL_TEXTURE_RECTANGLE`); software cores upload their CPU framebuffer. Hidden offscreen SDL window
   satisfies SDL with nothing visible.
3. **Host displays the surface in one Avalonia window** — `GameSurfaceView : NativeControlHost` →
   NSView whose `CALayer.contents` is set to the shared surface each frame (CATransaction on the UI
   thread), driven by the child's per-frame "ready" signal. Validated in a plain test window first.
4. **Input forwarding** — host forwards abstracted controller/keyboard state over the pipe; child
   injects via SDL3 `SDL_AttachVirtualJoystick` + `SDL_SetJoystickVirtual*`. ~1 frame latency.
5. **Wire into EmuTV** — make EmuTV a real fullscreen Space (chromeless via
   `ExtendClientAreaToDecorationsHint`); LaunchGame hosts the child's surface in EmuTV's own window;
   quit returns to the carousel. Remove the window-level/activation/presentation-option hacks.
6. **MoltenVK 3D cores** — render Vulkan cores into an IOSurface-backed `VkImage` via
   `VK_EXT_metal_objects`, reusing the libvkpresent device/queue.

Each phase builds clean, is committed + pushed, and any problem found mid-phase is split into a
sub-task and resolved before advancing. Aesthetic/functional target: **1:1 with the Linux/Windows
couch experience.**
