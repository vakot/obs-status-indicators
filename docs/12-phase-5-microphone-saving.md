# Phase 5 — Microphone and saving indicators

## Result

Phase 5 is **possible** with one timing qualification for saving. The provider now resolves an audio input after OBS emits `OBS_FRONTEND_EVENT_FINISHED_LOADING`, tracks its mute/enabled/lifetime signals, retains and releases the OBS reference, and tracks Replay Buffer output completion without making either optional object a plugin-fatal dependency.

## Microphone resolution

The public OBS 31.1.1 frontend API does not expose a dedicated global Mic/Aux getter. The provider therefore enumerates input sources, prefers the stable `wasapi_input_capture` source type used by the Windows fixture, and falls back deterministically to the first audio-capable input. Localized display names are never used as the primary key. Source UUID comparison prevents replacing an unchanged reference during refresh.

The microphone state is available while the resolved source is enabled. `microphoneMuted` is retained as a separate fact and is carried into the renderer-neutral `MIC` entry; the GDI renderer marks the muted row with a red background. `mute`, `rename`, `update`, `enable`, `audio_activate`, and `audio_deactivate` update the snapshot. `remove` and `destroy` suppress only the microphone entry; recording, Replay Buffer, and saving remain independent.

Resolution is deferred until `FINISHED_LOADING`. OBS loads third-party modules before all profile-owned global audio/output objects are ready, so resolving those objects from `obs_module_load` is unsafe. Profile and scene-collection lifecycle events trigger a later refresh.

## Replay Buffer saving

The provider obtains its own reference from `obs_frontend_get_replay_buffer_output()` and connects the output's exact `saved()` signal. The fixture confirmed `SaveReplayBuffer` writes an MP4 and emits the signal. The frontend `OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED` starts a one-second `SAVING` state, and an OBS tick callback expires it and is removed on completion or teardown.

OBS exposes save completion but no generic pre-save signal for another component's configured hotkey. Consequently, this MVP implementation is honest about the boundary: `SAVING` is a bounded post-completion lifecycle confirmation, not a claimed progress indicator from the instant a user presses an arbitrary hotkey. A future exact-progress implementation would need a supported request hook or a plugin-owned save command.

## Verification

With one standalone OBS 32.2.1 process, the configured USB PnP Audio Device resolved as `wasapi_input_capture`; websocket mute toggles produced `mic=1 muted=1` and `mic=1 muted=0` snapshots. Starting Replay Buffer, saving, and stopping produced `replay=1`, `saving=1`, then `saving=0` before `replay=0`. The earlier startup crash caused by premature resolution was reproduced once and eliminated by the `FINISHED_LOADING` gate.
