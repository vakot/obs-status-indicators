# Verification and acceptance matrix

## Automated tests

Add tests for renderer-independent logic before relying on manual OBS runs.

| Area | Cases | Expected result |
| --- | --- | --- |
| State normalization | paused without recording; start/stop; profile loss | coherent state; no contradictory `REC` + `PAUSED` presentation |
| Indicator ordering | every subset of REC/PAUSED/REPLAY/MIC/SAVING | deterministic vertical order |
| Layout | remove first, middle, last row | one final compact layout; no intermediate gap |
| Saving timer | start, completion, cancellation, unload | no stale timer callback; indicator expires exactly once |
| Source lifetime | mute, rename, remove, destroy, re-resolve | no use-after-release; only MIC becomes unavailable |
| Failure isolation | renderer init failure; capture affinity failure | OBS state/output path remains unaffected |

## Manual state and lifecycle matrix

Run against one clean `obs-dev` OBS process with the test profile.

| Scenario | Expected |
| --- | --- |
| Launch OBS | plugin loads and reconciles current state |
| Start recording | `REC` appears after the started event |
| Pause recording | `PAUSED` replaces `REC` |
| Resume recording | normal recording presentation returns |
| Stop recording | recording indicator disappears |
| Start Replay Buffer | `REPLAY` appears |
| Stop Replay Buffer | `REPLAY` disappears |
| Save replay | `SAVING` reflects the defined save lifecycle and clears on completion/timeout |
| Toggle microphone | `MIC` follows the documented prior-overlay semantics |
| Remove microphone source | other indicators continue; `MIC` becomes unavailable |
| Multiple indicators | rows have correct order and spacing |
| Remove top/middle row | remaining rows reflow in one visual step |
| Click overlay area | underlying application receives the click |
| Switch applications | overlay does not steal activation or keyboard focus |
| Close OBS | callbacks, timers, window, and UI thread are destroyed |

## Capture matrix

The overlay must display a unique marker for this test. Record the result, OBS version, Windows build, capture source, and whether the marker is visible in the resulting frame.

| Capture path | Test setup | Result to record |
| --- | --- | --- |
| Display Capture | capture the monitor containing the marker | marker absent/present; screenshot and recording evidence |
| Window Capture | capture a window under/near the marker | marker absent/present; screenshot and recording evidence |
| Game Capture | capture a practical test app | marker absent/present; screenshot and recording evidence |

Also run normal windowed, maximized, borderless fullscreen, and practical exclusive fullscreen cases. A test failure must be classified as either an implementation defect or a platform/capture-path limitation; do not silently broaden the API design to hide it.

## Definition of done

The MVP is ready only when:

- the plugin is a normal Windows x64 OBS module;
- no Python, external process, IPC, scene integration, or output-rendering hook is required;
- all state transitions and initial reconciliation are correct;
- the overlay is transparent, topmost, non-activating, and click-through;
- layout updates are atomic;
- optional overlay failures do not affect OBS outputs;
- unload is deterministic;
- capture-exclusion results are documented per capture path, including limitations.
