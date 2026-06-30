using System;
using System.Linq;
using Avalonia.Controls;
using Avalonia.Threading;
using Emutastic.Services;

namespace Emutastic.Views
{
    /// <summary>
    /// Phase 3 validation harness for the native single-window EmuTV design. A plain window hosting a
    /// <see cref="GameSurfaceView"/>, into which a game is launched EMBEDDED (headless game-host rendering
    /// into shared IOSurfaces). Proves the game renders INSIDE one Avalonia window — no second window — before
    /// EmuTV itself is wired up (Phase 5). Run with: <c>EMUTASTIC_EMBED_TEST=1 Emutastic &lt;core&gt; &lt;rom&gt;</c>.
    /// </summary>
    public sealed class EmbedTestWindow : Window
    {
        private readonly GameSurfaceView _view = new();
        private readonly string _core, _rom;
        private Action<string, string>? _cmdHandler;

        public EmbedTestWindow(string core, string rom)
        {
            _core = core; _rom = rom;
            Title = "EmuTV embed test";
            Width = 960; Height = 540;
            Background = Avalonia.Media.Brushes.Black;
            Content = _view;
            Opened += OnOpened;
            Closed += OnClosed;
        }

        private void OnOpened(object? sender, EventArgs e)
        {
            // The game-host announces its ring + control ids once (EMUTASTIC-CMD iosurface <w>x<h> <ctrl> <ids>).
            _cmdHandler = (verb, arg) =>
            {
                if (verb != "iosurface") return;
                try
                {
                    var p = arg.Split(' ');             // "<w>x<h> <ctrl> <input> <id0>,<id1>,<id2>"
                    var wh = p[0].Split('x');
                    int w = int.Parse(wh[0]), h = int.Parse(wh[1]);
                    uint ctrl = uint.Parse(p[1]);
                    uint input = uint.Parse(p[2]);
                    uint[] ids = p[3].Split(',').Select(uint.Parse).ToArray();
                    _view.Bind(w, h, ctrl, input, ids);
                    System.Diagnostics.Trace.WriteLine($"[EmbedTest] bound {w}x{h} ctrl={ctrl} input={input} ids=[{string.Join(",", ids)}]");
                }
                catch (Exception ex) { System.Diagnostics.Trace.WriteLine($"[EmbedTest] parse failed: {ex}"); }
            };
            GameHostLauncher.OnHostCommand += _cmdHandler;
            GameHostLauncher.Launch(_core, _rom, "", null, null,
                onExit: _ => Dispatcher.UIThread.Post(() => { try { _view.Unbind(); Close(); } catch { } }),
                fullscreen: false, embedded: true);
        }

        private void OnClosed(object? sender, EventArgs e)
        {
            if (_cmdHandler != null) { GameHostLauncher.OnHostCommand -= _cmdHandler; _cmdHandler = null; }
            _view.Unbind();
        }
    }
}
