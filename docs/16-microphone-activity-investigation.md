# Microphone activity investigation

Date: 2026-09-05

## Verdict: POSSIBLE, with heuristic limits

OBS exposes two different kinds of microphone information:

- Source signals such as `audio_activate` and `audio_deactivate` describe the source lifecycle. They do not answer whether the current samples contain audible input.
- The public `obs_volmeter` API can attach to an OBS source and deliver per-channel `magnitude`, `peak`, and `input_peak` levels. The implementation uses `input_peak`, which represents the raw input level before the source volume control is applied.

The plugin can therefore show a microphone indicator as muted-looking when the resolved microphone source has produced no input above a selected threshold for a selected time window. This is a best-effort activity heuristic, not a speech detector or a hardware diagnostic.

Primary references:

- [OBS source reference](https://github.com/obsproject/obs-studio/blob/master/docs/sphinx/reference-sources.rst) documents source audio lifecycle signals.
- [OBS source API](https://github.com/obsproject/obs-studio/blob/master/libobs/obs-source.h) documents source audio callbacks and source state APIs.
- [OBS audio controls API](https://github.com/obsproject/obs-studio/blob/master/libobs/obs-audio-controls.h) documents `obs_volmeter_t`, source attachment, callbacks, and the `input_peak` channel array.

## Implemented policy

- Scan every channel reported by the volume meter. Activity on any channel counts as microphone activity.
- Treat an input peak strictly above `-60 dBFS` as audible. The exact threshold is intentional: values at or below the threshold are treated as silence.
- Keep the indicator active while the source is being initialized, so a newly resolved microphone does not flash muted before its first meter window.
- After 2.0 seconds without an audible sample, set the existing `microphoneMuted` state and reuse the existing muted presentation (`mic-off` / red tile).
- Restore the normal microphone presentation on the next OBS tick after an audible sample is observed.
- Preserve explicit OBS source mute behavior: a source muted in the mixer is immediately presented as muted regardless of audio activity.
- If OBS cannot create or attach the volume meter, retain the existing source-availability and explicit-mute behavior and log a warning instead of disabling the plugin.

The volume-meter callback runs outside the plugin's normal state-publication path. It only records an atomic activity flag. The OBS tick callback consumes that flag and publishes state changes on the normal OBS thread, avoiding UI/state callbacks from the audio callback.

## Limitations and interpretation

Silence is not the same as mute. A user who is quiet, a microphone with a very low signal, a noisy threshold configuration, or a disconnected device can all produce the same muted-looking presentation. The threshold and timeout are fixed for this first implementation because the request calls for a status heuristic rather than additional settings.

The implementation follows the microphone source selected by the existing provider: it prefers the WASAPI input source and otherwise uses the first audio source. It does not independently enumerate Windows capture devices or infer which source represents a human microphone.

## Verification

Automated policy coverage is in `tests/microphone-activity-test.cpp` and is included in the CTest suite. The portable OBS check is:

1. Build and run `RUN_TESTS`.
2. Install into the standalone `obs-dev` folder while OBS is stopped.
3. Start exactly one instance with `scripts/start.ps1`.
4. Leave the configured microphone silent for at least two seconds and inspect the latest plugin log for a state snapshot with `mic=1 muted=1`.
5. Speak into the microphone or make a clearly audible test sound and verify the normal microphone presentation returns.
6. Toggle the mixer mute control and verify the indicator remains muted even while audio is present.
7. Stop OBS before any rebuild or reinstall and verify that no `obs64` process remains.

The runtime silence result depends on the fixture's actual microphone noise floor. If its ambient noise exceeds `-60 dBFS`, the indicator will correctly remain active according to this policy; that is an expected limitation, not evidence that the scan is disconnected.
