// avrec — native macOS gameplay-recording encoder. Takes the session's BGRA frames + S16LE stereo
// audio and writes an MP4 (H.264/HEVC) or MOV (ProRes) via AVFoundation's AVAssetWriter, which
// encodes through VideoToolbox (hardware-accelerated on Apple Silicon). No ffmpeg, no third-party
// libs, no bundling — system frameworks only, native arm64. This replaces the ffmpeg encode path on
// macOS (see Services/RecordingService.cs). Manual retain/release (-fno-objc-arc) so the ObjC handles
// can live in a malloc'd C struct, exactly like native/snapvideo.
//
// C ABI (see Platform/AvRecNative.cs):
//   void* avrec_start(path, srcW, srcH, dstW, dstH, fps, sampleRate, channels,
//                     codec, videoKbps, audioKbps)   -> handle or NULL on failure
//   int   avrec_video(h, bgra, len)   -> 1 appended, 0 dropped (encoder behind), -1 error
//   int   avrec_audio(h, s16le, len)  -> 1 appended, 0 dropped/no-audio, -1 error
//   int   avrec_stop(h)               -> 0 ok, -1 error (finalizes the file + frees the handle)
// codec: 0 = H.264, 1 = HEVC, 2 = ProRes 422, 3 = ProRes 4444

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    AVAssetWriter *writer;
    AVAssetWriterInput *vin;
    AVAssetWriterInputPixelBufferAdaptor *adaptor;
    AVAssetWriterInput *ain;            // nil when there's no audio track
    CMAudioFormatDescriptionRef afmt;   // NULL when there's no audio track
    int srcW, srcH, dstW, dstH;
    double fps;
    int sampleRate, channels;
    long long vframes;                  // video PTS counter (frames)
    long long asamples;                 // audio PTS counter (sample frames)
} AvRec;

