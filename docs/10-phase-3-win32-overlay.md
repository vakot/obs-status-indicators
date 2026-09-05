# Phase 3 - Win32 overlay foundation

## Result

Phase 3 is **possible**. The plugin owns a dedicated Win32 UI thread and a borderless topmost popup that renders the active indicators through a 32-bit DIB and `UpdateLayeredWindow`.

## Window contract

The overlay uses `WS_POPUP` with `WS_EX_LAYERED`, `WS_EX_TRANSPARENT`, `WS_EX_NOACTIVATE`, and `WS_EX_TOOLWINDOW`. It reports `HTTRANSPARENT` for `WM_NCHITTEST` and `MA_NOACTIVATE` for `WM_MOUSEACTIVATE`, so it has no intended input/focus ownership. Each active indicator is a 48x48 black square at 80% opacity with an 8-pixel icon inset; active squares are stacked vertically with an 8-pixel transparent gap and positioned at the primary display's exact top-left origin `(0, 0)`.

`HWND_TOPMOST` places the window in the topmost band but does not permanently fix its order relative to other topmost windows. Start11 and similar tools can move their own topmost panel ahead after interaction. The renderer therefore registers out-of-context `SetWinEventHook` listeners for foreground and top-level show/hide/reorder events. These callbacks only coalesce and post a private message; the overlay UI thread then reasserts `SetWindowPos(HWND_TOPMOST, ..., SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)`. This is event-driven recovery with no periodic timer or 1-2 second polling loop. Hook registration is nonfatal so the overlay remains usable if the capability is unavailable.

All window creation, painting, positioning, and destruction happen on the overlay thread. The plugin unload order destroys the overlay before destroying the OBS state provider.

## Rendering and capture evidence

The indicators are rendered into a premultiplied-alpha-compatible 32-bit DIB using Lucide SVG assets through Qt6Svg and published with `UpdateLayeredWindow`. Pixels outside the indicator squares are fully transparent; the `UpdateLayeredWindow` source alpha applies 80% opacity to each complete indicator tile, including both its black background and Lucide glyph. Recording, pause, Replay Buffer, microphone, and saving each have a distinct glyph; muted microphone uses Lucide's `mic-off` glyph. OBS 32.2.1 logged:

- `capture exclusion enabled for overlay`.
- `overlay window ready at primary-display top-left`.
- `topmost order recovery hooks enabled`.

Window inspection during the run found the visible `OBSStatusIndicatorsOverlay` at the primary-display top-left origin and returned `HTTRANSPARENT`. The desktop screenshot did not contain the indicator while the foreground game was displayed; this is consistent with the active `WDA_EXCLUDEFROMCAPTURE` behavior and is evidence that the capture API excluded the overlay from that screenshot. It is not evidence that every capture path or exclusive-fullscreen compositor will exclude it.

## Runtime limitation

The test monitor is scaled, so physical `GetWindowRect` coordinates report a scaled size while the renderer uses logical dimensions. DPI awareness and final visual sizing should be revisited before adding settings or monitor-selection logic.

## Shutdown evidence

The renderer's `stop()` posts `WM_QUIT` to the UI thread and joins it; the UI thread destroys its HWND and unregisters its class. The supplied fixture's tray configuration prevented a clean OBS process exit in the automated harness, so the process was cleaned up as one exact test process after the window-style/affinity assertions. A user-driven clean-exit test remains part of Phase 7.
