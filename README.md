# OBS Status Indicators

OBS Status Indicators is a Windows x64 OBS Studio plugin that shows a small, click-through overlay for important OBS states.

## Features

- Recording, paused recording, Replay Buffer, and combined recording + Replay Buffer states.
- Microphone availability, explicit mute state, and prolonged-silence warning.
- Camera indicator for active OBS `Video Capture Device` sources that are producing video frames.
- Short-lived Replay Buffer saving indicator.
- Configurable overlay origin, orientation, offset, gap, indicator size, colors, and opacity.
- Non-activating, click-through, topmost overlay that is excluded from capture where Windows supports `WDA_EXCLUDEFROMCAPTURE`.

The overlay is a status display only. It does not provide controls and does not capture mouse or keyboard input.

## Requirements

- Windows x64.
- OBS Studio x64 with a compatible frontend API. The project is developed and tested against OBS Studio `31.1.1`.
- For source builds: Visual Studio Build Tools 2022 with the C++ workload and CMake.

## Installation

Download the latest Windows artifact from the [GitHub Releases page](https://github.com/vakot/obs-status-indicators/releases). For release `v0.0.3`, the package is `obs-status-indicators-windows-x64-v0.0.3.zip`.

1. Close OBS completely.
2. Extract the ZIP into the OBS installation directory.
3. Allow the `obs-plugins` and `data` directories to merge.
4. Start OBS. The plugin starts automatically with OBS; no separate indicator process is required.

To update an existing installation from a local artifact:

```powershell
.\scripts\update.ps1 `
  -Artifact '.\release\obs-status-indicators-windows-x64-v0.0.3.zip' `
  -ObsRoot 'C:\Path\To\OBS'
```

## Startup

For a normal OBS installation, start OBS using its usual shortcut or system startup entry. The plugin loads when OBS loads.

For the supplied standalone development OBS installation, use the repository script. It closes existing OBS processes, builds and installs the current plugin, and starts exactly one instance:

```powershell
Set-Location 'C:\Users\vakot\Documents\GitHub\obs-status-indicators'
.\scripts\start.ps1
```

To start the standalone fixture with Replay Buffer enabled immediately:

```powershell
$obs = (Resolve-Path '.\obs-dev\bin\64bit\obs64.exe').Path
Start-Process -FilePath $obs `
  -WorkingDirectory (Split-Path -Parent $obs) `
  -ArgumentList @('--portable', '--profile', 'Sync_Replay_Dev', '--startreplaybuffer', '--minimize-to-tray')
```

Do not run multiple OBS instances against the same profile. Close the existing instance before rebuilding or installing the plugin.

## Settings

Open `Tools > Status Indicators` in OBS. The settings include:

- Origin and orientation.
- Offset and gap.
- Indicator size.
- Background color and icon color.
- Shared opacity.

Settings are persisted in OBS plugin configuration. The full layout and local verification flow is documented in [`docs/15-local-test-flow.md`](docs/15-local-test-flow.md).

## Camera detection

The camera indicator scans OBS DirectShow sources with source ID `dshow_input`. It is visible only when at least one matching source is active and its asynchronous frame timestamp continues advancing.

- No camera source: hidden.
- Camera source inactive or hidden: hidden.
- Camera source configured but disconnected or stopped: hidden after the one-second output grace period.
- Active camera producing frames: visible.

The scan runs every 250 ms. This is an OBS-source/output heuristic, not a direct hardware or Windows privacy-state diagnostic. See [`docs/24-camera-indicator.md`](docs/24-camera-indicator.md) for details and limitations.

## Build and test

Configure the Windows preset once if needed, then build and run the local tests:

```powershell
Set-Location 'C:\Users\vakot\Documents\GitHub\obs-status-indicators'
cmake --preset windows-x64
cmake --build --preset windows-x64 --parallel 8
cmake --build build_x64 --config RelWithDebInfo --target RUN_TESTS
```

Install the current build into the standalone OBS fixture:

```powershell
cmake --install build_x64 --config RelWithDebInfo
```

The convenient development flow is:

```powershell
.\scripts\start.ps1
```

## Releases

Releases are built locally; this repository does not use GitHub Actions. The release script builds, tests, packages, hashes, tags, and uploads the release through GitHub CLI.

Run it from a clean, up-to-date `master` checkout after updating `buildspec.json` to the intended version:

```powershell
.\scripts\release.ps1 -Version 0.0.4
```

Use `-DryRun` with an unreleased version to build and package without creating a tag or GitHub release:

```powershell
.\scripts\release.ps1 -Version 0.0.4 -DryRun
```

See [`docs/17-release-and-update-flow.md`](docs/17-release-and-update-flow.md) for the complete release and signing flow.

## Documentation

- [`docs/README.md`](docs/README.md) — investigation and architecture index.
- [`docs/15-local-test-flow.md`](docs/15-local-test-flow.md) — reproducible standalone testing flow.
- [`docs/17-release-and-update-flow.md`](docs/17-release-and-update-flow.md) — release and update procedure.
- [`docs/23-overlay-window-behavior.md`](docs/23-overlay-window-behavior.md) — overlay z-order and input behavior.
- [`docs/24-camera-indicator.md`](docs/24-camera-indicator.md) — camera output detection details.

## Licensing

This repository currently does not include a separate project license file. Unless the project owner adds one, the plugin source should be treated as not broadly licensed for redistribution beyond the permissions explicitly granted by the copyright holder.

The committed Lucide icon assets are distributed under the licenses recorded in [`data/icons/lucide/LICENSE`](data/icons/lucide/LICENSE). OBS Studio and its dependencies retain their own licenses. Release packages include the Lucide license file alongside the icon assets.
