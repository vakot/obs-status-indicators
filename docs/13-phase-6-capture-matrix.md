# Phase 6 - Capture and display behavior

## Result

The planned MVP remains **possible with a qualified capture-exclusion statement**. The native overlay was excluded from the tested desktop/display capture artifacts, and the Window Capture path was exercised against a real window. Game Capture source creation works, but this environment did not provide a hookable game target, so universal exclusion across every Game Capture implementation remains unproven.

## Test environment

- Windows 11 Pro 25H2 x64.
- Portable OBS 32.2.1 in `obs-dev`.
- Primary display: LG SMARTGAME+, 3840x2160.
- OBS output: 1920x1080 H.264 video with AAC audio in a 4.9 second MP4.
- Fixture scene: `Gameplay Test`, with the existing DXGI `Display Capture` source.
- Window target: Windows 11 Notepad, selected as `Untitled - Notepad:Notepad:Notepad.exe`.
- Exactly one OBS process was used; the process count was zero after cleanup.

The experiment temporarily enabled obs-websocket on localhost, created capture sources, switched to `Gameplay Test`, recorded, extracted the last frame with FFmpeg, and restored the websocket configuration. The temporary capture inputs were removed from the portable scene file after the run.

## Matrix

| Path | Result | Evidence and interpretation |
| --- | --- | --- |
| Display Capture | PASS for the tested path | A desktop still image and the recorded MP4 frame contained the desktop/game content but no overlay marker. The overlay log reported `capture exclusion enabled for overlay`. |
| Window Capture | PASS for source operation; exclusion is structural | `window_capture` was created with the BitBlt method and captured the Notepad client window. The overlay is a separate top-level window, so it is outside the selected window bounds; this confirms compatibility, but does not by itself prove WDA behavior for a capture implementation that composites windows. |
| Game Capture | INCONCLUSIVE in this environment | `game_capture` was created in window mode. OBS attempted to hook `Notepad.exe`, reported that the hook was not loaded, and stopped capture. Notepad is not a game target, so this is an unavailable fixture rather than a product failure. |
| Still image | PASS for desktop capture | `desktop-with-overlay.png` was produced from the primary desktop. The exclusion-enabled overlay was not present in the captured bitmap. |
| Short recording | PASS for desktop capture | `2026-09-04 15-25-48.mp4` was created, probed as H.264/AAC at 1920x1080, and its extracted last frame did not contain the overlay marker. |

## Unavailable cases

Normal, maximized, borderless-fullscreen, exclusive-fullscreen, monitor-change, and third-party overlay coexistence cases were not run in this phase. The supplied machine has one practical display fixture and no separate hookable game application; these cases must remain explicit acceptance limitations rather than being marked as passed.

## Technical conclusion

`SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` is suitable for the MVP's best-effort exclusion model and works with the tested Display Capture path. It is not a universal guarantee for every recorder, compositor, driver, or exclusive-fullscreen path; the product wording must say **excluded on supported Windows capture paths where the OS honors the affinity**, not “never capturable.”

No capture-specific plugin code change is required. The existing dedicated Win32 overlay, `WDA_EXCLUDEFROMCAPTURE` setup, click-through styles, and nonfatal error handling are the correct implementation boundary for this phase.

## Runtime evidence

The latest OBS log was `obs-dev/config/obs-studio/logs/2026-09-04 15-25-28.txt`. Relevant entries include:

```text
[obs-status-indicators] capture exclusion enabled for overlay
[window-capture: 'Phase6 Window Capture'] update settings:
User added source 'Phase6 Window Capture' (window_capture) to scene 'Gameplay Test'
User added source 'Phase6 Game Capture' (game_capture) to scene 'Gameplay Test'
[game-capture: 'Phase6 Game Capture'] attempting to hook process: Notepad.exe
[game-capture: 'Phase6 Game Capture'] hook not loaded yet, retrying..
[game-capture: 'Phase6 Game Capture'] capture stopped
```
