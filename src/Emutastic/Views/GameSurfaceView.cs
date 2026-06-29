using System;
using Avalonia.Controls;
using Avalonia.Platform;
using Avalonia.Threading;
using Emutastic.Platform;

namespace Emutastic.Views
{
    /// <summary>
    /// macOS embedded game view (Phase 3 of the native single-window EmuTV design). A layer-backed NSView
    /// hosted inside an Avalonia window via <see cref="NativeControlHost"/>; its CALayer displays the latest
    /// game frame from the headless game-host's shared IOSurface ring. The game thus renders INSIDE the
    /// EmuTV window — no second window, no focus handoff, OS-hidden chrome (EmuTV is genuinely fullscreen).
    ///
    /// The game-host announces the ring + control ids once (EMUTASTIC-CMD iosurface ...); <see cref="Bind"/>
    /// looks them up and starts a UI-thread poll that reads the control mailbox each tick and points the
    /// layer at the freshest finished slot. Everything here runs on the UI (main) thread — required for
    /// NSView/CALayer mutation on macOS.
    /// </summary>
    public sealed class GameSurfaceView : NativeControlHost
    {
        private IntPtr _view;     // NSView*
        private IntPtr _layer;    // CALayer*
        private DispatcherTimer? _poll;

        // Bound ring state.
        private IOSurfaceInterop.IOSurface?[] _ring = Array.Empty<IOSurfaceInterop.IOSurface?>();
        private IOSurfaceInterop.IOSurface? _control;
        private IntPtr _controlBase;
        private long _lastSeq = -1;
        private int _curSlot = -1;

        protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
        {
            if (OperatingSystem.IsMacOS())
            {
                _view = IOSurfaceInterop.HostView.CreateView();
                _layer = IOSurfaceInterop.HostView.ViewLayer(_view);
                if (_view != IntPtr.Zero) return new PlatformHandle(_view, "NSView");
            }
            return base.CreateNativeControlCore(parent);   // non-macOS / failure: empty host
        }

        protected override void DestroyNativeControlCore(IPlatformHandle control)
        {
            StopPoll();
            if (_view != IntPtr.Zero) { IOSurfaceInterop.HostView.DestroyView(_view); _view = IntPtr.Zero; _layer = IntPtr.Zero; }
            else base.DestroyNativeControlCore(control);
        }

        /// <summary>
        /// Start displaying the game-host's ring. <paramref name="ringIds"/> are the global IOSurface ids of
        /// the render ring; <paramref name="controlId"/> is the mailbox surface carrying the latest slot|seq.
        /// Idempotent: a re-bind (new launch) replaces the previous binding. UI thread.
        /// </summary>
        public void Bind(int width, int height, uint controlId, uint[] ringIds)
        {
            Dispatcher.UIThread.VerifyAccess();
            Unbind();
            if (!OperatingSystem.IsMacOS() || _layer == IntPtr.Zero || ringIds.Length == 0) return;

            _ring = new IOSurfaceInterop.IOSurface?[ringIds.Length];
            for (int i = 0; i < ringIds.Length; i++) _ring[i] = IOSurfaceInterop.IOSurface.Lookup(ringIds[i]);
            _control = controlId != 0 ? IOSurfaceInterop.IOSurface.Lookup(controlId) : null;
            _controlBase = _control != null ? _control.Lock(true) : IntPtr.Zero;   // stable mapped base for polling
            _lastSeq = -1; _curSlot = -1;

            // Poll faster than any display refresh (≈125 Hz) so we never lag a produced frame; the actual
            // present cadence is still the WindowServer compositing the layer at vsync. Reading the mailbox
            // is one atomic int64; we only touch the layer when the sequence advances.
            _poll = new DispatcherTimer(DispatcherPriority.Render) { Interval = TimeSpan.FromMilliseconds(8) };
            _poll.Tick += (_, _) => Pump();
            _poll.Start();
        }

        public void Unbind()
        {
            StopPoll();
            if (_control != null) { _control.Unlock(true); _control.Dispose(); _control = null; _controlBase = IntPtr.Zero; }
            foreach (var s in _ring) s?.Dispose();
            _ring = Array.Empty<IOSurfaceInterop.IOSurface?>();
            _lastSeq = -1; _curSlot = -1;
        }

        private void StopPoll() { _poll?.Stop(); _poll = null; }

        private void Pump()
        {
            if (_layer == IntPtr.Zero || _controlBase == IntPtr.Zero) return;
            var (slot, seq) = IOSurfaceInterop.ReadMailbox(_controlBase);
            if (seq == _lastSeq || slot < 0 || slot >= _ring.Length) return;   // nothing new (or not ready yet)
            var surf = _ring[slot];
            if (surf == null) return;
            _lastSeq = seq;
            if (slot != _curSlot)   // only re-point the layer when the slot actually changes
            {
                _curSlot = slot;
                IOSurfaceInterop.HostView.SetSurface(_layer, surf.Handle);
            }
            else
            {
                // Same slot, new seq (child re-rendered into it): nudge CoreAnimation to recomposite.
                IOSurfaceInterop.HostView.SetSurface(_layer, surf.Handle);
            }
        }
    }
}
