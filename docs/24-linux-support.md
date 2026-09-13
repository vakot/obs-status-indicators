# Linux support

The plugin supports Linux through the Qt platform backend already used by OBS. The Linux renderer owns a single top-level `QWidget` on the Qt GUI thread and receives copied layout/settings snapshots through queued Qt invocations.

The overlay uses Qt's platform-neutral hints for a frameless tool window, translucency, no focus, click-through input, and compositor-managed topmost placement. It does not use Xlib, X11-only window calls, a desktop-wide polling timer, or global window hooks, so the same implementation can run under X11 and Wayland.

Linux desktop environments intentionally retain authority over some window behavior:

- capture exclusion is not exposed through a portable Qt/Linux API, so the overlay may be included by screen capture;
- `WindowStaysOnTopHint` is best effort and can be restricted by a Wayland compositor;
- input transparency and stacking behavior depend on the active Qt platform plugin and compositor policy.

The renderer follows primary-screen geometry changes and uses the screen's global geometry, including non-zero origins in multi-monitor layouts. If no Qt application or primary screen is available, overlay startup fails without preventing the OBS plugin from loading.

Build Linux with the `ubuntu-x86_64` CMake preset and the OBS/libobs, obs-frontend-api, and Qt 6 development packages supplied by the host or OBS dependency environment.
