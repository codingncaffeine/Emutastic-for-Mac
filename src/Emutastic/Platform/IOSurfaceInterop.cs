using System;
using System.Runtime.InteropServices;

namespace Emutastic.Platform
{
    /// <summary>
    /// macOS-only P/Invoke over libemusurface.dylib — the cross-process IOSurface helper behind the
    /// native single-window EmuTV design (matching OpenEmu's architecture). The child --game-host
    /// process renders the game HEADLESS into a shared GPU <see cref="IOSurface"/>; the parent EmuTV
    /// process looks it up by integer id (handed over the existing pipe) and shows it in a CALayer in
    /// its own fullscreen-Space window. One app, one window — no second window, no focus handoff, and
    /// the OS hides the Dock/menu because EmuTV is genuinely fullscreen.
    ///
    /// Transport is a GLOBAL IOSurface + IOSurfaceLookup(id), proven cross-process on Sequoia (M4).
    /// The "insecure" note on global surfaces only means another LOCAL process could read the frames —
    /// irrelevant for a self-signed local emulator, and it avoids all mach-port plumbing.
    /// </summary>
    public static class IOSurfaceInterop
    {
        private const string Lib = "emusurface";   // libemusurface.dylib, ships @ OutDir (csproj CopyEmuSurface)

        [DllImport(Lib, EntryPoint = "emusurf_create")]
        private static extern IntPtr emusurf_create(int w, int h, out uint outId);

        [DllImport(Lib, EntryPoint = "emusurf_lookup")]
        private static extern IntPtr emusurf_lookup(uint id);

        [DllImport(Lib, EntryPoint = "emusurf_get_id")]       private static extern uint   emusurf_get_id(IntPtr s);
        [DllImport(Lib, EntryPoint = "emusurf_width")]        private static extern int    emusurf_width(IntPtr s);
        [DllImport(Lib, EntryPoint = "emusurf_height")]       private static extern int    emusurf_height(IntPtr s);
        [DllImport(Lib, EntryPoint = "emusurf_bytes_per_row")]private static extern nuint  emusurf_bytes_per_row(IntPtr s);
        [DllImport(Lib, EntryPoint = "emusurf_lock")]         private static extern void   emusurf_lock(IntPtr s, int readOnly);
        [DllImport(Lib, EntryPoint = "emusurf_unlock")]       private static extern void   emusurf_unlock(IntPtr s, int readOnly);
        [DllImport(Lib, EntryPoint = "emusurf_base")]         private static extern IntPtr emusurf_base(IntPtr s);
        [DllImport(Lib, EntryPoint = "emusurf_increment_use")]private static extern uint   emusurf_increment_use(IntPtr s);
        [DllImport(Lib, EntryPoint = "emusurf_retain")]       private static extern void   emusurf_retain(IntPtr s);
        [DllImport(Lib, EntryPoint = "emusurf_release")]      private static extern void   emusurf_release(IntPtr s);

        /// <summary>
        /// A retained reference to a shared BGRA8888 IOSurface. Dispose releases the CF reference (it
        /// does not destroy the surface while another process still holds it). Not thread-safe; guard
        /// per-frame swaps externally.
        /// </summary>
        public sealed class IOSurface : IDisposable
        {
            public IntPtr Handle { get; private set; }
            public uint   Id     { get; }
            public int    Width  { get; }
            public int    Height { get; }
            public int    BytesPerRow { get; }

            private IOSurface(IntPtr handle)
            {
                Handle = handle;
                Id     = emusurf_get_id(handle);
                Width  = emusurf_width(handle);
                Height = emusurf_height(handle);
                BytesPerRow = (int)emusurf_bytes_per_row(handle);
            }

            /// <summary>Create a new global BGRA surface (producer side, in the game-host). Null on failure.</summary>
            public static IOSurface? Create(int width, int height)
            {
                var h = emusurf_create(width, height, out _);
                return h == IntPtr.Zero ? null : new IOSurface(h);
            }

            /// <summary>Look up an existing global surface by id (consumer side, in EmuTV). Null if not found.</summary>
            public static IOSurface? Lookup(uint id)
            {
                var h = emusurf_lookup(id);
                return h == IntPtr.Zero ? null : new IOSurface(h);
            }

            public IntPtr Lock(bool readOnly)   { emusurf_lock(Handle, readOnly ? 1 : 0); return emusurf_base(Handle); }
            public void   Unlock(bool readOnly) => emusurf_unlock(Handle, readOnly ? 1 : 0);

            /// <summary>Bump the use count + return the new seed — tells CoreAnimation the contents changed.</summary>
            public uint IncrementUse() => emusurf_increment_use(Handle);

            public void Dispose()
            {
                if (Handle != IntPtr.Zero) { emusurf_release(Handle); Handle = IntPtr.Zero; }
                GC.SuppressFinalize(this);
            }

            ~IOSurface() { if (Handle != IntPtr.Zero) emusurf_release(Handle); }
        }