void* avrec_start(const char* cpath, int srcW, int srcH, int dstW, int dstH,
                  double fps, int sampleRate, int channels,
                  int codec, int videoKbps, int audioKbps) {
    @autoreleasepool {
        if (!cpath || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return NULL;
        if (fps <= 0) fps = 60.0;

        NSString *path = [NSString stringWithUTF8String:cpath];
        NSURL *url = [NSURL fileURLWithPath:path];
        [[NSFileManager defaultManager] removeItemAtURL:url error:nil];

        BOOL prores = (codec >= 2);
        NSString *fileType = prores ? AVFileTypeQuickTimeMovie : AVFileTypeMPEG4;

        NSError *err = nil;
        AVAssetWriter *writer = [AVAssetWriter assetWriterWithURL:url fileType:fileType error:&err];
        if (!writer) { NSLog(@"[avrec] writer create failed: %@", err); return NULL; }
        [writer retain];

        NSString *codecKey;
        switch (codec) {
            case 1:  codecKey = AVVideoCodecTypeHEVC;            break;
            case 2:  codecKey = AVVideoCodecTypeAppleProRes422;  break;
            case 3:  codecKey = AVVideoCodecTypeAppleProRes4444; break;
            default: codecKey = AVVideoCodecTypeH264;            break;
        }

        NSMutableDictionary *videoSettings = [NSMutableDictionary dictionaryWithDictionary:@{
            AVVideoCodecKey  : codecKey,
            AVVideoWidthKey  : @(dstW),
            AVVideoHeightKey : @(dstH),
        }];
        if (!prores) {
            int bps = (videoKbps > 0 ? videoKbps : 8000) * 1000;
            NSMutableDictionary *comp = [NSMutableDictionary dictionaryWithDictionary:@{
                AVVideoAverageBitRateKey          : @(bps),
                AVVideoExpectedSourceFrameRateKey : @((int)(fps + 0.5)),
                AVVideoMaxKeyFrameIntervalKey     : @((int)(fps * 2.0 + 0.5)),
                AVVideoAllowFrameReorderingKey    : @NO,   // realtime: no B-frames, lower latency
            }];
            if (codec == 0) comp[AVVideoProfileLevelKey] = AVVideoProfileLevelH264HighAutoLevel;
            videoSettings[AVVideoCompressionPropertiesKey] = comp;
        }

        AVAssetWriterInput *vin = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                                    outputSettings:videoSettings];
        vin.expectsMediaDataInRealTime = YES;
        if (![writer canAddInput:vin]) { NSLog(@"[avrec] cannot add video input"); [writer release]; return NULL; }
        [writer addInput:vin];
        [vin retain];

        NSDictionary *pbAttrs = @{
            (id)kCVPixelBufferPixelFormatTypeKey     : @(kCVPixelFormatType_32BGRA),
            (id)kCVPixelBufferWidthKey               : @(dstW),
            (id)kCVPixelBufferHeightKey              : @(dstH),
            (id)kCVPixelBufferIOSurfacePropertiesKey : @{},
        };
        AVAssetWriterInputPixelBufferAdaptor *adaptor =
            [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:vin
                                                                          sourcePixelBufferAttributes:pbAttrs];
        [adaptor retain];

        // Audio track (optional — skipped cleanly if the format can't be built).
        AVAssetWriterInput *ain = nil;
        CMAudioFormatDescriptionRef afmt = NULL;
        if (sampleRate > 0 && channels > 0) {
            NSDictionary *audioSettings = @{
                AVFormatIDKey         : @(kAudioFormatMPEG4AAC),
                AVNumberOfChannelsKey : @(channels),
                AVSampleRateKey       : @((double)sampleRate),
                AVEncoderBitRateKey   : @((audioKbps > 0 ? audioKbps : 192) * 1000),
            };
            AVAssetWriterInput *a = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio
                                                                      outputSettings:audioSettings];
            a.expectsMediaDataInRealTime = YES;
            AudioStreamBasicDescription asbd = {0};
            asbd.mSampleRate       = sampleRate;
            asbd.mFormatID         = kAudioFormatLinearPCM;
            asbd.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
            asbd.mBitsPerChannel   = 16;
            asbd.mChannelsPerFrame = channels;
            asbd.mFramesPerPacket  = 1;
            asbd.mBytesPerFrame    = 2 * channels;
            asbd.mBytesPerPacket   = 2 * channels;
            if ([writer canAddInput:a] &&
                CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &asbd, 0, NULL, 0, NULL, NULL, &afmt) == noErr) {
                [writer addInput:a];
                [a retain];
                ain = a;
            } else {
                NSLog(@"[avrec] audio track unavailable — recording video only");
                if (afmt) { CFRelease(afmt); afmt = NULL; }
            }
        }

        if (![writer startWriting]) {
            NSLog(@"[avrec] startWriting failed: %@", writer.error);
            if (afmt) CFRelease(afmt);
            if (ain) [ain release];
            [adaptor release]; [vin release]; [writer release];
            return NULL;
        }
        [writer startSessionAtSourceTime:kCMTimeZero];

        AvRec *r = (AvRec*)calloc(1, sizeof(AvRec));
        r->writer = writer; r->vin = vin; r->adaptor = adaptor; r->ain = ain; r->afmt = afmt;
        r->srcW = srcW; r->srcH = srcH; r->dstW = dstW; r->dstH = dstH;
        r->fps = fps; r->sampleRate = sampleRate; r->channels = channels;
        return r;
    }
}

