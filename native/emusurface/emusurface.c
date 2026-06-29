// libemusurface — cross-process IOSurface helper for the macOS embedded game-render path.
//
// The native single-window design (matching OpenEmu): the game runs in the child --game-host
// process (so SDL/libretro own that process's main thread, uncontended), but renders HEADLESS into
// a shared GPU IOSurface instead of a visible window. The parent EmuTV process looks the surface up
// by its integer ID — handed over the existing parent<->host pipe — and shows it in a CALayer inside
// its single fullscreen-Space window. One app, one window: no second window, no focus handoff, and
// the OS hides the Dock/menu because EmuTV is genuinely fullscreen.
//
// Transport: a GLOBAL IOSurface + IOSurfaceLookup(id). Proven cross-process on Sequoia (M4). The
// "deprecated/insecure" tag on global surfaces only means any LOCAL process could read the frames —
// irrelevant for a self-signed local emulator. We pass only a uint32 id, no mach-port plumbing.
//
// This shim also hosts (Phase 2) the GL->IOSurface FBO binding and (Phase 6) the Vulkan binding, so
// all native macOS surface code lives in one place. Built by build.sh -> libemusurface.dylib.

#include <IOSurface/IOSurface.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <string.h>

#ifndef EMUSURF_PIXFMT
#define EMUSURF_PIXFMT 0x42475241  /* 'BGRA' — kCVPixelFormatType_32BGRA, matches the GL/SDL BGRA path */
#endif

// Create a GLOBAL BGRA8888 IOSurface (w x h). Returns a retained IOSurfaceRef (opaque void*) and
// writes its global lookup id to *outId. NULL on failure.
void *emusurf_create(int w, int h, uint32_t *outId) {
    if (w <= 0 || h <= 0) return NULL;
    int bpe = 4, bpr = w * bpe;
    uint32_t pf = EMUSURF_PIXFMT;
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFNumberRef nW   = CFNumberCreate(0, kCFNumberIntType, &w);
    CFNumberRef nH   = CFNumberCreate(0, kCFNumberIntType, &h);
    CFNumberRef nBpe = CFNumberCreate(0, kCFNumberIntType, &bpe);
    CFNumberRef nBpr = CFNumberCreate(0, kCFNumberIntType, &bpr);
    CFNumberRef nPf  = CFNumberCreate(0, kCFNumberSInt32Type, &pf);
    CFDictionarySetValue(d, kIOSurfaceWidth, nW);
    CFDictionarySetValue(d, kIOSurfaceHeight, nH);
    CFDictionarySetValue(d, kIOSurfaceBytesPerElement, nBpe);
    CFDictionarySetValue(d, kIOSurfaceBytesPerRow, nBpr);
    CFDictionarySetValue(d, kIOSurfacePixelFormat, nPf);
    CFDictionarySetValue(d, kIOSurfaceIsGlobal, kCFBooleanTrue);   // cross-process lookup by id
    IOSurfaceRef s = IOSurfaceCreate(d);
    CFRelease(nW); CFRelease(nH); CFRelease(nBpe); CFRelease(nBpr); CFRelease(nPf); CFRelease(d);
    if (!s) return NULL;
    if (outId) *outId = IOSurfaceGetID(s);
    return (void *)s;
}

// Look up a global surface by id (the parent side). Returns a retained IOSurfaceRef or NULL.
void *emusurf_lookup(uint32_t id) { return (void *)IOSurfaceLookup(id); }

uint32_t emusurf_get_id(void *s)  { return s ? IOSurfaceGetID((IOSurfaceRef)s) : 0; }
int   emusurf_width(void *s)      { return s ? (int)IOSurfaceGetWidth((IOSurfaceRef)s) : 0; }
int   emusurf_height(void *s)     { return s ? (int)IOSurfaceGetHeight((IOSurfaceRef)s) : 0; }
size_t emusurf_bytes_per_row(void *s) { return s ? IOSurfaceGetBytesPerRow((IOSurfaceRef)s) : 0; }

// CPU access (software cores upload their framebuffer here; the parent never needs to lock when it
// hands the surface straight to a CALayer). readOnly avoids invalidating the GPU cache on the reader.
void  emusurf_lock(void *s, int readOnly)   { if (s) IOSurfaceLock((IOSurfaceRef)s, readOnly ? kIOSurfaceLockReadOnly : 0, NULL); }
void  emusurf_unlock(void *s, int readOnly) { if (s) IOSurfaceUnlock((IOSurfaceRef)s, readOnly ? kIOSurfaceLockReadOnly : 0, NULL); }
void *emusurf_base(void *s)         { return s ? IOSurfaceGetBaseAddress((IOSurfaceRef)s) : NULL; }

// "Tickle" a surface whose contents changed out from under CoreAnimation (when the parent sets it as
// a CALayer's contents and re-uses it each frame, the seed bump tells the WindowServer to recomposite).
uint32_t emusurf_increment_use(void *s) { if (!s) return 0; IOSurfaceIncrementUseCount((IOSurfaceRef)s); return IOSurfaceGetSeed((IOSurfaceRef)s); }

void  emusurf_retain(void *s)  { if (s) CFRetain(s); }
void  emusurf_release(void *s) { if (s) CFRelease(s); }
