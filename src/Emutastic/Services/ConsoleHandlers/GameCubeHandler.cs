using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace Emutastic.Services.ConsoleHandlers
{
    /// <summary>
    /// Handler for GameCube via Dolphin libretro.
    ///
    /// Key differences from generic cores:
    ///  - Requires all 4 controller ports configured post-LoadGame (triggers Dolphin's input stage 2 init)
    ///  - dolphin_cpu_core must be auto-selected at runtime from the core's valid list (avoid JIT)
    ///  - Analog sticks active
    ///
    /// Linux port note: upstream blocked Windows' d3d11.dll D3D11CreateDevice (via VirtualProtect
    /// byte-patching) during context_reset so Dolphin would fall straight through to OpenGL. On
    /// Linux there is no d3d11.dll and the Linux dolphin_libretro.so has no Direct3D backend — it
    /// uses OpenGL/Vulkan natively — so the dolphin_gfx_backend=OGL core option below is sufficient
    /// and the D3D-blocking hooks are no-ops here. (The upstream code patched the Windows SYSTEM
    /// dll, not the Dolphin core, so there are no core-specific byte offsets to re-derive.)
    /// </summary>
    public class GameCubeHandler : ConsoleHandlerBase
    {
        private const uint RETRO_DEVICE_JOYPAD = 1;

        public override string ConsoleName => "GameCube";
        public override bool UsesAnalogStick => true;

        // =====================================================================
        // Core options
        // =====================================================================
        public override List<(string key, string label)> GetVisualOptions() => new()
        {
            ("dolphin_efb_scale", "Internal Resolution"),
            ("dolphin_max_anisotropy", "Anisotropic Filtering"),
            ("dolphin_anti_aliasing", "Anti-Aliasing"),
        };

        public override Dictionary<string, string> GetDefaultCoreOptions() => new()
        {
            // Dual-core + fastmem: the original single-core/no-fastmem safety config was a
            // Windows-era caution (SEH/VEH conflicts with .NET) that does NOT apply on Linux —
            // Dolphin's SIGSEGV fastmem handler chains cleanly with CoreCLR's, and the CPU
            // thread never touches GL so it needs no shared context. A/B on Mario Kart DD
            // attract race (RTX 3080 Ti, 2026-06-07): single-core no-fastmem 42-46fps →
            // dual-core 54-58 → dual-core+fastmem locked 60. If a game crashes on launch,
            // flip these three in Preferences → Core Options (values store overrides these).
            ["dolphin_main_cpu_thread"]        = "enabled",
            ["dolphin_fastmem"]                = "enabled",
            ["dolphin_fastmem_arena"]          = "enabled",
            // dolphin_cpu_core: NOT pre-seeded — OnVariableAnnounced picks the safest
            // non-JIT value from the core's own valid list at runtime
            ["dolphin_dsp_jit"]                = "disabled",
            ["dolphin_dsp_enable_jit"]         = "disabled",
            ["dolphin_dsp_hle"]                = "enabled",
            ["dolphin_skip_gc_bios"]           = "enabled",
            // Backend selection is via GET_PREFERRED_HW_RENDER (PreferredHwContext below), NOT this option
            // — this Dolphin build doesn't even expose dolphin_gfx_backend (only dolphin_renderer). Kept
            // for Linux builds that do read it; harmless where the key is absent. macOS picks Vulkan via
            // PreferredHwContext so Dolphin's Vulkan backend drives our libvkpresent HW-render path.
            ["dolphin_gfx_backend"]            = "OGL",
            ["dolphin_renderer"]               = "Hardware",
            // Shader compilation. macOS/Vulkan: mode 2 = Async (UberShaders) — render immediately with a
            // generic ubershader while specialized shaders compile on a BACKGROUND worker, so no stutter
            // and full speed. (Mode 1 forced sync-ubershaders compiling on the emu thread → ~9fps grind.)
            // Vulkan can spawn the compile worker; the GL path could NOT (surfaceless-EGL "Failed to create
            // shared context for shader compiling"), so off-macOS keeps mode 1 (sync ubershaders) to avoid
            // the specialized-mode-0 inline-compile 0-1fps slideshow (the Linux GameCube 1fps bug).
            ["dolphin_shader_compilation_mode"] = OperatingSystem.IsMacOS() ? "2" : "1",
            // Do NOT precompile the full uber set at boot: at this GPU's idle clocks that stalled
            // boot for 60s+. On-demand compiles amortize, and the global cache makes them one-time.
            ["dolphin_wait_for_shaders"]        = "disabled",
            // Internal resolution. NOT 1x: at native res Dolphin's framebuffer-indirection render lands
            // tucked in the bottom-left corner of our managed FBO. Filled correctly from ~3x up. Default
            // 4x (2560x2112) is a safe, sharp default; macOS holds locked 60 even at the core's max 6x
            // (3840x3168 / 4K+) on the M4 thanks to JITArm64 + the downscale readback — users can crank it
            // via the in-game "Internal Resolution" option (heavier titles may want to stay at 4x).
            ["dolphin_efb_scale"]              = "4",

            // ── Performance options ──────────────────────────────────────────
            // EFB copies to texture instead of RAM: avoids expensive VRAM→RAM→VRAM roundtrip.
            // This alone can be worth 20-30% for GC titles that use EFB effects heavily.
            ["dolphin_efb_copy_method"]        = "Texture",
            ["dolphin_efb_copy_to_texture"]    = "enabled",   // alternate key name (core version dependent)
            // Disable CPU reads from EFB — expensive synchronous stall, most games don't need it.
            ["dolphin_efb_access_enable"]      = "disabled",
            ["dolphin_efb_access"]             = "disabled",  // alternate key name
            // GPU-side texture decode — offloads format conversion from CPU to GPU.
            ["dolphin_gpu_texture_decode"]     = "enabled",
            // No texture filtering at 1x — eliminates bilinear sampling overhead.
            ["dolphin_texture_filtering"]      = "Nearest",
            // No anisotropic filtering.
            ["dolphin_max_anisotropy"]         = "0",
            // No MSAA.
            ["dolphin_video_multisampling"]    = "disabled",
            ["dolphin_msaa"]                   = "disabled",
            // Widescreen hack adds a clip-space transform pass — skip it.
            ["dolphin_widescreen_hack"]        = "disabled",
        };

        // =====================================================================
        // Controller ports
        // =====================================================================
        public override void ConfigureControllerPorts(LibretroCore core)
        {
            // Dolphin's BootCore (called inside retro_load_game) expects all 4 ports
            // to receive retro_set_controller_port_device before retro_run. Without
            // this, the input subsystem is left partially initialised and retro_run crashes.
            for (uint port = 0; port < 4; port++)
                core.SetControllerPortDevice(port, RETRO_DEVICE_JOYPAD);
        }

        // =====================================================================
        // CPU core mode — toggle at runtime via UseJit property
        // =====================================================================

        /// <summary>
        /// When true, selects JIT64 for full-speed emulation.
        /// (The old "JIT64 requires fastmem=disabled" note was the Windows SEH concern;
        /// on Linux JIT64 + fastmem chain signals cleanly — see the A/B note above.)
        /// If the game crashes on launch, set this back to false.
        /// </summary>
        public bool UseJit { get; set; } = true;

        public override void OnVariableAnnounced(string key, string[] validValues,
            Dictionary<string, string> coreOptions)
        {
            if (key != "dolphin_cpu_core" || validValues.Length == 0)
                return;

            string? pick;
            if (UseJit)
            {
                // Native recompiler — ~10-20x faster than the PowerPC interpreter. The right JIT is
                // ARCH-SPECIFIC and the core only lists the one valid for this build: JITArm64 ("4") on
                // Apple Silicon / arm64, JIT64 ("1") on x86-64. (The old "1"-only lookup fell through to
                // validValues[0]="0"=Interpreter on arm64 → the 9fps GameCube bug.) Values are numeric.
                pick = validValues.FirstOrDefault(v => v == "4")     // CPUCore::JITArm64 (Apple Silicon)
                    ?? validValues.FirstOrDefault(v => v == "1")     // CPUCore::JIT64 (x86-64)
                    ?? validValues.FirstOrDefault(v => v.IndexOf("jit", StringComparison.OrdinalIgnoreCase) >= 0);
            }
            else
            {
                // CachedInterpreter: safe fallback if JIT64 is unstable.
                pick = validValues.FirstOrDefault(v => v == "5")
                    ?? validValues.FirstOrDefault(v => v.IndexOf("cachedinterpreter",  StringComparison.OrdinalIgnoreCase) >= 0)
                    ?? validValues.FirstOrDefault(v => v.IndexOf("cached interpreter", StringComparison.OrdinalIgnoreCase) >= 0)
                    ?? validValues.FirstOrDefault(v => v == "0")
                    ?? validValues.FirstOrDefault(v => v.IndexOf("interpreter",        StringComparison.OrdinalIgnoreCase) >= 0
                                                    && v.IndexOf("jit",                StringComparison.OrdinalIgnoreCase) <  0);
            }

            string selected = pick ?? validValues[0];
            coreOptions[key] = selected;
            System.Diagnostics.Trace.WriteLine($"[GameCubeHandler] dolphin_cpu_core SELECT (UseJit={UseJit}): '{selected}' from [{string.Join(", ", validValues)}]");
        }

        // =====================================================================
        // D3D11 blocking around context_reset — Windows-only; no-op on Linux.
        // (No d3d11.dll exists; the Linux Dolphin core has no Direct3D backend.)
        // =====================================================================
        public override void OnBeforeContextReset() { /* no-op on Linux */ }
        public override void OnAfterContextReset()  { /* no-op on Linux */ }

        // Returned to Dolphin via GET_PREFERRED_HW_RENDER (env 56) — multi-backend cores pick their
        // renderer from it. macOS: Vulkan (6) over MoltenVK → our libvkpresent HW-render path (downscale
        // + async ring), since Dolphin's GL on Apple is degraded and can't pipeline glReadPixels.
        // Elsewhere: OpenGL Core (3).
        public override int PreferredHwContext => OperatingSystem.IsMacOS() ? 6 : 3;
        // With dolphin_main_cpu_thread=disabled Dolphin renders on retro_run's thread (our
        // _emuThread).  There is no separate Dolphin EmuThread, so we must NOT release the
        // GL context after context_reset — it must stay current for get_current_framebuffer
        // and retro_run.  Setting false keeps the context current and skips the N64-style
        // context release that caused ctx=0x0 on every GCF call.
        public override bool AllowHwSharedContext => false;
        // Dolphin creates its own EmuThread context against an internal DC, not ours,
        // so presenting our overlay back buffer shows an empty frame (black screen).
        // Use the same FBO-0 readback path as N64 instead.
        public override bool UseEmbeddedWindow => false;

        // GL overlay (cog menu, save/load slots, cheats panel) is incompatible
        // with rendering directly to FBO 0, so users who flip the AMD/Intel
        // compatibility option in Preferences trade the overlay for working
        // GameCube video.
        public override bool UseGLOverlay => !UseDefaultFramebuffer;

        public override bool UseDefaultFramebuffer =>
            App.Configuration?.GetEmulatorConfiguration().ResolveAmdIntelCompat() ?? false;

        // Remove the 1x and 2x Internal Resolution choices: at those scales dolphin_libretro's
        // OGL present centers its output in a believed-backbuffer smaller than our FBO, so the
        // game renders into the lower-left corner of the window (the low-res cornering bug —
        // see project_gamecube_lowres_cornering). It renders correctly from ~3x up; default is 4x.
        public override string[] FilterCoreOptionValues(string key, string[] values)
        {
            if (key == "dolphin_efb_scale")
                return values.Where(v => v.Trim() != "1" && v.Trim() != "2").ToArray();
            return values;
        }

        // Dolphin looks for dolphin-emu/Sys under the system directory. Linux places it beside the core
        // (coreDllDir). macOS keeps it in the standard libretro System dir (System/dolphin-emu/Sys), so
        // return the default there.
        public override string ResolveSystemDirectory(string defaultDir, string coreDllDir)
            => OperatingSystem.IsMacOS() ? defaultDir : coreDllDir;
    }
}