        /// <summary>
        /// GL ↔ IOSurface binding (Phase 2) over libemusurface. The game-host renders the libretro frame
        /// into an IOSurface-backed GL_TEXTURE_RECTANGLE FBO on its current CGL context; a GL_APPLE_fence
        /// lets us tell the parent a surface is ready only once its GPU writes have completed. macOS only.
        /// </summary>
        public static class GlSurface
        {
            [DllImport(Lib, EntryPoint = "emusurf_gl_make_fbo")]
            public static extern int MakeFbo(IntPtr surface, int w, int h, out uint tex, out uint fbo);
            [DllImport(Lib, EntryPoint = "emusurf_gl_bind_fbo")]    public static extern void BindFbo(uint fbo);
            [DllImport(Lib, EntryPoint = "emusurf_gl_unbind_fbo")]  public static extern void UnbindFbo();
            [DllImport(Lib, EntryPoint = "emusurf_gl_delete_fbo")]  public static extern void DeleteFbo(uint fbo, uint tex);
            [DllImport(Lib, EntryPoint = "emusurf_gl_gen_fence")]   public static extern uint GenFence();
            [DllImport(Lib, EntryPoint = "emusurf_gl_set_fence")]   public static extern void SetFence(uint fence);
            [DllImport(Lib, EntryPoint = "emusurf_gl_test_fence")]  public static extern int  TestFence(uint fence);
            [DllImport(Lib, EntryPoint = "emusurf_gl_finish_fence")]public static extern void FinishFence(uint fence);
            [DllImport(Lib, EntryPoint = "emusurf_gl_delete_fence")]public static extern void DeleteFence(uint fence);
        }

        /// <summary>
        /// CVDisplayLink vsync (Phase 2): headless IOSurface mode has no window swap to block on, so the
        /// present loop waits on this instead — vsync-locked pacing identical in effect to the window swap.
        /// </summary>
        public static class Vsync
        {
            [DllImport(Lib, EntryPoint = "emusurf_vsync_create")]  public static extern IntPtr Create();
            [DllImport(Lib, EntryPoint = "emusurf_vsync_wait")]    public static extern int    Wait(IntPtr h, int timeoutMs);
            [DllImport(Lib, EntryPoint = "emusurf_vsync_destroy")] public static extern void   Destroy(IntPtr h);
        }

        /// <summary>
        /// Headless runtime self-test of the C# &lt;-&gt; libemusurface marshalling chain (run via
        /// <c>Emutastic --selftest-iosurface</c>). Creates a surface, writes + reads a pattern through
        /// the locked base pointer, then re-looks-it-up by id and confirms the bytes survive. The
        /// cross-PROCESS guarantee is already proven by native/emusurface/iosurf_spike.c; this proves
        /// the managed interop (out-param id, nuint stride, IntPtr base) is correct. Returns 0 on pass.
        /// </summary>
        public static int SelfTest()
        {
            if (!OperatingSystem.IsMacOS()) { Console.Error.WriteLine("[iosurf] not macOS"); return 1; }
            const int W = 64, H = 48;
            try
            {
                using var s = IOSurface.Create(W, H);
                if (s == null) { Console.Error.WriteLine("[iosurf] Create returned null"); return 2; }
                if (s.Id == 0 || s.Width != W || s.Height != H || s.BytesPerRow < W * 4)
                { Console.Error.WriteLine($"[iosurf] bad metadata id={s.Id} {s.Width}x{s.Height} bpr={s.BytesPerRow}"); return 3; }

                byte Pat(int x, int y, int c) => (byte)((x * 5 + y * 13 + c * 17) & 0xFF);
                var baseW = s.Lock(false);
                if (baseW == IntPtr.Zero) { Console.Error.WriteLine("[iosurf] lock(write) base null"); return 4; }
                unsafe
                {
                    byte* p = (byte*)baseW;
                    for (int y = 0; y < H; y++)
                        for (int x = 0; x < W; x++)
                            for (int c = 0; c < 4; c++)
                                p[y * s.BytesPerRow + x * 4 + c] = Pat(x, y, c);
                }
                s.Unlock(false);

                // Re-look-up by id (same process here, but exercises the lookup binding) and verify bytes.
                using var s2 = IOSurface.Lookup(s.Id);
                if (s2 == null) { Console.Error.WriteLine($"[iosurf] Lookup({s.Id}) returned null"); return 5; }
                var baseR = s2.Lock(true);
                int mism = 0;
                unsafe
                {
                    byte* p = (byte*)baseR;
                    for (int y = 0; y < H && mism < 5; y++)
                        for (int x = 0; x < W && mism < 5; x++)
                            for (int c = 0; c < 4; c++)
                                if (p[y * s2.BytesPerRow + x * 4 + c] != Pat(x, y, c)) mism++;
                }
                s2.Unlock(true);
                if (mism != 0) { Console.Error.WriteLine($"[iosurf] {mism} byte mismatches after lookup"); return 6; }

                Console.WriteLine($"[iosurf] OK — created+looked-up surface id={s.Id} {W}x{H} bpr={s.BytesPerRow}, pattern survived round-trip");
                return 0;
            }
            catch (Exception ex) { Console.Error.WriteLine($"[iosurf] exception: {ex}"); return 7; }
        }
    }
}
