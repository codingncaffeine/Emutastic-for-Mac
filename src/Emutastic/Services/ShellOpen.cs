using System;
using System.Diagnostics;

namespace Emutastic.Services
{
    /// <summary>
    /// Opens a file, folder, or URL with the desktop's default handler — macOS <c>open</c>, else
    /// <c>xdg-open</c>. Always pass the target via ArgumentList — the Process.Start(file, arguments)
    /// overload treats the second string as a RAW argument line, so a path with spaces
    /// ("Manuals/NES/Super Mario Bros [811b027e]/manual.pdf") got split into four
    /// arguments and the opener silently opened nothing.
    /// </summary>
    public static class ShellOpen
    {
        // macOS ships `open`; Linux desktops ship `xdg-open`. (Windows would be the shell-execute path,
        // but this port targets macOS + Linux.)
        private static string Opener => OperatingSystem.IsMacOS() ? "open" : "xdg-open";

        public static void Open(string target)
        {
            try
            {
                var psi = new ProcessStartInfo(Opener) { UseShellExecute = false };
                psi.ArgumentList.Add(target);
                Process.Start(psi);
            }
            catch (Exception ex) { Trace.WriteLine($"[ShellOpen] {target}: {ex.Message}"); }
        }

        /// <summary>Open the file manager with the given FILE selected — the analog of upstream's
        /// <c>explorer.exe /select</c>. macOS uses <c>open -R</c> (reveal in Finder). Linux tries the
        /// freedesktop FileManager1 D-Bus interface (Dolphin/Nautilus highlight the file) and falls back
        /// to opening the containing folder when no file manager implements it. Fire-and-forget.</summary>
        public static void ShowInFolder(string filePath)
        {
            if (OperatingSystem.IsMacOS())
            {
                try
                {
                    var psi = new ProcessStartInfo("open") { UseShellExecute = false };
                    psi.ArgumentList.Add("-R");        // reveal + select in Finder
                    psi.ArgumentList.Add(filePath);
                    Process.Start(psi);
                }
                catch (Exception ex) { Trace.WriteLine($"[ShellOpen] reveal({filePath}): {ex.Message}"); }
                return;
            }

            _ = System.Threading.Tasks.Task.Run(() =>
            {
                try
                {
                    string uri = new System.Uri(filePath).AbsoluteUri;
                    var psi = new ProcessStartInfo("dbus-send") { UseShellExecute = false, RedirectStandardError = true };
                    foreach (var a in new[]
                    {
                        "--session", "--print-reply", "--dest=org.freedesktop.FileManager1",
                        "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1.ShowItems",
                        $"array:string:{uri}", "string:",
                    })
                        psi.ArgumentList.Add(a);
                    using var p = Process.Start(psi);
                    if (p != null && p.WaitForExit(3000) && p.ExitCode == 0) return;
                }
                catch (Exception ex) { Trace.WriteLine($"[ShellOpen] ShowItems({filePath}): {ex.Message}"); }
                string? dir = System.IO.Path.GetDirectoryName(filePath);
                if (!string.IsNullOrEmpty(dir)) Open(dir);
            });
        }
    }
}
