// libemusurface (parent side) — host the shared game IOSurface inside the EmuTV window.
//
// The parent EmuTV process embeds a layer-backed NSView (via Avalonia NativeControlHost) and, each
// display tick, points a CALayer's contents at the latest ready IOSurface from the game-host's ring.
// One window, one Space — the game appears INSIDE EmuTV, no second window, no focus handoff.
//
// IMPORTANT orientation note: a layer-backed NSView's *backing* layer has its geometry (bounds/position/
// transform) MANAGED BY APPKIT — setting `affineTransform` on it is silently overridden, so it cannot be
// used to flip the image. The game renders GL bottom-up, so it would show upside down. The fix is to put
// the IOSurface in a CONTENT SUBLAYER we own outright (AppKit never touches sublayers) and flip THAT. The
// view subclass keeps the sublayer filling the view's bounds on every layout pass.
//
// AppKit/QuartzCore objc lives here (separate .m) so emusurface.c stays pure C. Manual retain/release
// (no ARC). All functions MUST be called on the main thread (Avalonia's UI thread is the process main
// thread on macOS), since they touch NSView/CALayer.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <IOSurface/IOSurface.h>
#import <objc/runtime.h>

// Orientation: the game renders GL bottom-up, so the picture must be flipped once relative to the WINDOW.
// The flip used to be set ONCE, at view creation — before NativeControlHost inserted the view into the
// window. In EmuTV's fullscreen window the view joins Avalonia's own layer tree (under geometryFlipped
// ancestors), and the picture came up upside down, OSD included; the windowed test harness never showed
// it. So the WANTED flip is stored on the layer and the transform is re-derived and re-applied whenever
// the view is attached, laid out, or bound, accounting for any flip the ancestors impose
// (contentsAreFlipped = parity of geometryFlipped up the tree). Verified by the user in EmuTV.
static void emusurf_apply_orientation(CALayer *layer) {
    if (!layer) return;
    BOOL want     = [[layer valueForKey:@"emuWantFlip"] boolValue];
    BOOL implicit = [layer contentsAreFlipped];
    BOOL apply    = want != implicit;
    CGAffineTransform t = apply ? CGAffineTransformMakeScale(1, -1) : CGAffineTransformIdentity;
    if (!CGAffineTransformEqualToTransform(layer.affineTransform, t)) {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        layer.affineTransform = t;
        [CATransaction commit];
    }
    NSNumber *last = [layer valueForKey:@"emuOrientLogged"];
    int state = (want ? 4 : 0) | (implicit ? 2 : 0) | (apply ? 1 : 0);
    if (!last || last.intValue != state) {
        [layer setValue:@(state) forKey:@"emuOrientLogged"];
        fprintf(stderr, "[emusurf] orientation want=%d ancestorsFlipped=%d -> transform=%s\n",
                want, implicit, apply ? "flipY" : "identity");
    }
}

// A layer-backed host view that keeps an owned content sublayer sized to its bounds. We control the
// sublayer's transform (the flip) ourselves — unlike the backing layer, AppKit leaves sublayers alone.
@interface EmuHostView : NSView
@property(assign) CALayer *content;   // weak: retained by the backing layer's sublayer list
- (void)resizeContent;
@end

@implementation EmuHostView
- (BOOL)isFlipped { return NO; }
- (void)resizeContent {
    if (!_content) return;
    CGRect b = self.bounds;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];           // no implicit resize animation
    _content.bounds = CGRectMake(0, 0, b.size.width, b.size.height);
    _content.position = CGPointMake(b.size.width / 2, b.size.height / 2);   // anchorPoint 0.5,0.5 → center
    [CATransaction commit];                          // NOTE: set bounds+position, never `frame`, so the
}                                                    // sublayer's flip transform isn't disturbed.
- (void)layout            { [super layout]; [self resizeContent]; emusurf_apply_orientation(_content); }
- (void)viewDidMoveToWindow { [super viewDidMoveToWindow]; emusurf_apply_orientation(_content); }
- (void)setFrameSize:(NSSize)s { [super setFrameSize:s]; [self resizeContent]; }
@end

