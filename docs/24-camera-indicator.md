# Camera indicator

Date: 2026-09-14

## Behavior

The camera indicator is visible only when at least one active OBS `Video Capture Device` source is producing advancing video frames.

- No DirectShow camera source: hidden.
- DirectShow camera source exists but is inactive: hidden.
- DirectShow camera source is active but the device is unavailable or stopped: hidden after the output grace period.
- DirectShow camera source is active and producing frames: visible.

## Detection

OBS identifies the built-in Windows `Video Capture Device` source with the stable source ID `dshow_input`. The provider scans all sources with that ID on the OBS tick thread and requires `obs_source_active(source)`.

For active sources, the provider reads the latest asynchronous frame with `obs_source_get_frame()`. A frame is considered output when its OBS timestamp advances since the previous scan. This distinguishes a running camera from a source that remains configured while the device is disconnected or turned off but leaves a stale frame available.

The scan runs every 250 ms. A one-second grace period prevents a normal frame-delivery scheduling delay from causing a flicker; if no new frame arrives during that period, the indicator is hidden. If multiple camera sources are active, the indicator remains visible while any one of them produces frames.

## Limits

This is an OBS-source/output heuristic, not a direct Windows camera privacy or hardware diagnostic. A device that continuously emits repeated frames, a virtual camera that intentionally freezes its timestamp, or an OBS source implementation that does not expose asynchronous frames may not be classified perfectly. The current portable fixture contains an unavailable DirectShow camera source, so automated tests cover the activity policy and source/controller logic; hardware-output acceptance requires a functioning camera.
