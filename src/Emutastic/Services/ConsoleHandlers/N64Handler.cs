using System;
using System.Collections.Generic;

namespace Emutastic.Services.ConsoleHandlers
{
    /// <summary>
    /// Handler for N64 — supports two cores with different option namespaces:
    ///  - parallel_n64 (parallel-n64-*): glide64 GL plugin, the original default.
    ///  - mupen64plus_next (mupen64plus-*): standard core; clean audio on the SDL3 path
    ///    where parallel_n64 under-produces. Visuals + defaults switch on CoreFileName.
    /// </summary>
    public class N64Handler : ConsoleHandlerBase
    {
        public override string ConsoleName => "N64";
        public override bool UsesAnalogStick => true;

        private bool IsMupen => CoreFileName.IndexOf("mupen", StringComparison.OrdinalIgnoreCase) >= 0;

        // Request Vulkan context for ParaLLEl-RDP (GPU-accelerated LLE renderer).
        // If the core doesn't support Vulkan, it will fall back to its own preferred context.
        public override int PreferredHwContext => 6; // RETRO_HW_CONTEXT_VULKAN

        // parallel-n64 is built on mupen64plus which ALWAYS spawns an internal EmuThread,
        // even for software plugins like angrylion.  That EmuThread needs its own GL context.
        // Returning true for SET_HW_SHARED_CONTEXT tells mupen64plus to create a shared
        // context on its EmuThread that shares objects (textures, FBOs) with ours.
        // We point get_current_framebuffer at our own FBO so the EmuThread renders into it,
        // then glReadPixels from the shared FBO inside the video callback.
        // UseEmbeddedWindow stays false — we use a hidden offscreen window, not HwndHost.
        // false: glide64 runs on our emu thread (same context as context_reset and readback).
        // Using true puts rendering on mupen64plus's EmuThread with a separate GL context;
        // context_reset's GL objects (FBOs etc.) aren't visible there → black screen.
        public override bool AllowHwSharedContext => false;

        public override List<(string key, string label)> GetVisualOptions() => IsMupen
            ? (OperatingSystem.IsMacOS()
                // macOS runs ParaLLEl-RDP (Vulkan), where the GLideN64 43screensize/txFilter knobs are inert.
                // Expose ParaLLEl-RDP's own resolution multiplier — its marquee sharp-N64 feature — instead.
                // Upscaling enlarges the rendered framebuffer (hence our readback); sized at load → ⚠ restart.
                ? new()
                {
                    ("mupen64plus-parallel-rdp-upscaling", "Upscaling (restart)"),
                    ("mupen64plus-parallel-rdp-synchronous", "Synchronous RDP"),
                    ("mupen64plus-parallel-rdp-deinterlace-method", "Deinterlace"),
                }
                : new()
                {
                    // 43screensize is the OUTPUT resolution GLideN64 renders to — it sizes the av_info
                    // max geometry, hence our HW-render FBO, hence the resolution we read back. (Native-
                    // res-factor only scales GLideN64's internal render, not the libretro framebuffer, so
                    // it does nothing through our readback.) Sized at init → ⚠ restart to take effect.
                    ("mupen64plus-43screensize", "Resolution (restart)"),
                    ("mupen64plus-txFilterMode", "Texture Filter"),
                })
            : new()
            {
                ("parallel-n64-parallel-rdp-upscaling", "Upscaling (restart)"),
            };

        public override Dictionary<string, string> GetDefaultCoreOptions() => IsMupen
            // mupen64plus_next: its own defaults render + sound correctly on Linux (verified), so
            // pre-seed only the essentials — GLideN64 GL renderer, dynarec CPU, controller pak for
            // saves, plus a sharper-than-native default resolution (the downscale-before-readback path
            // makes a larger FBO cheap). The parallel-n64-* keys below are unknown to this core.
            ? new Dictionary<string, string>
            {
                // macOS: ParaLLEl-RDP over Vulkan/MoltenVK (Phase 2 HW-render) — GLideN64-over-GL is
                // broken on Apple's strict GL core profile (black). Elsewhere keep GLideN64 (GL).
                ["mupen64plus-rdp-plugin"]   = OperatingSystem.IsMacOS() ? "parallel" : "gliden64",
                ["mupen64plus-rsp-plugin"]   = "hle",
                ["mupen64plus-cpucore"]      = "dynamic_recompiler",
                ["mupen64plus-pak1"]         = "memory",
                ["mupen64plus-43screensize"] = "960x720",
                // macOS/ParaLLEl-RDP defaults. Upscaling 2x is a light sharpness bump with bulletproof
                // headroom; 4x/8x stay user-selectable via the exposed Upscaling option (8x holds 60fps on
                // the M4 thanks to libvkpresent's pipelined Vulkan readback + the async GPU below). The
                // synchronous=False is the single biggest perf win at high upscaling: the core submits its
                // render and continues, so the GPU pipelines across frames instead of the CPU blocking on
                // each frame's render (~12ms → it lifted 8x from ~47fps to a locked 60). Accuracy-sensitive
                // games (CPU framebuffer effects) can re-enable it via the "Synchronous RDP" option.
                ["mupen64plus-parallel-rdp-upscaling"]   = "2x",
                ["mupen64plus-parallel-rdp-synchronous"] = "False",
            }
            : new Dictionary<string, string>
        {
            // GL HLE plugin (GPU-accelerated) — the Phase-1 GL HW-render path. NOT "parallel" (ParaLLEl-RDP
            // is Vulkan-only; with no Vulkan yet the core falls back to SOFTWARE angrylion → slow). Switch
            // back to "parallel" once Vulkan HW-render (Phase 2) lands, for ParaLLEl-RDP accuracy/upscaling.
            ["parallel-n64-gfxplugin"]             = "glide64",
            ["parallel-n64-cpucore"]               = "dynamic_recompiler",
            ["parallel-n64-disable_expmem"]        = "disabled",
            ["parallel-n64-framerate"]             = "fullspeed",
            ["parallel-n64-angrylion-sync"]        = "Low",
            ["parallel-n64-angrylion-vioverlay"]   = "Filtered",
            ["parallel-n64-angrylion-multithread"] = "all threads",
            ["parallel-n64-angrylion-overscan"]    = "disabled",
            ["parallel-n64-audio-buffer-size"]     = "2048",
            ["parallel-n64-pak1"]                  = "memory",
            ["parallel-n64-pak2"]                  = "none",
            ["parallel-n64-pak3"]                  = "none",
            ["parallel-n64-pak4"]                  = "none",
            ["parallel-n64-astick-deadzone"]       = "15",
            ["parallel-n64-astick-sensitivity"]    = "100",
            ["parallel-n64-gfxplugin-accuracy"]    = "low",
            ["parallel-n64-parallel-rdp-upscaling"] = "4x",
            ["parallel-n64-parallel-rdp-synchronous"] = "enabled",
            ["parallel-n64-screensize"]            = "640x480",
            ["parallel-n64-aspectratiohint"]       = "normal",
            ["parallel-n64-filtering"]             = "automatic",
            ["parallel-n64-virefresh"]             = "auto",
            ["parallel-n64-bufferswap"]            = "disabled",
            ["parallel-n64-alt-map"]               = "disabled",
        };
    }
}
