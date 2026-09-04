# Phase 7 - Stability, packaging, and release readiness

## Result

The MVP is **possible and release-ready for the documented native Windows scope**. The RelWithDebInfo artifact builds, the portable OBS install loads it, and repeated lifecycle runs completed without a stale overlay thread, duplicate plugin load, or lingering OBS process.

## Build and package verification

Executed on Windows 11 Pro 25H2 x64 with Visual Studio Build Tools 2022:

1. Built the `windows-x64` RelWithDebInfo target.
2. Ran the Visual Studio `RUN_TESTS` target; the `indicator-controller` test passed.
3. Installed the artifact into the supplied `obs-dev` portable installation.
4. Confirmed the runtime DLL at `obs-dev/obs-plugins/64bit/obs-status-indicators.dll` and the locale at `obs-dev/data/obs-plugins/obs-status-indicators/locale/en-US.ini`.

CMake also emits the template-compatible staging copy under `obs-dev/obs-status-indicators/bin/64bit`. The direct `obs-plugins/64bit` copy is the one loaded by the standalone OBS runtime used for verification.

## Lifecycle verification

Three consecutive runs were performed with exactly one OBS process at a time, using:

```text
obs64.exe --portable --profile Sync_Replay_Dev
```

Each cycle verified:

- the process became responsive;
- `[obs-status-indicators] plugin loaded` was present in the fresh log;
- `[obs-status-indicators] overlay window ready` was present;
- graceful shutdown returned the OBS process count to zero.

The observed logs were `2026-09-04 15-30-24.txt`, `2026-09-04 15-30-38.txt`, and `2026-09-04 15-30-51.txt`. The final result was `final_obs_count=0`.

## Ownership and teardown review

The provider unregisters its frontend callback, releases retained source/output references, removes the saving tick callback, and publishes copied state snapshots. The overlay owns its window class and UI thread, handles layout messages on that thread, joins the thread during stop, destroys the window, and unregisters the class. The controller test covers empty, full, precedence, muted, and first/middle/last removal layouts.

The repository does not claim a sanitizer or OS-level handle-leak proof. Such tooling is not part of the supplied OBS/Visual Studio fixture, so leak-sensitive behavior is covered by deterministic teardown code, repeated runtime cycles, and explicit process/thread shutdown evidence.

## Remaining product qualifications

The documented capture qualification from Phase 6 remains part of the release statement: Windows capture affinity is best effort and was verified for the supplied Display Capture path, but it cannot guarantee exclusion from every recorder, compositor, driver, or exclusive-fullscreen implementation. Saving is a bounded post-completion indicator because the public API does not expose a generic pre-save hook for an arbitrary Replay Buffer hotkey.
