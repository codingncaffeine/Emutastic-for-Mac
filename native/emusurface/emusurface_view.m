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
- (void)layout            { [super layout]; [self resizeContent]; }
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

// Flip the content sublayer vertically (GL renders bottom-up; a CALayer treats contents top-down). Applied
// to the sublayer's affineTransform — which is ours to control, unlike the AppKit-managed backing layer.
void emusurf_layer_set_flip(void *layer, int flip) {
    if (!layer) return;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    ((CALayer *)layer).affineTransform = flip ? CGAffineTransformMakeScale(1, -1) : CGAffineTransformIdentity;
    [CATransaction commit];
}
