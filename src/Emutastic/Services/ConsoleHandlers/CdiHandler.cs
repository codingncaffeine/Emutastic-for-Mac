using System.Collections.Generic;
using System.IO;

namespace Emutastic.Services.ConsoleHandlers
{
    /// <summary>
    /// Handler for Philips CD-i (SAME CDi / MAME).
    ///
    /// SAME CDi sources confirm:
    ///   - retro_set_controller_port_device is a no-op stub — ignore it.
    ///   - Thumbpad cursor is driven via RETRO_DEVICE_MOUSE queries on port 0.
    ///   - The mouse path is gated by the "same_cdi_mouse_enable" core option
    ///     which defaults to "disabled" — we must override it to "enabled".
    ///   - JOYPAD is also polled on port 0 in parallel (buttons + d-pad still work).
    /// </summary>
    public class CdiHandler : ConsoleHandlerBase
    {
        public override string ConsoleName => "CDi";
        public override bool UsesAnalogStick => true;
        public override bool PromoteAnalogStickToDpad => true;

        public override Dictionary<string, string> GetDefaultCoreOptions()
            => new()
            {
                // Enables the MAME mouse-input path that drives the CD-i thumbpad cursor.
                // Without this the core ignores all RETRO_DEVICE_MOUSE queries.
                ["same_cdi_mouse_enable"] = "enabled",
            };

        // The known-good CD-i BIOS archives (any one is enough for SAME CDi to boot a disc).
        private static readonly string[] BiosZips = { "cdibios.zip", "cdimono1.zip", "cdimono2.zip" };

        /// <summary>
        /// SAME CDi reads the CD-i BIOS from its MAME rompath, which is &lt;System&gt;/same_cdi/bios/.
        /// We document "put the BIOS in the System folder", so bridge the two: copy any CD-i BIOS the
        /// user dropped at the System root into same_cdi/bios/ where the core actually looks. Copy
        /// (not move) so the documented location keeps working; idempotent (size-checked).
        /// </summary>
        public override void PrepareSystemDirectory(string systemDir)
        {
            try
            {
                string biosDir = Path.Combine(systemDir, "same_cdi", "bios");
                Directory.CreateDirectory(biosDir);
                foreach (string name in BiosZips)
                {
                    string src = Path.Combine(systemDir, name);
                    if (!File.Exists(src)) continue;
                    string dst = Path.Combine(biosDir, name);
                    if (!File.Exists(dst) || new FileInfo(dst).Length != new FileInfo(src).Length)
                        File.Copy(src, dst, overwrite: true);
                }
            }
            catch { /* best-effort; if it still can't find the BIOS the core reports it (now visible in logs) */ }
        }
    }
}
