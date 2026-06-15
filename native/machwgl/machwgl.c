// machwgl — offscreen OpenGL hardware-render context for 3D libretro cores on macOS (CGL).
//
// macOS analog of native/wlpresent/wl_hwgl.c. A surfaceless core-profile GL context + FBO that lives
// on the emu (worker) thread; the core renders into our FBO via get_current_framebuffer, and
// wlp_hw_readback glReadPixels() it back to a top-down BGRA frame for the normal present path.
// Exports the SAME wlp_hw_* C ABI the Linux shim does, so HwGlContext.cs is unchanged.
//
// macOS specifics:
//   - CGL (not NSOpenGL) so the context isn't AppKit/main-thread-bound — it stays on the worker.
//   - No OpenGL ES and CGL core profile maxes at GL 4.1; GLES (ctx 2/4) and Vulkan (6) are declined.
//   - GL 3.2+ core entry points are direct exports of OpenGL.framework (gl3.h); the core resolves its
//     own symbols through wlp_hw_proc → dlsym on the framework.
//   - v1 uses a synchronous glReadPixels (no PBO ring); Apple-Silicon unified memory makes a
//     window-sized readback cheap. The async ring can be ported later if a core needs it.
#include <OpenGL/OpenGL.h>   // CGL
#include <OpenGL/gl3.h>      // GL 3.2+ core
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif

static struct {
    CGLContextObj ctx;
    GLuint vao, fbo, color, ds;
    int w, h, legacy;
    unsigned char *rb; int rbcap;
    char info[256];
    double issue_ms, map_ms, mapcall_ms, copy_ms;
    volatile int tgt_w, tgt_h;
} H;

static void *g_glfw;   // OpenGL.framework handle for wlp_hw_proc

static double now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6; }

// ctx_type: 1=OPENGL(compat) 2=GLES2 3=OPENGL_CORE 4=GLES3 (6=VULKAN). Returns 1 on success.
int wlp_hw_init(int ctx_type, int major, int minor, int want_depth, int want_stencil, int maxw, int maxh, int use_glx) {
    (void)use_glx; (void)major; (void)minor;
    if (ctx_type == 2 || ctx_type == 4) { fprintf(stderr, "[machwgl] OpenGL ES not available on macOS — declining (ctx_type=%d)\n", ctx_type); return 0; }
    if (ctx_type == 6) return 0;   // Vulkan — not here
    if (maxw < 1) maxw = 640;
    if (maxh < 1) maxh = 480;

    // OPENGL_CORE → GL 4.1 core (covers GLideN64's 3.3 needs). Bare OPENGL → legacy 2.1 (macOS has
    // no >2.1 compat profile). Most 3D cores that work here request core (type 3).
    H.legacy = (ctx_type != 3);
    CGLOpenGLProfile profile = H.legacy ? kCGLOGLPVersion_Legacy : kCGLOGLPVersion_GL4_Core;

    CGLPixelFormatAttribute attribs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)profile,
        kCGLPFAAccelerated,
        kCGLPFAColorSize,   (CGLPixelFormatAttribute)24,
        kCGLPFAAlphaSize,   (CGLPixelFormatAttribute)8,
        kCGLPFADepthSize,   (CGLPixelFormatAttribute)(want_depth ? 24 : 0),
        kCGLPFAStencilSize, (CGLPixelFormatAttribute)(want_stencil ? 8 : 0),
        (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatObj pix = NULL; GLint npix = 0;
    if (CGLChoosePixelFormat(attribs, &pix, &npix) != kCGLNoError || !pix) { fprintf(stderr, "[machwgl] CGLChoosePixelFormat failed\n"); return 0; }
    CGLError e = CGLCreateContext(pix, NULL, &H.ctx);
    CGLDestroyPixelFormat(pix);
    if (e != kCGLNoError || !H.ctx) { fprintf(stderr, "[machwgl] CGLCreateContext failed (%d)\n", e); return 0; }
    CGLSetCurrentContext(H.ctx);

    H.w = maxw; H.h = maxh;

    // Core profile requires a bound VAO for draws + an explicit draw buffer; create defaults so our
    // own ops (and the core's first calls) are legal. (No-op cost in legacy.)
    if (!H.legacy) { glGenVertexArrays(1, &H.vao); glBindVertexArray(H.vao); }

    glGenFramebuffers(1, &H.fbo); glBindFramebuffer(GL_FRAMEBUFFER, H.fbo);
    glGenTextures(1, &H.color); glBindTexture(GL_TEXTURE_2D, H.color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, maxw, maxh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, H.color, 0);
    if (want_depth || want_stencil) {
        glGenRenderbuffers(1, &H.ds); glBindRenderbuffer(GL_RENDERBUFFER, H.ds);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, maxw, maxh);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, H.ds);
    }
    if (!H.legacy) { GLenum db = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &db); }
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) fprintf(stderr, "[machwgl] FBO incomplete: 0x%x\n", st);
    glBindFramebuffer(GL_FRAMEBUFFER, H.fbo);
    glViewport(0, 0, maxw, maxh);

    const char *ren = (const char*)glGetString(GL_RENDERER), *ven = (const char*)glGetString(GL_VENDOR), *ver = (const char*)glGetString(GL_VERSION);
    snprintf(H.info, sizeof H.info, "renderer=%s vendor=%s version=%s asyncReadback=0", ren ? ren : "?", ven ? ven : "?", ver ? ver : "?");
    fprintf(stderr, "[machwgl] GL HW-render ctx type=%d %dx%d depth=%d fbo=%u %s\n", ctx_type, maxw, maxh, want_depth, H.fbo, H.info);
    return 1;
}

