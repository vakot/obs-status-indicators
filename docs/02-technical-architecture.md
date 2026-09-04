# Technical architecture

## Data flow

```text
OBS frontend callbacks / source signals / output signals
                         ↓
                 ObsStateProvider
                         ↓
                  IndicatorState
                         ↓
                IndicatorController
                         ↓ copied snapshot
                 WindowsOverlayRenderer
                         ↓
              one complete Win32 layout update
```

The renderer must not query OBS. The provider must not know Win32 details. The controller owns presentation precedence and the complete visible set.

## OBS API mapping

| Concern | Initial query | Change notification | Lifetime rule |
| --- | --- | --- | --- |
| Recording active | `obs_frontend_recording_active()` | `OBS_FRONTEND_EVENT_RECORDING_STARTED`, `OBS_FRONTEND_EVENT_RECORDING_STOPPED` | No polling |
| Recording paused | `obs_frontend_recording_paused()` | `OBS_FRONTEND_EVENT_RECORDING_PAUSED`, `OBS_FRONTEND_EVENT_RECORDING_UNPAUSED` | `paused` is meaningful only while recording is active |
| Replay Buffer active | `obs_frontend_replay_buffer_active()` | `OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED`, `OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED` | No polling |
| Replay saved | No persistent state required | `obs_frontend_get_replay_buffer_output()` → `obs_output_get_signal_handler()` → `saved()` | Hold/release the output reference; disconnect before release |
| Microphone mute | `obs_source_muted(source)` | source `mute` signal | Hold a safe source reference or weak reference |
| Microphone/source availability | source lookup and source metadata | `destroy`, `remove`, `rename`, `update`, and profile/collection changes | Missing source is non-fatal |

The frontend header and documentation are authoritative for these names and semantics: [OBS frontend API header](https://github.com/obsproject/obs-studio/blob/master/frontend/api/obs-frontend-api.h) and [frontend API reference](https://github.com/obsproject/obs-studio/blob/master/docs/sphinx/reference-frontend-api.rst). The Replay Buffer output’s `saved()` signal is registered by OBS’s FFmpeg muxer: [obs-ffmpeg-mux.c](https://github.com/obsproject/obs-studio/blob/master/plugins/obs-ffmpeg/obs-ffmpeg-mux.c).

## State model

Use explicit state names; do not overload one boolean with multiple meanings.

```cpp
struct IndicatorState {
    bool recording = false;
    bool recordingPaused = false;
    bool replayBuffer = false;
    bool microphoneAvailable = false;
    bool microphoneMuted = false;
    bool microphoneActive = false; // only if the chosen microphone semantics need it
    bool saving = false;
};
```

The visible-state reducer should enforce invariants:

- `recordingPaused` is ignored when `recording` is false.
- `PAUSED` and the normal `REC` entry are mutually exclusive.
- `saving` expires through one owned timer and is cleared on completion/failure/teardown.
- `microphoneAvailable == false` suppresses only `MIC`.

The exact visible microphone rule remains a Phase 0 product decision. The current profile proves that the runtime has a configured `wasapi_input_capture` source, but a source being configured, unmuted, active, or audibly receiving samples are different facts.

For `SAVING`, the first supported path should be Replay Buffer save: request/save is initiated, the `saved()` output signal confirms completion, and the controller clears the transient state. Do not show a successful save indication for ordinary recording stop or an arbitrary hotkey unless a corresponding output lifecycle signal is verified. If the signal is unavailable for an output, mark that saving path unsupported rather than inventing a file-watcher state machine.

## Lifecycle

### Load

1. Initialize the provider and state lock/dispatch mechanism.
2. Create/start the renderer UI thread and overlay window.
3. Register frontend callbacks and source/output signals.
4. Query authoritative state and publish one initial snapshot.
5. Log success or isolate optional failures.

### Unload

1. Mark teardown started so callbacks become no-ops.
2. Remove frontend callbacks.
3. Disconnect source/output signals.
4. Cancel transient timers.
5. Publish an empty/close command and join the renderer thread.
6. Release OBS references and destroy synchronization objects.

No OBS callback may call Win32 window operations directly unless the implementation proves that callback is running on the renderer thread. The safe default is to update state and enqueue a copied snapshot.

## Reference and thread ownership

- `ObsStateProvider` owns OBS references and signal registrations.
- `IndicatorController` owns the last coherent state and desired entries.
- `WindowsOverlayRenderer` owns `HWND`, GDI/DIB resources, the UI thread, and its message loop.
- The producer-to-renderer boundary carries values, not raw OBS pointers.
- Shutdown first prevents new work, then removes registrations, then joins the UI thread.

Use a mutex or a single-producer message/command mechanism only where needed. A generic event bus, thread pool, or service process is not justified by this feature.

## Build and packaging boundary

Use the official OBS plugin template as the baseline for CMake and packaging. It links `OBS::libobs` and optionally `OBS::obs-frontend-api`: [obs-plugintemplate CMakeLists.txt](https://github.com/obsproject/obs-plugintemplate/blob/master/CMakeLists.txt). The output is a normal Windows x64 OBS module under `obs-plugins/64bit`; the MVP has no external runtime dependency beyond the OBS installation and Windows APIs.
