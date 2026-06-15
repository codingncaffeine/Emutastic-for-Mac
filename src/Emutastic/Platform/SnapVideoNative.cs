using System;
using System.Runtime.InteropServices;

namespace Emutastic.Platform
{
    /// <summary>
    /// P/Invoke to libsnapvideo.dylib — a macOS-native (AVFoundation) decoder that turns a snap
    /// .mp4 into BGRA frames for the game-card preview. System frameworks only; nothing bundled.
    /// macOS-only: never call these on other platforms (the dylib isn't built there).
    /// </summary>
    internal static class SnapVideoNative
    {
        private const string Lib = "snapvideo";   // resolves libsnapvideo.dylib next to the app

        /// <summary>Opens the mp4. Returns a handle (IntPtr.Zero on failure) and the video size + fps.</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr snap_open([MarshalAs(UnmanagedType.LPUTF8Str)] string path,
                                              out int width, out int height, out double fps);

        /// <summary>Copies the next frame as BGRA into dst (width*height*4). 1 = frame, 0 = end, -1 = error.</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int snap_next_bgra(IntPtr handle, IntPtr dst, int dstCap);

        /// <summary>Restarts the decoder from the first frame (for looping).</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void snap_rewind(IntPtr handle);

        /// <summary>Releases the decoder. Safe to call once per handle.</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void snap_close(IntPtr handle);
    }
}
