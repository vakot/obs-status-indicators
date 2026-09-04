# Phase 4 — Indicator controller and atomic layout

## Result

Phase 4 is **possible** and implemented. The controller converts each copied `IndicatorState` into a complete ordered layout, while the renderer consumes that value on its UI thread and applies the new surface, size, position, and visibility together.

## State and precedence

`IndicatorState` now includes the five product facts needed by the MVP: recording, recording pause, Replay Buffer, microphone availability/mute state, and saving. The microphone and saving fields are intentionally defaulted until their OBS-specific providers are implemented in Phase 5.

The current deterministic order is:

1. `PAUSED` or `REC` — pause replaces the normal recording entry.
2. `REPLAY`.
3. `MIC` — omitted when the microphone is unavailable.
4. `SAVING` — independent of recording and Replay Buffer state.

The controller emits no placeholder rows, so removing the first, middle, or last entry produces a compact stack with no intermediate gap state.

## Threading and rendering

`WindowsOverlayRenderer::update_layout` copies the layout under its mutex and posts one private message to the renderer thread. That thread takes the latest snapshot, draws every row into one premultiplied-alpha DIB, calls `UpdateLayeredWindow`, resizes/repositions the topmost popup, and shows or hides it as one operation. OBS callbacks therefore never touch an HWND or GDI resource.

## Verification

`tests/indicator-controller-test.cpp` covers the empty state, recording, pause precedence, the full four-entry currently representable combination, and removal of first/middle/last entries. The test is registered with CTest and does not require OBS, Qt, or a Windows overlay.
