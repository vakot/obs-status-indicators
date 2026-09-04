# Phase 2 - OBS state provider

## Result

The renderer-independent OBS state provider is implemented and builds successfully. It owns one frontend callback, reconciles authoritative OBS values into a copyable `IndicatorState`, publishes only changed snapshots after startup, and removes the callback before destruction.

## State and event mapping

`IndicatorState` currently contains the MVP OBS facts that do not require source/output-specific ownership:

- `recording` from `obs_frontend_recording_active()`.
- `recordingPaused` from `obs_frontend_recording_paused()`, guarded by `recording` so the model cannot expose paused-without-recording.
- `replayBuffer` from `obs_frontend_replay_buffer_active()`.

The provider reconciles after recording started/stopped/paused/unpaused, Replay Buffer started/stopped, profile and scene-collection changes, and finished loading. The event is only a trigger; the query remains authoritative. A forced initial reconciliation happens immediately after callback registration.

## Verification

Using one OBS 32.2.1 process and the `Sync_Replay_Dev` profile, the provider logged the initial all-false snapshot, a recording-start snapshot, a recording-stop snapshot, a Replay Buffer start snapshot, and a Replay Buffer stop snapshot. The websocket test server was enabled only for this experiment and the original authenticated/disabled configuration was restored afterward.

The installed OBS fixture did not retain a paused recording state with its current simple-output configuration: `PauseRecord` returned success, but `GetRecordStatus` remained `outputPaused=false` and no paused snapshot was observable. This is recorded as an environment/test-fixture limitation, not as permission to replace the planned authoritative pause query with polling or inference. A later manual test should use an OBS output/configuration that exposes pause and verify the existing event/query path.

## Ownership and teardown

The provider has no renderer dependency and contains no OBS object references beyond the frontend callback registration. Its destructor calls `stop()`, which removes the exact callback/context pair. The current portable fixture has `SysTrayEnabled=true`; window-close leaves a tray process, so each test verifies that exactly one process exists and cleans up that exact process before restoring experiment configuration.
