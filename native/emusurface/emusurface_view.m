// libemusurface (parent side) — host the shared game IOSurface inside the EmuTV window.
//
// The parent EmuTV process embeds a layer-backed NSView (via Avalonia NativeControlHost) and, each
// display tick, points its CALayer's contents at the latest ready IOSurface from the game-host's ring.
// One window, one Space — the game appears INSIDE EmuTV, no second window, no focus handoff.
//
// AppKit/QuartzCore objc lives here (separate .m) so emusurface.c stays pure C. Manual retain/release
// (no ARC) to match the rest of the shim. All functions MUST be called on the main thread (Avalonia's
// UI thread is the process main thread on macOS), since they touch NSView/CALayer.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <IOSurface/IOSurface.h>

// Create a layer-backed NSView to host the game. Returns a retained NSView* (release via _destroy).
void *emusurf_view_create(void) {
    NSView *v = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)];
    v.wantsLayer = YES;
    v.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
    CALayer *l = v.layer;
    l.contentsGravity = kCAGravityResizeAspect;   // letterbox; the surface is already aspect-fit so this is exact
    l.magnificationFilter = kCAFilterNearest;     // crisp upscaled pixels (matches the GL NEAREST sampling)
    l.minificationFilter = kCAFilterTrilinear;
    l.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    l.opaque = YES;
    return (void *)v;   // owned (alloc); released in emusurf_view_destroy
}

// The view's backing layer (where we set contents). NULL-safe.
void *emusurf_view_layer(void *view) { return view ? (void *)((NSView *)view).layer : NULL; }

void emusurf_view_destroy(void *view) { if (view) [(NSView *)view release]; }

// Point the layer at an IOSurface as its displayed contents. No implicit animation (a cross-fade
// between frames would smear). CALayer accepts an IOSurfaceRef directly as `contents`.
void emusurf_layer_set_surface(void *layer, void *surface) {
    if (!layer) return;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    ((CALayer *)layer).contents = (id)surface;
    [CATransaction commit];
}

// GL renders bottom-origin; a macOS layer-backed view is also bottom-origin, so by default the image is
// upright. Exposed so orientation can be corrected with one call if a given path needs it.
void emusurf_layer_set_flip(void *layer, int flip) {
    if (!layer) return;
    ((CALayer *)layer).affineTransform = flip ? CGAffineTransformMakeScale(1, -1) : CGAffineTransformIdentity;
}
