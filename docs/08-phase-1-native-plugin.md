# Phase 1 - Native plugin skeleton

## Result

Phase 1 is **possible**. A native Windows x64 OBS module can be built with the current OBS plugin-template CMake conventions, installed into the supplied portable fixture, discovered by OBS 32.2.1, and given a matching callback-registration path for teardown.

## Implementation

- `CMakeLists.txt` uses the plugin-template bootstrap and links `OBS::libobs` plus `OBS::obs-frontend-api`.
- `buildspec.json` pins the reproducible template SDK inputs used for the first build: OBS sources 31.1.1 and the 2025-07-11 prebuilt dependency archives.
- `CMakePresets.json` enables the frontend API and uses the locally installed Windows SDK. The generic template SDK version selector was changed from 10.0.22621 to `x64` because this machine provides Windows SDK 10.0.26100.0.
- `src/plugin-main.c` declares the module, enables the default locale, registers one frontend callback during `obs_module_load`, unregisters it during `obs_module_unload`, and logs through the `[obs-status-indicators]` prefix.
- `OBS_DEV_DIR` adds a local install path to `obs-dev/obs-plugins/64bit` and `obs-dev/data/obs-status-indicators` without requiring a normal OBS installation.

## Build evidence

The build used MSVC 19.44.35228.0, Visual Studio 2022 Build Tools, CMake from the Visual Studio installation, and Windows SDK 10.0.26100.0. RelWithDebInfo configuration and build completed successfully.

The portable OBS run used OBS 32.2.1, profile `Sync_Replay_Dev`, and produced these relevant log entries:

- `obs-status-indicators.dll` was listed by the OBS module loader.
- `[obs-status-indicators] plugin loaded; frontend callback registered`.
- Multiple `[obs-status-indicators] frontend event received: ...` entries were written during startup.
- OBS reached `==== Startup complete ========================`.

The build SDK is currently 31.1.1 because that is the version and hash shipped by the current official plugin-template bootstrap. The runtime smoke test is against OBS 32.2.1; the skeleton only uses stable module/frontend entry points, but later phases must keep the SDK/runtime pairing under review and move to a matching SDK pin if required by an API or ABI change.

## Shutdown note

The fixture has `SysTrayEnabled=true`. Sending a normal main-window close leaves OBS resident in the tray, so the test harness must verify the process count after shutdown and explicitly clean up the one known test PID when the tray prevents termination. No concurrent OBS instance was used.

## Remaining Phase 1 risk

The current test proves loading and frontend dispatch. A normal UI-driven exit/unload should be repeated after the next phase introduces owned state and teardown; the tray setting should be disabled in a dedicated disposable fixture if an unload-specific test is needed.