int avrec_video(void* h, const uint8_t* bgra, int len) {
    @autoreleasepool {
        AvRec *r = (AvRec*)h;
        if (!r || !bgra) return -1;
        if (r->writer.status != AVAssetWriterStatusWriting) return -1;
        if (len < r->srcW * r->srcH * 4) return -1;
        // Frames arrive at ~realtime cadence, so the HW encoder is normally ready; briefly wait out a
        // transient not-ready (e.g. a keyframe) before dropping, rather than dropping immediately.
        for (int spin = 0; !r->vin.isReadyForMoreMediaData && spin < 8; spin++) usleep(1000);
        if (!r->vin.isReadyForMoreMediaData) return 0;   // still behind — drop (parity with the queue drop)

        CVPixelBufferPoolRef pool = r->adaptor.pixelBufferPool;
        if (!pool) return 0;
        CVPixelBufferRef pb = NULL;
        if (CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool, &pb) != kCVReturnSuccess || !pb) return -1;

        CVPixelBufferLockBaseAddress(pb, 0);
        uint8_t *dst = (uint8_t*)CVPixelBufferGetBaseAddress(pb);
        size_t dstStride = CVPixelBufferGetBytesPerRow(pb);
        int sW = r->srcW, sH = r->srcH, dW = r->dstW, dH = r->dstH;
        size_t srcStride = (size_t)sW * 4;
        if (sW == dW && sH == dH) {
            for (int y = 0; y < dH; y++)
                memcpy(dst + (size_t)y * dstStride, bgra + (size_t)y * srcStride, srcStride);
        } else {
            // Nearest-neighbor scale (integer upscale, or DAR-adjusted width) — matches the crisp
            // pixel look of the old ffmpeg "flags=neighbor" path.
            for (int y = 0; y < dH; y++) {
                int sy = (int)((long long)y * sH / dH);
                const uint32_t *srow = (const uint32_t*)(bgra + (size_t)sy * srcStride);
                uint32_t *drow = (uint32_t*)(dst + (size_t)y * dstStride);
                for (int x = 0; x < dW; x++) drow[x] = srow[(int)((long long)x * sW / dW)];
            }
        }
        CVPixelBufferUnlockBaseAddress(pb, 0);

        CMTime pts = CMTimeMakeWithSeconds((double)r->vframes / r->fps, 90000);
        BOOL ok = [r->adaptor appendPixelBuffer:pb withPresentationTime:pts];
        CVPixelBufferRelease(pb);
        if (ok) { r->vframes++; return 1; }
        NSLog(@"[avrec] appendPixelBuffer failed: %@", r->writer.error);
        return 0;
    }
}

int avrec_audio(void* h, const uint8_t* pcm, int len) {
    @autoreleasepool {
        AvRec *r = (AvRec*)h;
        if (!r) return -1;
        if (!r->ain || !r->afmt) return 0;   // no audio track — silently ignore
        if (r->writer.status != AVAssetWriterStatusWriting) return -1;
        if (len <= 0) return 0;
        if (!r->ain.isReadyForMoreMediaData) return 0;

        int bytesPerFrame = 2 * r->channels;
        int numFrames = len / bytesPerFrame;
        if (numFrames <= 0) return 0;

        CMBlockBufferRef bb = NULL;
        if (CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, len, kCFAllocatorDefault,
                NULL, 0, len, kCMBlockBufferAssureMemoryNowFlag, &bb) != kCMBlockBufferNoErr || !bb)
            return -1;
        if (CMBlockBufferReplaceDataBytes(pcm, bb, 0, len) != kCMBlockBufferNoErr) { CFRelease(bb); return -1; }

        CMSampleTimingInfo timing;
        timing.duration             = CMTimeMake(1, r->sampleRate);
        timing.presentationTimeStamp = CMTimeMake(r->asamples, r->sampleRate);
        timing.decodeTimeStamp       = kCMTimeInvalid;
        CMSampleBufferRef sb = NULL;
        OSStatus st = CMSampleBufferCreateReady(kCFAllocatorDefault, bb, r->afmt,
                                                numFrames, 1, &timing, 0, NULL, &sb);
        CFRelease(bb);
        if (st != noErr || !sb) { if (sb) CFRelease(sb); return -1; }

        BOOL ok = [r->ain appendSampleBuffer:sb];
        CFRelease(sb);
        if (ok) { r->asamples += numFrames; return 1; }
        return 0;
    }
}

int avrec_stop(void* h) {
    @autoreleasepool {
        AvRec *r = (AvRec*)h;
        if (!r) return -1;
        int result = -1;
        if (r->writer && r->writer.status == AVAssetWriterStatusWriting) {
            [r->vin markAsFinished];
            if (r->ain) [r->ain markAsFinished];
            dispatch_semaphore_t sem = dispatch_semaphore_create(0);
            [r->writer finishWritingWithCompletionHandler:^{ dispatch_semaphore_signal(sem); }];
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
            dispatch_release(sem);
            result = (r->writer.status == AVAssetWriterStatusCompleted) ? 0 : -1;
            if (result != 0) NSLog(@"[avrec] finishWriting status=%ld err=%@", (long)r->writer.status, r->writer.error);
        }
        if (r->afmt) CFRelease(r->afmt);
        if (r->ain) [r->ain release];
        if (r->adaptor) [r->adaptor release];
        if (r->vin) [r->vin release];
        if (r->writer) [r->writer release];
        free(r);
        return result;
    }
}
