// Spike: prove cross-process IOSurface sharing on this Mac before building the real pipeline.
// Two genuinely-separate (exec'd) processes — NOT fork — because Apple frameworks (CoreFoundation
// /XPC, which IOSurfaceLookup uses) are not fork-without-exec safe.
//
//   producer --global : create a GLOBAL IOSurface, write a known BGRA pattern, print "ID=<n>", hold it alive.
//   consumer <id>     : IOSurfaceLookup(id) in a SEPARATE process, verify the pattern byte-for-byte.
//
// Build: clang -Wno-deprecated-declarations -framework IOSurface -framework CoreFoundation -o /tmp/iosurf_spike iosurf_spike.c
// This is a throwaway diagnostic; the real helper lives in libemusurface.* once the transport is proven.

#include <IOSurface/IOSurface.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define W 64
#define H 64
#define BPE 4               // BGRA8888

static CFMutableDictionaryRef surface_props(int global) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int w = W, h = H, bpe = BPE, bpr = W * BPE;
    uint32_t pf = 'BGRA';   // kCVPixelFormatType_32BGRA
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
    if (global) CFDictionarySetValue(d, kIOSurfaceIsGlobal, kCFBooleanTrue);
    CFRelease(nW); CFRelease(nH); CFRelease(nBpe); CFRelease(nBpr); CFRelease(nPf);
    return d;
}

static uint8_t expected_byte(int x, int y, int c) { return (uint8_t)((x * 3 + y * 7 + c * 11) & 0xFF); }

static int do_producer(int global) {
    CFMutableDictionaryRef props = surface_props(global);
    IOSurfaceRef s = IOSurfaceCreate(props);
    CFRelease(props);
    if (!s) { fprintf(stderr, "PRODUCER: IOSurfaceCreate failed\n"); return 2; }
    IOSurfaceID id = IOSurfaceGetID(s);
    IOSurfaceLock(s, 0, NULL);
    uint8_t *base = (uint8_t *)IOSurfaceGetBaseAddress(s);
    size_t bpr = IOSurfaceGetBytesPerRow(s);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            for (int c = 0; c < 4; c++)
                base[y * bpr + x * 4 + c] = expected_byte(x, y, c);
    IOSurfaceUnlock(s, 0, NULL);
    printf("ID=%u\n", id);
    fflush(stdout);
    sleep(30);   // hold the surface alive for the consumer
    return 0;
}

static int do_consumer(IOSurfaceID id) {
    IOSurfaceRef s = IOSurfaceLookup(id);   // real cross-process kernel lookup
    if (!s) { fprintf(stderr, "CONSUMER: IOSurfaceLookup(%u) returned NULL\n", id); return 3; }
    IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL);
    uint8_t *base = (uint8_t *)IOSurfaceGetBaseAddress(s);
    size_t bpr = IOSurfaceGetBytesPerRow(s);
    int mism = 0;
    for (int y = 0; y < H && mism < 5; y++)
        for (int x = 0; x < W && mism < 5; x++)
            for (int c = 0; c < 4; c++)
                if (base[y * bpr + x * 4 + c] != expected_byte(x, y, c)) {
                    fprintf(stderr, "CONSUMER: mismatch at (%d,%d,%d): got %u want %u\n",
                            x, y, c, base[y * bpr + x * 4 + c], expected_byte(x, y, c));
                    mism++;
                }
    IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL);
    CFRelease(s);
    if (mism == 0) { printf("CONSUMER: OK — looked up surface %u by ID and matched %dx%d byte-for-byte\n", id, W, H); return 0; }
    return 4;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "producer") == 0)
        return do_producer(!(argc >= 3 && strcmp(argv[2], "--nonglobal") == 0));
    if (argc >= 3 && strcmp(argv[1], "consumer") == 0)
        return do_consumer((IOSurfaceID)strtoul(argv[2], NULL, 10));
    fprintf(stderr, "usage: %s producer [--nonglobal] | consumer <id>\n", argv[0]);
    return 1;
}
