using System;
using System.Runtime.InteropServices;
using Avalonia.Threading;

namespace Emutastic.Platform
{
    /// <summary>
    /// Shared layout of the input mailbox IOSurface (Phase 4 of the native single-window EmuTV design).
    /// In embedded mode the headless game-host is NOT the active app, so on macOS it can't reliably read
    /// the controller (GameController delivers to the active app only). So the PARENT (EmuTV, active) reads
    /// the controllers and writes their raw SDL-gamepad state here; the child applies it through its normal
    /// mapping. Plain CPU shared memory; an aligned int32 seq makes torn reads detectable.
    ///
    /// Layout (little-endian): [0]=int32 seq, [4]=int32 connectedCount, then 4 ports × 16 bytes from offset
    /// 8: [+0]=uint32 button mask (bit b = SDL gamepad button b, 0..20), [+4]=6×int16 axes
    /// (LEFTX,LEFTY,RIGHTX,RIGHTY,LTRIG,RTRIG).
    /// </summary>
    public static class InputMailbox
    {
        public const int MaxPorts = 4;
        public const int Axes = 6;
        public const int PortStride = 16;     // 4 (mask) + 12 (6×int16)
        public const int HeaderBytes = 8;     // seq + count
        public const int SizeBytes = HeaderBytes + MaxPorts * PortStride;   // 72
        public static int PortOffset(int p) => HeaderBytes + p * PortStride;
    }

    /// <summary>
    /// Parent-side controller reader/forwarder. Opens the SDL gamepads in the PARENT process (the active app,
    /// which actually receives controller input on macOS) and, each UI-thread tick, writes their raw state
    /// into the input mailbox surface for the embedded child to consume. UI thread only (SDL gamepad pump is
    /// main-thread-only on macOS). Started by the host view when a game is embedded; stopped on unbind.
    /// </summary>
    public sealed class ControllerForwarder : IDisposable
    {
        const string SDL = "SDL3";
        const uint SDL_INIT_GAMEPAD = 0x00002000;
        [DllImport(SDL)] [return: MarshalAs(UnmanagedType.I1)] static extern bool SDL_InitSubSystem(uint flags);
        [DllImport(SDL)] static extern void SDL_PumpEvents();
        [DllImport(SDL)] static extern void SDL_UpdateGamepads();
        [DllImport(SDL)] static extern IntPtr SDL_GetGamepads(out int count);
        [DllImport(SDL)] static extern IntPtr SDL_OpenGamepad(uint instanceId);
        [DllImport(SDL)] static extern void SDL_CloseGamepad(IntPtr gamepad);
        [DllImport(SDL)] [return: MarshalAs(UnmanagedType.I1)] static extern bool SDL_GetGamepadButton(IntPtr gamepad, int button);
        [DllImport(SDL)] static extern short SDL_GetGamepadAxis(IntPtr gamepad, int axis);
        [DllImport(SDL)] static extern void SDL_free(IntPtr mem);

        private readonly IntPtr _base;                       // mapped base of the input mailbox surface
        private readonly IOSurfaceInterop.IOSurface _surface; // kept alive + write-locked while forwarding
        private readonly IntPtr[] _pads = new IntPtr[InputMailbox.MaxPorts];
        private DispatcherTimer? _timer;
        private int _seq;

        private ControllerForwarder(IOSurfaceInterop.IOSurface surface, IntPtr basePtr)
        { _surface = surface; _base = basePtr; }

        /// <summary>Look up the mailbox surface by id and start forwarding. Null if the surface isn't found.</summary>
        public static ControllerForwarder? Start(uint inputSurfaceId)
        {
            if (!OperatingSystem.IsMacOS() || inputSurfaceId == 0) return null;
            SDL_InitSubSystem(SDL_INIT_GAMEPAD);
            var surf = IOSurfaceInterop.IOSurface.Lookup(inputSurfaceId);
            if (surf == null) return null;
            var basePtr = surf.Lock(false);   // write-mapped for the lifetime of forwarding
            var fwd = new ControllerForwarder(surf, basePtr);
            fwd._timer = new DispatcherTimer(DispatcherPriority.Input) { Interval = TimeSpan.FromMilliseconds(8) };
            fwd._timer.Tick += (_, _) => fwd.Pump();
            fwd._timer.Start();
            return fwd;
        }

        private void OpenPads()
        {
            for (int i = 0; i < _pads.Length; i++) { if (_pads[i] != IntPtr.Zero) { SDL_CloseGamepad(_pads[i]); _pads[i] = IntPtr.Zero; } }
            IntPtr arr = SDL_GetGamepads(out int count);
            for (int i = 0; i < count && i < InputMailbox.MaxPorts; i++)
            {
                uint id = (uint)Marshal.ReadInt32(arr, i * 4);
                _pads[i] = SDL_OpenGamepad(id);
            }
            if (arr != IntPtr.Zero) SDL_free(arr);
        }

        private int _rescan;
        private unsafe void Pump()
        {
            if (_base == IntPtr.Zero) return;
            SDL_PumpEvents();
            SDL_UpdateGamepads();
            // Re-enumerate occasionally (cheap) so a controller connected mid-game is picked up.
            if (_rescan-- <= 0 || NoPads()) { OpenPads(); _rescan = 120; }

            byte* b = (byte*)_base;
            int connected = 0;
            for (int p = 0; p < InputMailbox.MaxPorts; p++)
            {
                IntPtr h = _pads[p];
                int off = InputMailbox.PortOffset(p);
                if (h == IntPtr.Zero) { *(uint*)(b + off) = 0; for (int a = 0; a < InputMailbox.Axes; a++) *(short*)(b + off + 4 + a * 2) = 0; continue; }
                connected++;
                uint mask = 0;
                for (int btn = 0; btn < 21; btn++) if (SDL_GetGamepadButton(h, btn)) mask |= 1u << btn;
                *(uint*)(b + off) = mask;
                for (int a = 0; a < InputMailbox.Axes; a++) *(short*)(b + off + 4 + a * 2) = SDL_GetGamepadAxis(h, a);
            }
            *(int*)(b + 4) = connected;
            System.Threading.Volatile.Write(ref *(int*)b, ++_seq);   // publish last (seq bump = "new frame ready")
        }

        private bool NoPads() { foreach (var p in _pads) if (p != IntPtr.Zero) return false; return true; }

        public void Dispose()
        {
            _timer?.Stop(); _timer = null;
            for (int i = 0; i < _pads.Length; i++) if (_pads[i] != IntPtr.Zero) { SDL_CloseGamepad(_pads[i]); _pads[i] = IntPtr.Zero; }
            try { _surface.Unlock(false); _surface.Dispose(); } catch { }
        }
    }
}
