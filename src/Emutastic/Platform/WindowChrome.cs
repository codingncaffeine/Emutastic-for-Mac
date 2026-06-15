using Avalonia;
using Avalonia.Controls;

namespace Emutastic.Platform;

/// <summary>
/// Native-OS window chrome toggle. When the user enables "native window controls"
/// (<c>ThemeConfiguration.UseWindowsChrome</c>), the app's custom frameless windows switch to the
/// real OS title bar + minimize/maximize/close — on macOS that's the native traffic lights at top-left.
/// Otherwise the custom borderless chrome (rounded, shadowed shell + drawn buttons) is kept.
/// </summary>
internal static class WindowChrome
{
    /// <summary>True when the user has opted into native OS window decorations.</summary>
    public static bool NativeEnabled =>
        App.Configuration?.GetThemeConfiguration()?.UseWindowsChrome == true;

    /// <summary>
    /// Switch <paramref name="window"/> to native OS chrome when <see cref="NativeEnabled"/> is set:
    /// show the system title bar, hide the custom title bar (and collapse its grid row), flatten the
    /// rounded/shadowed shell, and turn off window transparency so the OS draws an opaque framed window.
    /// Returns <c>true</c> when native chrome was applied — the caller should then SKIP its custom-chrome
    /// wiring (edge-resize + title-bar drag), which the OS now handles.
    /// </summary>
    public static bool ApplyIfEnabled(Window window, Grid? customTitleBar, Grid? rootGrid,
                                      int titleRowIndex, Border? outerBorder, Border? innerClip)
    {
        if (!NativeEnabled)
            return false;

        window.WindowDecorations = WindowDecorations.Full;
        window.TransparencyLevelHint = new[] { WindowTransparencyLevel.None };

        if (customTitleBar is not null)
            customTitleBar.IsVisible = false;
        if (rootGrid is not null && titleRowIndex >= 0 && titleRowIndex < rootGrid.RowDefinitions.Count)
            rootGrid.RowDefinitions[titleRowIndex].Height = new GridLength(0);
        if (outerBorder is not null)
        {
            window.Background = outerBorder.Background;   // opaque (was Transparent for the frameless shell)
            outerBorder.BorderThickness = new Thickness(0);
            outerBorder.CornerRadius = new CornerRadius(0);
            outerBorder.Effect = null;                    // drop the floating-window shadow
        }
        if (innerClip is not null)
            innerClip.CornerRadius = new CornerRadius(0);

        return true;
    }
}