// Create the host view + its owned content sublayer. Returns a retained NSView* (release via _destroy).
void *emusurf_view_create(void) {
    EmuHostView *v = [[EmuHostView alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)];
    v.wantsLayer = YES;
    v.layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    v.layer.opaque = YES;

    CALayer *c = [CALayer layer];
    c.contentsGravity = kCAGravityResizeAspect;   // letterbox; the surface is already aspect-fit so this is exact
    c.magnificationFilter = kCAFilterNearest;     // crisp upscaled pixels (matches the GL NEAREST sampling)
    c.minificationFilter = kCAFilterTrilinear;
    c.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    c.opaque = YES;
    c.anchorPoint = CGPointMake(0.5, 0.5);
    [v.layer addSublayer:c];   // retains c
    v.content = c;
    [v resizeContent];
    return (void *)v;   // owned (alloc); released in emusurf_view_destroy
}

// The CONTENT sublayer (where we set contents + the flip) — NOT the backing layer. NULL-safe.
void *emusurf_view_layer(void *view) { return view ? (void *)((EmuHostView *)view).content : NULL; }

void emusurf_view_destroy(void *view) { if (view) [(NSView *)view release]; }

// Point the content layer at an IOSurface. No implicit animation (a cross-fade between frames would smear).
void emusurf_layer_set_surface(void *layer, void *surface) {
    if (!layer) return;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    ((CALayer *)layer).contents = (id)surface;
    [CATransaction commit];
}

// Force the CURRENT process to be a background/accessory app: no Dock tile, no menu bar, never becomes
// active, invisible to Spaces. The headless game-host MUST call this — inside a .app bundle LaunchServices
// makes the process Regular (foreground) and the SDL_MAC_BACKGROUND_APP hint only SKIPS promotion, it does
// NOT demote an already-Regular bundle. A Regular hidden child still participates in activation/Spaces, so
// when it launches/exits macOS slides the Space. Regular→Accessory is an Apple-supported runtime transition.
void emusurf_set_background_app(void) {
    [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

// Read the current process activation policy (0=Regular 1=Accessory 2=Prohibited). For diagnostics.
long emusurf_activation_policy(void) {
    return (long)[[NSApplication sharedApplication] activationPolicy];
}

// Request an upright picture for bottom-up (GL) content: flip=1. The transform actually applied to the
// sublayer (ours to control, unlike the AppKit-managed backing layer) accounts for any flip the host
// hierarchy already imposes — see emusurf_apply_orientation.
void emusurf_layer_set_flip(void *layer, int flip) {
    if (!layer) return;
    [(CALayer *)layer setValue:@(flip != 0) forKey:@"emuWantFlip"];
    emusurf_apply_orientation((CALayer *)layer);
}

// Diagnostics: re-evaluate the orientation (the view may have been re-parented since the last layout)
// and describe the decision plus the layer chain up to the root, for emulator.log. Returns bytes written.
int emusurf_layer_orientation_report(void *layer, char *buf, int len) {
    if (!layer || !buf || len <= 0) return 0;
    CALayer *l0 = (CALayer *)layer;
    emusurf_apply_orientation(l0);
    int n = snprintf(buf, len, "want=%d ancestorsFlipped=%d transform=%s chain=",
                     [[l0 valueForKey:@"emuWantFlip"] boolValue], [l0 contentsAreFlipped],
                     l0.affineTransform.d < 0 ? "flipY" : "identity");
    for (CALayer *l = l0; l && n < len; l = l.superlayer)
        n += snprintf(buf + n, len - n, "%s%s(gf=%d m22=%.0f)", l == l0 ? "" : " > ",
                      object_getClassName(l), l.geometryFlipped, l.transform.m22);
    return n < len ? n : len - 1;
}
