using System;
using System.Collections.Generic;

namespace Emutastic.Services.ConsoleHandlers
{
    public class PspHandler : ConsoleHandlerBase
    {
        public override string ConsoleName => "PSP";
        public override bool UsesAnalogStick => true;

        // PPSSPP requests GL 3.1 core but initializes GL via glewInit(), which fails on any core-profile
        // context ("[G3D] glewInit() failed." → black screen) and macOS has NO compatibility profile above
        // 2.1 — so GL is a dead end here. macOS uses Vulkan instead (PreferredHwContext + ppsspp_backend
        // below). Off-macOS keeps the compatibility-GL workaround.
        public override bool ForceCompatibilityGlProfile => !OperatingSystem.IsMacOS();

        // macOS: Vulkan (6) over MoltenVK via libvkpresent; elsewhere no preference (the GL path above).
        public override int PreferredHwContext => OperatingSystem.IsMacOS() ? 6 : -1;

        public override Dictionary<string, string> GetDefaultCoreOptions() => OperatingSystem.IsMacOS()
            ? new Dictionary<string, string> { ["ppsspp_backend"] = "vulkan" }
            : new Dictionary<string, string>();

        // PPSSPP self-paces its CPU to wall clock (its own timing), so it enqueues a wall-clock-sized
        // chunk of audio per retro_run, not a fixed frame's worth. Pacing the loop by Thread.Sleep to
        // that measured chunk (the default audio-progress budget) forms a positive feedback loop that
        // drifts off 60 (~50fps) and stutters audio. RetroArch paces this by audio backpressure instead
        // (runloop_iterate audio_sync: the blocking audio write IS the clock) — match that.
        public override bool PaceByAudioBackpressure => true;

        public override List<(string key, string label)> GetVisualOptions() => new()
        {
            ("ppsspp_internal_resolution", "Internal Resolution"),
            ("ppsspp_texture_filtering", "Texture Filter"),
            ("ppsspp_mulitsample_level", "Anti-Aliasing"),
            ("ppsspp_texture_scaling_level", "Texture Upscaling"),
        };
    }
}
