# Windows overlay approach

## Recommended window

Create one compact, top-level popup window per process, owned by the plugin and created on the renderer UI thread.

| Requirement | Implementation choice |
| --- | --- |
| Borderless | `WS_POPUP`, no non-client frame |
| Transparent/composited | `WS_EX_LAYERED` plus `UpdateLayeredWindow` with premultiplied alpha |
| Topmost | `SetWindowPos(HWND_TOPMOST, ..., SWP_NOACTIVATE)` |
| Non-activating | `WS_EX_NOACTIVATE`, `SWP_NOACTIVATE`, do not call foreground APIs |
| Click-through | `WS_EX_TRANSPARENT`; return `HTTRANSPARENT` from `WM_NCHITTEST` as a defensive measure |
| No taskbar/Alt-Tab | `WS_EX_TOOLWINDOW`, no `WS_EX_APPWINDOW` |
| Capture exclusion | `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` after creation |

Microsoft documents that a layered window with `WS_EX_TRANSPARENT` passes mouse events through and that `WS_EX_NOACTIVATE` prevents normal foreground activation: [window features](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features) and [extended window styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles). `HTTRANSPARENT` is documented under [WM_NCHITTEST](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest).

## Rendering choice

Use GDI to draw the small set of text/icon rows into a 32-bit premultiplied-alpha DIB and publish the whole surface with `UpdateLayeredWindow`.

Why this is the right POC choice:

- It is native to Windows and requires no Qt, browser, GPU hook, or added runtime.
- It directly supports alpha composition and a compact window.
- It makes atomic updates straightforward: build the new surface, calculate the new size/position, and submit one update.
- The indicator count and update frequency are too small to justify a rendering framework.

Keep drawing code separate from layout code. The layout produces row rectangles and the renderer paints those rectangles. Do not issue one asynchronous move/hide operation per row.

## Position and sizing

- Use a fixed 8 px edge offset for the MVP.
- Use the primary monitor as the documented default, or the monitor containing the OBS main window if the implementation can obtain it without pulling Qt into the renderer.
- Measure the exact visible rows, add fixed padding, and size the window to that content.
- On a display/work-area change, recompute the target rectangle and apply one reposition/update operation.
- Do not add user-configurable monitor or position settings in v0.0.1.

## Capture exclusion

Call:

```cpp
SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
```

The call is valid for a top-level window owned by the current process, which is satisfied when the window is created directly by the OBS plugin inside OBS. Check the return value. A failure logs a warning and leaves the overlay operational.

Windows supports `WDA_EXCLUDEFROMCAPTURE` starting with Windows 10 version 2004; older Windows 10 versions treat it as `WDA_MONITOR`: [SetWindowDisplayAffinity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity).

This is not a universal capture contract. The finished MVP must record actual results for:

1. OBS Display Capture of the monitor containing the overlay.
2. OBS Window Capture of a target application beneath/near the overlay.
3. OBS Game Capture of a practical test application.

For each, verify both an OBS recording frame and an OBS screenshot/preview where applicable. A successful API call alone is insufficient. Window Capture may not include an unrelated top-level overlay by design, while Game Capture can use a different capture path; these are hypotheses to verify, not promises.

Microsoft also explicitly states that display affinity is not a security/DRM guarantee. The product wording should therefore say “best-effort exclusion from supported Windows capture paths,” not “invisible to all capture.”

## Fullscreen and other overlays

Normal and maximized desktop applications, borderless fullscreen, and practical exclusive fullscreen must be separate test cases. A topmost compositor window can remain visible over normal and borderless modes but cannot be guaranteed above every exclusive presentation path.

Do not add injection, swap-chain hooks, game-process code, or compatibility layers for that limitation. Record it as a platform constraint. Test Discord, Steam, and NVIDIA overlays for obvious focus/input conflicts only; no ordering guarantee is in scope.