void wlp_hw_make_current(void) { if (H.ctx) CGLSetCurrentContext(H.ctx); }
unsigned int wlp_hw_fbo(void) { return H.fbo; }

void* wlp_hw_proc(const char *sym) {
    if (!g_glfw) g_glfw = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_LOCAL);
    void *p = g_glfw ? dlsym(g_glfw, sym) : NULL;
    if (!p) p = dlsym(RTLD_DEFAULT, sym);
    return p;
}

// Copy BGRA src (GL bottom-up) into out as top-down, forcing opaque alpha (the window surface has an
// alpha channel for rounded corners; the SW present path likewise hard-sets 255).
static void copy_flip_opaque(unsigned char *o, const unsigned char *src, int w, int h, int bottom_left) {
    int stride = w * 4;
    if (bottom_left) for (int y = 0; y < h; y++) memcpy(o + y * stride, src + (h - 1 - y) * stride, stride);
    else            memcpy(o, src, stride * h);
    for (int i = 3, n = stride * h; i < n; i += 4) o[i] = 0xFF;
}

static int s_dbg;
int wlp_hw_readback(void *out, int cur_w, int cur_h, int bottom_left, int *out_w, int *out_h) {
    if (!H.ctx || !out || cur_w <= 0 || cur_h <= 0) return 0;
    if (cur_w > H.w) cur_w = H.w;
    if (cur_h > H.h) cur_h = H.h;
    double t0 = now_ms();
    GLenum errBefore = glGetError();   // errors left by the core's frame
    glBindFramebuffer(GL_READ_FRAMEBUFFER, H.fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    int need = cur_w * cur_h * 4;
    if (need > H.rbcap) { free(H.rb); H.rb = (unsigned char*)malloc(need); H.rbcap = need; }
    glReadPixels(0, 0, cur_w, cur_h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, H.rb);
    GLenum errRead = glGetError();
    copy_flip_opaque((unsigned char*)out, H.rb, cur_w, cur_h, bottom_left);
    glBindFramebuffer(GL_FRAMEBUFFER, H.fbo);   // restore for the core's next draw
    if (out_w) *out_w = cur_w;
    if (out_h) *out_h = cur_h;
    H.issue_ms += 0.05 * ((now_ms() - t0) - H.issue_ms);
    // Diagnostics only when EMUTASTIC_GL_PERF is set — the nonzero scan walks every pixel, so it must
    // not run in shipped binaries. (GL HW-render is unused for N64 on macOS; this path is for GL cores.)
    if (getenv("EMUTASTIC_GL_PERF") && (s_dbg < 3 || (s_dbg % 180) == 0)) {
        long nz = 0, n = (long)cur_w * cur_h * 4;
        for (long i = 0; i < n; i++) if (H.rb[i]) nz++;
        GLenum fbst = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
        fprintf(stderr, "[machwgl] readback #%d %dx%d errBefore=0x%x errRead=0x%x fboStatus=0x%x nonzero=%ld/%ld\n",
                s_dbg, cur_w, cur_h, errBefore, errRead, fbst, nz, n);
    }
    s_dbg++;
    return 1;
}

const char* wlp_hw_info(void) { return H.info; }
void wlp_hw_readback_times(double *issue, double *map) { if (issue) *issue = H.issue_ms; if (map) *map = H.map_ms; }
void wlp_hw_readback_times2(double *mapcall, double *copy) { if (mapcall) *mapcall = H.mapcall_ms; if (copy) *copy = H.copy_ms; }
void wlp_hw_set_present_target(int w, int h) { H.tgt_w = w; H.tgt_h = h; }

void wlp_hw_destroy(void) {
    if (H.ctx) { CGLSetCurrentContext(NULL); CGLDestroyContext(H.ctx); }
    free(H.rb);
    memset(&H, 0, sizeof H);
}
