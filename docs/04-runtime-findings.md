# Runtime findings from `obs-dev`

## Environment

Observed on 2026-09-04:

- OS: Windows 11 Pro 25H2, x64.
- Standalone runtime: `obs-dev/bin/64bit/obs64.exe`, OBS Studio 32.2.1.
- Runtime mode: portable installation using the repository’s `obs-dev` directory.
- Active development profile: `Sync_Replay_Dev`.
- Display reported by the profile: LG SMARTGAME+ at 3840×2160.
- OBS renderer: Direct3D 11.
- Display Capture: `monitor_capture`, DXGI method.
- Microphone: WASAPI input source (`wasapi_input_capture`) using the configured USB PnP Audio Device.
- Replay Buffer: enabled in the simple-output profile with a 90-second buffer and NVENC recording encoder.

The standalone process started successfully and reached `==== Startup complete ===============================================` in the OBS log. The log also showed the expected OBS WebSocket module, NVENC availability, DXGI Display Capture initialization, and WASAPI microphone initialization.

## What this proves

- The supplied runtime is suitable for native plugin load testing once a plugin is built.
- The test profile already contains the important runtime ingredients for the MVP: Display Capture, Replay Buffer, and a microphone source.
- The machine has a usable Windows graphics/capture stack for a first overlay probe.
- OBS 32.2.1 exposes the frontend event and state APIs required by the architecture through the current official headers/documentation.

## Controlled experiment notes

- OBS was kept to one running instance at a time after the user clarified the constraint.
- The existing OBS WebSocket configuration was temporarily enabled for a connectivity/version probe and then restored to its original disabled/authenticated settings. The WebSocket was an investigation aid only and is not part of the proposed plugin architecture; no state mutation result is used as evidence.
- No plugin source exists yet, so no native overlay could be loaded into OBS during this investigation.

## Not yet experimentally verified

These require the Phase 3 overlay implementation and must remain explicit release gates:

- `WDA_EXCLUDEFROMCAPTURE` behavior in Display Capture, Window Capture, and Game Capture.
- Whether the overlay remains visible over every relevant borderless/exclusive fullscreen application.
- Click-through behavior against a real underlying application.
- No-focus/no-taskbar behavior of the final renderer window.
- Atomic visual reflow under actual state transitions.
- Microphone semantics matching the previous overlay, because the prior implementation is not present in this repository.

The absence of those results is not evidence that the MVP is impossible; it means the implementation must not claim them until the capture and UI probes are run.
