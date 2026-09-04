# Development conventions

## Boundaries and naming

- Use `ObsStateProvider` for OBS-specific acquisition and references.
- Use `IndicatorController` for precedence, normalization, transient state, and desired layout.
- Use `WindowsOverlayRenderer` for HWND/thread/GDI/rendering operations only.
- Name state fields after facts (`recordingPaused`, `microphoneMuted`) rather than visual labels.
- Keep renderer inputs value-based; do not expose OBS pointers to the renderer.

## C++ and build

- Follow the current OBS plugin-template CMake/toolchain conventions.
- Prefer the project’s existing compiler warnings and formatting configuration once the skeleton is added.
- Link only `libobs` and `obs-frontend-api` plus Windows system libraries required by the renderer.
- Do not add Qt, a browser, a socket client, a game hook, or another dependency for the POC.
- Keep Windows-specific code under a clearly named Windows overlay boundary; do not create unused cross-platform abstractions.

## Ownership and lifetime

- Every `obs_*_get_*` reference is paired with the documented release call.
- Every signal/callback registration has a matching removal on unload.
- The renderer owns its HWND, DIB/font resources, timers, thread, and message loop.
- Teardown order is: stop accepting work → unregister callbacks/signals → cancel timers → close/join UI thread → release objects.
- Use weak source references or re-resolution when a microphone source can be removed/replaced.

## Threading

- Treat OBS callbacks as unsuitable for direct Win32 operations.
- Publish copied state snapshots to the overlay thread.
- Perform all HWND/layout/paint operations on the overlay thread.
- Use one narrow synchronization mechanism; do not add a general task system.
- Timers must be owned by the controller/renderer boundary, cancellable, and unable to target destroyed state.

## Error handling and logging

- Overlay initialization, capture-affinity setup, and microphone resolution are optional failures; log and isolate them.
- Do not let exceptions or fatal error paths cross OBS callback boundaries.
- Use OBS logging with the prefix `[obs-status-indicators]`.
- Log lifecycle and capability events: load, unload, overlay init/failure, affinity enabled/failed, signal registration failure, microphone unavailable.
- Avoid logging every normal transition after the POC diagnostics are complete.
- Never log credentials, source-sensitive data, or full user paths unless needed for a local diagnostic.

## Scope control

Keep v0.0.1 fixed: primary/default display position, vertical stack, small readable high-contrast styling, and no settings UI. Defer configuration, animation, theming, monitor selection, opacity controls, and other platforms until the core acceptance matrix passes.
