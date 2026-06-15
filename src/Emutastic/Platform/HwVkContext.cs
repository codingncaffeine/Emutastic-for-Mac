using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Emutastic.Platform
{
    /// <summary>
    /// Vulkan (MoltenVK) hardware-render context for 3D libretro cores on macOS — the Vulkan analog of
    /// <see cref="HwGlContext"/>. Backed by libvkpresent.dylib (native/vkpresent/macvk.c). We create the
    /// VkInstance; the core's negotiation interface (if provided) creates the VkDevice with its features;
    /// the core renders into its own VkImage and hands it to us via libretro's Vulkan interface, and
    /// Readback copies that image to top-down BGRA for the normal present path. macOS only.
    /// </summary>
    public static class HwVkContext
    {
        const string LIB = "vkpresent";

        static HwVkContext()
        {
            // Ensure the single DllImport resolver (which also resolves "vkpresent") is registered.
            // On macOS it's GlInterop.Gl; on Linux it's WlToplevelPresenter. Both are safe to force-run.
            RuntimeHelpers.RunClassConstructor(typeof(Gl).TypeHandle);
            RuntimeHelpers.RunClassConstructor(typeof(WlToplevelPresenter).TypeHandle);
        }

        [DllImport(LIB)] static extern void vkp_hw_set_negotiation(IntPtr neg);
        [DllImport(LIB)] static extern int  vkp_hw_init(int ctxType, int major, int minor, int wantDepth, int wantStencil, int maxw, int maxh);
        [DllImport(LIB)] static extern IntPtr vkp_hw_interface();
        [DllImport(LIB)] static extern int  vkp_hw_readback(byte[] outBgra, int curW, int curH, int bottomLeft, out int outW, out int outH);
        [DllImport(LIB)] static extern void vkp_hw_destroy();
        [DllImport(LIB)] static extern IntPtr vkp_hw_info();
        [DllImport(LIB)] static extern void vkp_hw_readback_times(out double issueMs, out double mapMs);
        [DllImport(LIB)] static extern void vkp_hw_set_present_target(int w, int h);

        /// <summary>Hand the core's Vulkan device-creation negotiation interface to the shim (before Init).</summary>
        public static void SetNegotiation(IntPtr neg) => vkp_hw_set_negotiation(neg);

        /// <summary>Create the Vulkan instance/device/queue (call on the emu/worker thread, after retro_load_game).</summary>
        public static bool Init(int ctxType, int major, int minor, bool depth, bool stencil, int maxW, int maxH)
            => vkp_hw_init(ctxType, major, minor, depth ? 1 : 0, stencil ? 1 : 0, maxW, maxH) != 0;

        /// <summary>The retro_hw_render_interface_vulkan pointer to hand the core via GET_HW_RENDER_INTERFACE.</summary>
        public static IntPtr InterfacePtr() => vkp_hw_interface();

        /// <summary>Copy the core's latest image to BGRA (top-down). bottomLeft is ignored (Vulkan is top-left).</summary>
        public static bool Readback(byte[] outBgra, int curW, int curH, bool bottomLeft, out int w, out int h)
            => vkp_hw_readback(outBgra, curW, curH, bottomLeft ? 1 : 0, out w, out h) != 0;

        /// <summary>Window pixel size — the readback GPU-downscales the (upscaled) core frame to this before
        /// copying to host, so high upscaling stays cheap. 0 = full-res readback.</summary>
        public static void SetPresentTarget(int w, int h) => vkp_hw_set_present_target(w, h);

        public static void Destroy() => vkp_hw_destroy();
        public static string Info() => Marshal.PtrToStringAnsi(vkp_hw_info()) ?? "";
        public static (double issue, double map) ReadbackTimes() { vkp_hw_readback_times(out double i, out double m); return (i, m); }
    }
}
