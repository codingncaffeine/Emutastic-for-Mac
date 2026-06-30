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
4. **Input forwarding** — ✅ done. The active PARENT reads the controller and writes raw SDL-gamepad
   state into an input-mailbox IOSurface; the child reads it (RB/RA wrap every raw gamepad read) so the
   whole per-game mapping is reused. Chose the forwarded mailbox over a virtual joystick (no un-testable
   ABI risk). Keyboard + rumble forwarding tracked as a follow-up (controller is the couch path).
5. **Wire into EmuTV** — ✅ done. LaunchGame (macOS) hosts the child's surface in EmuTV's own window via
   `GameSurfaceView`; quit returns to the carousel. Because the game lives in EmuTV's window, the app
   never loses focus → the Dock/menu stay hidden with NO window-level/activation/presentation hacks.
6. **3D / Vulkan cores** — ✅ functionally done via readback (see below); zero-copy deferred as a
   perf optimization.

### Phase 6 detail — 3D/Vulkan cores already composite into the IOSurface (no new code)

On macOS, HW (Vulkan/MoltenVK) cores already render **offscreen** (`native/vkpresent/macvk.c`) and read
the frame back to a CPU BGRA buffer that flows through the **same `Present()` path** as 2D cores
(`_hwBufA/_hwBufB`, "the software-readback path"). The embedded IOSurface present sits downstream of that
path, so 3D cores composite into the shared ring for free — and identically to the current Mac 3D path
(which is already readback), so parity holds. Verified on an M4: mupen64plus_next (ParaLLEl-RDP) running
Ocarina of Time embedded — ring bound, `hwReadback≈1.0ms`, vsync-locked 100 fps, 644 valid frames, no
errors. Zero-copy (`VK_EXT_metal_objects`: render straight into an IOSurface-backed `VkImage`, skipping the
~1ms readback) is a possible optimization, not required for parity — deferred.

Each phase builds clean, is committed + pushed, and any problem found mid-phase is split into a
sub-task and resolved before advancing. Aesthetic/functional target: **1:1 with the Linux/Windows
couch experience.**
