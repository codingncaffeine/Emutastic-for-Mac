using System;
using System.Runtime.InteropServices;

namespace Emutastic.Platform
{
    /// <summary>
    /// P/Invoke to libretrolog.dylib — formats the libretro variadic log callback in native C
    /// (vsnprintf) and forwards finished strings to a managed sink. Used on macOS so core log
    /// messages keep their "%s"/"%d" arguments (a managed delegate can't read C varargs on arm64).
    /// macOS-only.
    /// </summary>
    internal static class RetroLogNative
    {
        private const string Lib = "retrolog";

        /// <summary>Managed sink: receives the fully-formatted message (UTF-8 pointer) + level.</summary>
        public delegate void Sink(int level, IntPtr msg);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void retrolog_set_sink(Sink sink);

        /// <summary>The native variadic function pointer to hand the core as its retro_log_printf_t.</summary>
        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr retrolog_get_callback();
    }
}
