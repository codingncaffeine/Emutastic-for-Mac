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
#include <CoreVideo/CoreVideo.h>
#include <dispatch/dispatch.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLIOSurface.h>
#include <stdint.h>
#include <stdlib.h>
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

// ─────────────────────────────────────────────────────────────────────────────────────────────
// GL ↔ IOSurface binding (Phase 2): the game-host renders the libretro frame into an IOSurface-backed
// GL_TEXTURE_RECTANGLE FBO instead of a visible window. All GL here runs on the CURRENT CGL context
// (SDL's), which the caller has already made current on the present thread. Uses GL_APPLE_fence — the
// macOS-native GPU fence — so the parent only ever composites a surface whose GPU writes are COMPLETE
// (no tearing, no half-frames). Legacy OpenGL.framework (2.1 compat) supports all of this.
// ─────────────────────────────────────────────────────────────────────────────────────────────

// Bind `surface` to a NEW GL_TEXTURE_RECTANGLE in the current context and attach it to a NEW FBO.
// Returns 0 on success; *outTex/*outFbo receive the GL names. Non-zero codes pinpoint the failure.
int emusurf_gl_make_fbo(void *surface, int w, int h, unsigned int *outTex, unsigned int *outFbo) {
    CGLContextObj cgl = CGLGetCurrentContext();
    if (!cgl || !surface || w <= 0 || h <= 0) return 1;
    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
    // BGRA8888 IOSurface <-> GL_TEXTURE_RECTANGLE (the documented macOS zero-copy binding).
    CGLError e = CGLTexImageIOSurface2D(cgl, GL_TEXTURE_RECTANGLE_ARB, GL_RGBA, w, h,
                                        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, (IOSurfaceRef)surface, 0);
    if (e != kCGLNoError) { glDeleteTextures(1, &tex); return 100 + (int)e; }
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, tex, 0);
    GLenum st = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE_EXT) { glDeleteFramebuffersEXT(1, &fbo); glDeleteTextures(1, &tex); return 200 + (int)st; }
    if (outTex) *outTex = tex;
    if (outFbo) *outFbo = fbo;
    return 0;
}

void emusurf_gl_bind_fbo(unsigned int fbo)  { glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo); }
void emusurf_gl_unbind_fbo(void)            { glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); }
void emusurf_gl_delete_fbo(unsigned int fbo, unsigned int tex) {
    if (fbo) glDeleteFramebuffersEXT(1, &fbo);
    if (tex) glDeleteTextures(1, &tex);
}

// GL_APPLE_fence: place a fence after the frame's draw commands so the parent can be told "ready" only
// once the GPU has actually finished writing this surface.
unsigned int emusurf_gl_gen_fence(void) { GLuint f = 0; glGenFencesAPPLE(1, &f); return f; }
void emusurf_gl_set_fence(unsigned int f) { glSetFenceAPPLE(f); glFlush(); }   // submit the work + fence
int  emusurf_gl_test_fence(unsigned int f) { return f && glTestFenceAPPLE(f) == GL_TRUE ? 1 : 0; }
void emusurf_gl_finish_fence(unsigned int f) { if (f) glFinishFenceAPPLE(f); }  // block until this surface is done
void emusurf_gl_delete_fence(unsigned int f) { if (f) glDeleteFencesAPPLE(1, &f); }

// ─────────────────────────────────────────────────────────────────────────────────────────────
// CVDisplayLink vsync (Phase 2): in headless IOSurface mode there is no SwapWindow to block on, so the
// present loop would spin. A CVDisplayLink signals a semaphore every display refresh; the present thread
// waits on it — vsync-locked pacing identical in effect to the window swap, smooth and no judder.
// ─────────────────────────────────────────────────────────────────────────────────────────────
typedef struct { CVDisplayLinkRef link; dispatch_semaphore_t sem; } emu_vsync;

static CVReturn emu_vsync_cb(CVDisplayLinkRef l, const CVTimeStamp *now, const CVTimeStamp *out,
                             CVOptionFlags inF, CVOptionFlags *outF, void *ctx) {
    (void)l; (void)now; (void)out; (void)inF; (void)outF;
    dispatch_semaphore_signal(((emu_vsync *)ctx)->sem);
    return kCVReturnSuccess;
}

void *emusurf_vsync_create(void) {
    emu_vsync *v = (emu_vsync *)calloc(1, sizeof(emu_vsync));
    if (!v) return NULL;
    v->sem = dispatch_semaphore_create(0);
    if (CVDisplayLinkCreateWithActiveCGDisplays(&v->link) != kCVReturnSuccess || !v->link) { free(v); return NULL; }
    CVDisplayLinkSetOutputCallback(v->link, emu_vsync_cb, v);
    CVDisplayLinkStart(v->link);
    return v;
}

// Block until the next display refresh, up to timeoutMs (safety so a missed callback can't hang us).
// Returns 1 if a vsync tick arrived, 0 on timeout.
int emusurf_vsync_wait(void *h, int timeoutMs) {
    if (!h) return 0;
    emu_vsync *v = (emu_vsync *)h;
    dispatch_time_t t = dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeoutMs * 1000000LL);
    return dispatch_semaphore_wait(v->sem, t) == 0 ? 1 : 0;
}

void emusurf_vsync_destroy(void *h) {
    if (!h) return;
    emu_vsync *v = (emu_vsync *)h;
    if (v->link) { CVDisplayLinkStop(v->link); CVDisplayLinkRelease(v->link); }
    free(v);
}
