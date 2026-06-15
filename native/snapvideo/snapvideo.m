// snapvideo — decode a ScreenScraper snap .mp4 to BGRA frames for the game-card preview.
//
// macOS-native: uses AVFoundation's AVAssetReader (system framework, no third-party libs, no
// bundling, native arm64). Exposes a tiny C ABI the C# side calls via DllImport; the C# side
// pulls frames on a timer and blits them into the existing WriteableBitmap. Manual retain/release
// (-fno-objc-arc) so the ObjC handles can live in a malloc'd C struct.
//
// API:
//   void* snap_open(const char* path, int* w, int* h, double* fps)  -> handle, or NULL on failure
//   int   snap_next_bgra(void* h, uint8_t* dst, int dstCap)         -> 1 frame copied, 0 end, -1 error
//   void  snap_rewind(void* h)                                      -> restart for looping
//   void  snap_close(void* h)
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    AVURLAsset *asset;
    AVAssetTrack *track;
    AVAssetReader *reader;
    AVAssetReaderTrackOutput *output;
    int width, height;
    double fps;
} SnapDecoder;

// (Re)create the reader/output so we can read from the top — AVAssetReader is one-shot.
static int snap_setup_reader(SnapDecoder *d) {
    NSError *err = nil;
    AVAssetReader *reader = [[AVAssetReader assetReaderWithAsset:d->asset error:&err] retain];
    if (!reader) return 0;
    NSDictionary *settings = @{ (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };
    AVAssetReaderTrackOutput *out =
        [[AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:d->track outputSettings:settings] retain];
    out.alwaysCopiesSampleData = NO;
    if (![reader canAddOutput:out] ) { [out release]; [reader release]; return 0; }
    [reader addOutput:out];
    if (![reader startReading]) { [out release]; [reader release]; return 0; }
    d->reader = reader;
    d->output = out;
    return 1;
}

static void snap_teardown_reader(SnapDecoder *d) {
    if (d->reader) { [d->reader cancelReading]; [d->reader release]; d->reader = nil; }
    if (d->output) { [d->output release]; d->output = nil; }
}

void* snap_open(const char* cpath, int* outW, int* outH, double* outFps) {
    @autoreleasepool {
        if (!cpath) return NULL;
        NSString *path = [NSString stringWithUTF8String:cpath];
        NSURL *url = [NSURL fileURLWithPath:path];
        AVURLAsset *asset = [AVURLAsset URLAssetWithURL:url options:nil];
        if (!asset) return NULL;
        NSArray *tracks = [asset tracksWithMediaType:AVMediaTypeVideo];
        if (tracks.count == 0) return NULL;
        AVAssetTrack *track = tracks.firstObject;
        CGSize sz = track.naturalSize;
        int w = (int)(sz.width + 0.5);
        int h = (int)(sz.height + 0.5);
        if (w <= 0 || h <= 0) return NULL;

        SnapDecoder *d = (SnapDecoder*)calloc(1, sizeof(SnapDecoder));
        d->asset = [asset retain];
        d->track = [track retain];
        d->width = w;
        d->height = h;
        d->fps = track.nominalFrameRate > 0 ? track.nominalFrameRate : 30.0;

        if (!snap_setup_reader(d)) {
            [d->track release];
            [d->asset release];
            free(d);
            return NULL;
        }
        if (outW)   *outW = w;
        if (outH)   *outH = h;
        if (outFps) *outFps = d->fps;
        return d;
    }
}

int snap_next_bgra(void* handle, uint8_t* dst, int dstCap) {
    if (!handle || !dst) return -1;
    SnapDecoder *d = (SnapDecoder*)handle;
    @autoreleasepool {
        if (!d->reader || d->reader.status != AVAssetReaderStatusReading) return 0;
        CMSampleBufferRef sample = [d->output copyNextSampleBuffer];
        if (!sample) return 0;   // end of stream (status flips to Completed)
        CVImageBufferRef img = CMSampleBufferGetImageBuffer(sample);
        if (!img) { CFRelease(sample); return -1; }

        CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
        int w = (int)CVPixelBufferGetWidth(img);
        int h = (int)CVPixelBufferGetHeight(img);
        size_t srcStride = CVPixelBufferGetBytesPerRow(img);
        const uint8_t *src = (const uint8_t*)CVPixelBufferGetBaseAddress(img);

        int dstW = (w < d->width) ? w : d->width;
        int dstStride = d->width * 4;
        int rows = (h < d->height) ? h : d->height;
        size_t rowBytes = (size_t)dstW * 4;
        if (rowBytes > srcStride) rowBytes = srcStride;

        int rc = 1;
        if (src && dstCap >= dstStride * rows) {
            for (int y = 0; y < rows; y++)
                memcpy(dst + (size_t)y * dstStride, src + (size_t)y * srcStride, rowBytes);
        } else {
            rc = -1;
        }
        CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
        CFRelease(sample);
        return rc;
    }
}

void snap_rewind(void* handle) {
    if (!handle) return;
    SnapDecoder *d = (SnapDecoder*)handle;
    @autoreleasepool {
        snap_teardown_reader(d);
        snap_setup_reader(d);
    }
}

void snap_close(void* handle) {
    if (!handle) return;
    SnapDecoder *d = (SnapDecoder*)handle;
    @autoreleasepool {
        snap_teardown_reader(d);
        if (d->track) [d->track release];
        if (d->asset) [d->asset release];
    }
    free(d);
}

#ifdef SNAPVIDEO_TEST
#include <stdio.h>
int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <snap.mp4>\n", argv[0]); return 2; }
    int w = 0, h = 0; double fps = 0;
    void* d = snap_open(argv[1], &w, &h, &fps);
    if (!d) { fprintf(stderr, "snap_open FAILED\n"); return 1; }
    printf("opened: %dx%d @ %.2ffps\n", w, h, fps);
    uint8_t* buf = (uint8_t*)malloc((size_t)w * h * 4);
    int frames = 0, rc;
    while ((rc = snap_next_bgra(d, buf, w * h * 4)) == 1) frames++;
    printf("decoded %d frames (last rc=%d)\n", frames, rc);
    snap_rewind(d);
    rc = snap_next_bgra(d, buf, w * h * 4);
    printf("after rewind, first frame rc=%d (1=ok)\n", rc);
    // sanity: is there non-zero pixel data?
    long nonzero = 0; for (long i = 0; i < (long)w*h*4; i++) if (buf[i]) nonzero++;
    printf("non-zero bytes in frame: %ld / %ld\n", nonzero, (long)w*h*4);
    free(buf);
    snap_close(d);
    return 0;
}
#endif
