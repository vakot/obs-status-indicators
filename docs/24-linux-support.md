# Linux support

The plugin supports Linux through the Qt platform backend already used by OBS. The Linux renderer receives copied layout/settings snapshots through queued Qt invocations.

On Wayland, when the compositor advertises `zwlr_layer_shell_v1`, the renderer uses a raw layer-shell surface on the overlay layer. This gives the compositor explicit top-level stacking, corner anchoring, no keyboard focus, and an empty input region, so the surface is not a normal Alt+Tab application window. Pixel-level opacity is applied to the shared-memory surface because Wayland does not provide the Qt window-opacity operation used by ordinary top-level windows.

On X11 and on Wayland compositors without layer-shell, the renderer falls back to a Qt frameless tool window with stays-on-top, no-focus, and click-through hints. These hints are best effort and remain subject to the desktop window manager.

Linux desktop environments intentionally retain authority over some window behavior:

- capture exclusion is not exposed through a portable Qt/Linux API, so the overlay may be included by screen capture;
- layer-shell is a compositor extension and is not available on every Wayland compositor;
- output selection for a layer surface created without an explicit `wl_output` is compositor-defined.

The renderer follows primary-screen geometry changes and uses the screen's global geometry, including non-zero origins in multi-monitor layouts. If no Qt application or primary screen is available, overlay startup fails without preventing the OBS plugin from loading.

Build Linux with the `ubuntu-x86_64` CMake preset and the OBS/libobs, obs-frontend-api, Qt 6, Wayland client, and `wayland-scanner` development packages supplied by the host or OBS dependency environment. Wayland support is compiled when the Wayland client library and scanner are available; otherwise the Qt fallback remains available.
