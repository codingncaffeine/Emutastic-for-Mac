using System;
using System.Collections.Generic;

namespace Emutastic.Services.ConsoleHandlers
{
    public class ThreeDsHandler : ConsoleHandlerBase
    {
        public override string ConsoleName => "3DS";
        public override bool UsesAnalogStick => true;

        // macOS: Vulkan over MoltenVK. Azahar's libretro layer (citra_libretro/libretro_vk.cpp) drives the
        // libretro Vulkan HW-render interface our libvkpresent serves — same path as N64/GameCube. The
        // backend is chosen by the citra_graphics_api option below; PreferredHwContext (env 56) is the
        // belt-and-suspenders hint. Elsewhere: leave the core's default (GL).
        public override int PreferredHwContext => OperatingSystem.IsMacOS() ? 6 : -1; // 6 = RETRO_HW_CONTEXT_VULKAN

        public override Dictionary<string, string> GetDefaultCoreOptions() => OperatingSystem.IsMacOS()
            ? new Dictionary<string, string>
            {
                ["citra_graphics_api"] = "Vulkan",
                // Disable the disk shader cache on macOS: MoltenVK can't deserialize pipeline binaries, so
                // azahar REBUILDS every cached pipeline from SPIR-V at boot (InitPLCache) — a long black
                // screen that grows with the cache. Off → fast boot; shaders compile on demand during play
                // (brief one-time hitches, like N64/GameCube) instead of a multi-second black boot.
                ["citra_use_disk_shader_cache"] = "disabled",
            }
            : new Dictionary<string, string>();

        // azahar mis-sizes its av_info max geometry when citra_resolution_factor is pre-seeded
        // before retro_load_game (10x boot reported 640x480 max -> render clipped to a corner
        // sliver; 1x boot reports the full 7200x4800). Verified with testing: applying the
        // factor through the live variables-dirty path right after frame 1 renders correctly
        // at any factor, so the saved value is deferred past load.
        public override System.Collections.Generic.IReadOnlyCollection<string> DeferUntilAfterLoad
            => new[] { "citra_resolution_factor" };

        public override List<(string key, string label)> GetVisualOptions() => new()
        {
            ("citra_resolution_factor", "Internal Resolution"),
            ("citra_texture_filter", "Texture Filter"),
        };
    }
}
