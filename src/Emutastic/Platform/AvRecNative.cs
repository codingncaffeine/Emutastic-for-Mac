using System;
using System.Runtime.InteropServices;

namespace Emutastic.Platform
{
    /// <summary>
    /// P/Invoke to libavrec.dylib — a macOS-native (AVFoundation/VideoToolbox) gameplay-recording
    /// encoder. Takes the session's BGRA frames + S16LE stereo audio and writes an MP4 (H.264/HEVC)
    /// or MOV (ProRes) with hardware-accelerated encode. System frameworks only; nothing bundled.
    /// macOS-only: never call these on other platforms (the dylib isn't built there).
    /// </summary>
    internal static class AvRecNative
    {
        private const string Lib = "avrec";   // resolves libavrec.dylib next to the app

        /// <summary>codec: 0 = H.264, 1 = HEVC, 2 = ProRes 422, 3 = ProRes 4444.</summary>
        public enum Codec { H264 = 0, Hevc = 1, ProRes422 = 2, ProRes4444 = 3 }

        /// <summary>Start an encode. Returns a handle (IntPtr.Zero on failure). Frames are scaled
        /// (nearest-neighbor) from src→dst; videoKbps/audioKbps are ignored for ProRes.</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr avrec_start([MarshalAs(UnmanagedType.LPUTF8Str)] string path,
                                                int srcW, int srcH, int dstW, int dstH,
                                                double fps, int sampleRate, int channels,
                                                int codec, int videoKbps, int audioKbps);

        /// <summary>Append one BGRA frame (tightly packed, top-down). 1 = appended, 0 = dropped, -1 = error.</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int avrec_video(IntPtr handle, byte[] bgra, int len);

        /// <summary>Append interleaved S16LE audio. 1 = appended, 0 = dropped/no-audio, -1 = error.</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int avrec_audio(IntPtr handle, byte[] s16le, int len);

        /// <summary>Finalize the file and free the handle. 0 = ok, -1 = error.</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int avrec_stop(IntPtr handle);
    }
}
