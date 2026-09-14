# Camera indicator

Date: 2026-09-14

## Behavior

The camera indicator is visible only when at least one active OBS `Video Capture Device` source has video output.

- No DirectShow camera source: hidden.
- DirectShow camera source exists but is inactive: hidden.
- DirectShow camera source is active but the device is unavailable or stopped: hidden after the output grace period.
- DirectShow camera source is active and has non-zero video output dimensions: visible.

## Detection

OBS identifies the built-in Windows `Video Capture Device` source with the stable source ID `dshow_input`. The provider scans all sources with that ID on the OBS tick thread and requires `obs_source_active(source)`.

For active sources, the provider checks `obs_source_get_width()` and `obs_source_get_height()` for non-zero output dimensions. This uses OBS's non-consuming output state and avoids taking the current asynchronous frame away from OBS's renderer.

The scan runs every 250 ms. A one-second grace period prevents a normal output scheduling delay from causing a flicker; if no output is reported during that period, the indicator is hidden. If multiple camera sources are active, the indicator remains visible while any one of them has output.

## Limits

This is an OBS-source/output heuristic, not a direct Windows camera privacy or hardware diagnostic. A device may remain classified as available if it keeps a non-zero output size while emitting a frozen or blank stream. The current portable fixture contains unavailable DirectShow camera sources, so automated tests cover the activity policy and source/controller logic; hardware-output acceptance requires a functioning camera.
