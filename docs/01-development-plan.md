# Development plan

This plan turns the MVP specification into independently verifiable phases. Each phase has a narrow scope, an exit gate, and evidence that should be recorded before moving on.

## Phase 0 — Baseline and feasibility gate

Objective: establish the runtime, API, and platform assumptions before creating the plugin.

Tasks:

- Confirm the supported target is Windows x64 and define the minimum supported Windows build. Use Windows 10 version 2004 or newer for the documented `WDA_EXCLUDEFROMCAPTURE` behavior; keep Windows 11 in the primary test set.
- Pin the initial development runtime to the supplied portable OBS 32.2.1 installation in `obs-dev`.
- Record the active profile, output settings, capture sources, microphone source, display topology, and encoder availability.
- Confirm exact OBS headers/API names from the current OBS source, not memory or a third-party wrapper.
- Decide the microphone product semantics from the previous overlay implementation before coding. Do not infer that “active,” “unmuted,” and “has audio” mean the same thing.
- Create a capture test fixture plan that can show a unique marker in the overlay and detect its presence in recorded frames.

Exit gate: native integration is feasible, the microphone identity/semantics are known, and capture exclusion is explicitly treated as a tested capability rather than a guarantee.

## Phase 1 — Native plugin skeleton and build

Objective: load and unload a no-op native plugin cleanly.

Tasks:

- Create the CMake project using current OBS plugin-template conventions.
- Link `OBS::libobs` and `OBS::obs-frontend-api`; avoid Qt unless a later concrete requirement appears.
- Add module metadata, locale hooks if required by the chosen template, `obs_module_load`, and `obs_module_unload`.
- Register and unregister one frontend event callback.
- Add a consistent `[obs-status-indicators]` log prefix.
- Add a local install target that places the plugin under `obs-plugins/64bit` for `obs-dev` testing without touching the normal OBS installation.

Exit gate: the plugin builds for Windows x64, appears in the standalone OBS log, receives a test frontend event, and unloads without a stale callback or process crash.

## Phase 2 — OBS state provider

Objective: translate OBS state into a small internal model with no renderer dependency.

Tasks:

- Implement recording state from `obs_frontend_recording_active()` and `OBS_FRONTEND_EVENT_RECORDING_STARTED/STOPPED`.
- Implement pause state from `obs_frontend_recording_paused()` and `OBS_FRONTEND_EVENT_RECORDING_PAUSED/UNPAUSED`.
- Implement Replay Buffer state from `obs_frontend_replay_buffer_active()` and the corresponding frontend start/stop events.
- Query all authoritative values after plugin initialization and after profile/scene-collection changes where source identity can change.
- Treat startup reconciliation as a first-class transition, not as a special renderer case.
- Keep all OBS object references owned and released by the provider.

Exit gate: a unit-testable provider produces correct snapshots for initial state, every start/stop/pause transition, and profile/source loss without polling.

## Phase 3 — Win32 overlay foundation

Objective: prove a compact, non-activating, click-through overlay independently of OBS state.

Tasks:

- Create the top-level window on a dedicated UI thread with a message loop.
- Use `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW` and a borderless popup window.
- Keep the window topmost using `SetWindowPos(HWND_TOPMOST, ..., SWP_NOACTIVATE)`.
- Render a hardcoded test marker into a 32-bit premultiplied-alpha surface and publish it using `UpdateLayeredWindow`.
- Apply `SetWindowDisplayAffinity` and record both the return value and `GetLastError()` on failure.
- Verify no taskbar/Alt-Tab entry, no foreground activation, and click-through behavior.

Exit gate: the marker is visible over normal desktop applications, does not take focus, passes clicks through, and can be destroyed deterministically.

## Phase 4 — Indicator controller and atomic layout

Objective: turn state snapshots into a complete desired visual set.

Tasks:

- Define an immutable/copyable `IndicatorState` and a renderer-neutral `IndicatorEntry` list.
- Resolve precedence: `PAUSED` replaces the normal `REC` presentation; `SAVING` is independent and transient; unavailable microphone state produces no `MIC` entry.
- Compute the entire ordered vertical stack from the new snapshot.
- Resize, reposition, and repaint the overlay in one UI-thread operation.
- Add reducer/layout tests that remove the first, middle, and last indicator and assert no intermediate gap state is observable.

Exit gate: all combinations of the five MVP indicators have deterministic output and one state change causes one coherent target layout.

## Phase 5 — Microphone and saving indicators

Objective: implement the two states that require source/output-specific behavior.

Tasks:

- Resolve the microphone source using stable identity where available; use the configured global input source or a documented fallback rather than a localized display name as the primary key.
- Retain a weak source reference or re-resolve by UUID/name when the source is destroyed or the profile changes.
- Subscribe to `mute`, `destroy`, `remove`, `rename`, and relevant enable/activity signals as supported by the exact source type.
- Match the previous overlay’s semantic rule for `MIC` and document it in a test fixture.
- Acquire the Replay Buffer output with its own reference and subscribe to its `saved()` output signal.
- Show `SAVING` only on a save request/lifecycle event with a bounded, cancellable timer; never equate “hotkey pressed” with “file successfully saved.”

Exit gate: missing microphone or output objects disable only their indicator, while recording and Replay Buffer indicators continue to work.

## Phase 6 — Capture and display behavior

Objective: determine the actual supported behavior of the finished overlay.

Tasks:

- Test Display Capture, Window Capture, and Game Capture separately with a unique overlay marker.
- Verify both still images and short recordings; inspect frames rather than trusting a successful API return.
- Test normal, maximized, borderless fullscreen, and practical exclusive fullscreen cases.
- Test primary-display positioning and monitor changes. Keep monitor selection fixed for the MVP; do not add configuration.
- Where practical, test coexistence with Discord, Steam, and NVIDIA overlays for focus/input conflicts only.

Exit gate: results are recorded per capture path and environment. Any limitation becomes part of the product acceptance statement.

## Phase 7 — Stability, packaging, and release readiness

Objective: prove safe lifecycle behavior and normal plugin installation.

Tasks:

- Exercise startup, shutdown, plugin unload/reload where supported, profile changes, source removal, and repeated state transitions.
- Verify no duplicate callbacks, stale HWNDs, abandoned UI threads, timer callbacks after destruction, or leaked OBS references.
- Build a RelWithDebInfo Windows x64 artifact with the supported OBS plugin toolchain.
- Install into a clean OBS portable fixture and verify the normal plugin lifecycle.
- Run the complete matrix in [05-test-matrix.md](05-test-matrix.md), attach logs and capture evidence, and record all unavailable cases.

Exit gate: the definition of done is satisfied, including the documented capture-exclusion limitations.

## Recommended implementation order

```text
Phase 0 → Phase 1 → Phase 2
                      ↘ Phase 3 → Phase 4 → Phase 5
                                      ↘ Phase 6 → Phase 7
```

Do not start visual polish, settings, animations, monitor selection, or cross-platform abstractions before the state, lifecycle, and capture gates pass.
