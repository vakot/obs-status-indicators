# Local testing flow

This is the reproducible end-to-end flow for the supplied portable OBS installation. It assumes PowerShell, Visual Studio Build Tools 2022, CMake, FFmpeg, and the repository paths shown below.

## 1. Enter the repository

```powershell
Set-Location 'C:\Users\vakot\Documents\GitHub\obs-status-indicators'
```

Keep OBS stopped while building and installing the plugin. The startup script below closes any existing `obs64` process, waits for graceful shutdown, force-stops only remaining OBS processes if necessary, verifies that no OBS process remains, and then starts the portable fixture with the `Sync_Replay_Dev` profile.

## 2. Build, test, and install

```powershell
$vsdev = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat'
$cmake = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

cmd /c "call `"$vsdev`" -arch=x64 -host_arch=x64 >NUL && `"$cmake`" --build --preset windows-x64 --parallel 8"
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }

& $cmake --build build_x64 --config RelWithDebInfo --target RUN_TESTS
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }

& $cmake --install build_x64 --config RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw 'Install failed' }

Get-Item .\obs-dev\obs-plugins\64bit\obs-status-indicators.dll
Get-Item .\obs-dev\data\obs-plugins\obs-status-indicators\locale\en-US.ini
```

The configured CTest target is `RUN_TESTS` because this build uses the Visual Studio generator; `--target test` is not the correct target for this generator.

The indicator glyphs are selected Lucide SVG assets. The pinned `lucide-static` package is a development-only asset source; refresh the committed assets only when changing the icon set:

```powershell
npm install
npm run sync-icons
```

The running plugin uses the Qt6Svg library shipped with the OBS fixture, so Node.js is not required at runtime.

## 3. Start exactly one OBS process

```powershell
& .\scripts\start.ps1
```

If an unclean previous stop shows the `OBS Studio Crash Detected` window, close that window and wait for the normal `OBS Status Indicators` window before testing.

## 4. Inspect startup

After the script returns, wait for the main window and inspect the newest log:

```powershell
Get-Process -Name obs64 | Select-Object Id,MainWindowTitle,Responding
Get-ChildItem .\obs-dev\config\obs-studio\logs -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1 |
    ForEach-Object { Get-Content $_.FullName | Select-String 'obs-status-indicators|overlay window|state snapshot|Replay Buffer' }
```

Expected startup markers include `plugin loaded`, `state provider started`, `overlay window ready`, and a state snapshot. Logs are under `obs-dev/config/obs-studio/logs`, not the repository root.

## 5. Fixture configuration

Use the portable profile `Sync_Replay_Dev` and scene collection `Sync_Replay_Dev`.

- `Gameplay Test`: the main `Display Capture` source (`monitor_capture`, DXGI, LG SMARTGAME+, 3840x2160).
- `Camera Test`: the unavailable DirectShow camera fixture; it is useful for source-loss tolerance but is not a valid video capture acceptance scene.
- `Combined Reference`: display plus camera source.
- Global audio: WASAPI desktop audio and a WASAPI input using the USB PnP Audio Device.
- Replay Buffer: enabled with a 90 second buffer and NVENC.
- Recording output: the configured Videos directory, normally 1920x1080 NVENC/H.264.

Keep the websocket configuration disabled for ordinary manual tests:

```powershell
Get-Content .\obs-dev\config\obs-studio\plugin_config\obs-websocket\config.json
```

It should report `server_enabled: false`. For an API-driven experiment, stop OBS first, back up that JSON, set `server_enabled` to `true` and `auth_required` to `false`, run the experiment, stop OBS, and restore the exact backup before the next test.

## 6. Settings dialog and persistence checks

Open `Tools > Status Indicators` in the standalone OBS window. The dialog is modeless and exposes:

- `Origin`: top-left, top-right, bottom-left, or bottom-right of the primary display;
- `Orientation`: vertical or horizontal;
- `Offset`: equal pixel distance from the selected corner's two edges;
- `Gap`: transparent pixel distance between indicator tiles;
- `Indicator size`: square tile width and height from 16px to 256px;
- `Background color`: solid tile fill color;
- `Icon color`: solid Lucide stroke color;
- `Opacity`: shared complete-indicator opacity from 1% to 100%.

The controls are grouped as `Layout` (Origin and Orientation side by side), `Spacing` (Indicator size and Gap side by side, with Offset below), and `Appearance` (Background color and Icon color side by side, with Opacity below them).

Use `Apply` and verify the overlay moves or repaints immediately. Use `Cancel` after changing a control and verify that unsaved values are discarded. The persisted file is:

```powershell
Get-Content .\obs-dev\config\obs-studio\plugin_config\obs-status-indicators\settings.json
```

The JSON uses `origin`, `orientation`, `offset`, `gap`, `indicator_size`, `background_color`, `icon_color`, and `opacity`. Color integers use opaque Qt `0xFFRRGGBB` representation; any alpha byte in an older file is ignored. Icon padding is calculated as `max(1, round(indicator_size / 6))`, producing the current 8px padding at the default 48px size. Stop OBS before editing this file directly, then restart the single fixture instance to test load-time persistence and malformed-value fallback.

## 7. State and indicator checks

Perform these one at a time in the running standalone instance and correlate the visible overlay with the newest log snapshots:

The overlay is anchored to the configured corner and spacing. Each active state is a square of the configured size whose complete tile, including its Lucide icon, is composited at the configured shared opacity (65% by default); icon padding scales with tile size and is 8px at the default 48px size. Multiple states use the configured orientation and gap. Recording-only uses `circle-dot`, replay-only uses `refresh-ccw`, and recording plus replay is combined into one indicator using `refresh-ccw-dot`. Paused recording uses `pause`; microphone and saving retain their action glyphs, and a muted microphone uses `mic-off`.

1. Start Recording. Expect the `circle-dot` indicator; pause recording and expect `PAUSED` to replace it; resume and stop.
2. Start Replay Buffer. Expect the `refresh-ccw` indicator. Start Recording while Replay Buffer remains active and verify the two states collapse into one `refresh-ccw-dot` indicator. Stop Recording and verify the replay-only icon returns; use `Save Replay Buffer`, expect a bounded `SAVING` state after the save-completion event, then its expiry; stop Replay Buffer.
3. Toggle the USB microphone mute control in the Audio Mixer. Expect `MIC` to remain present and carry the muted presentation; unmute and verify the red muted presentation clears.
4. Switch between `Gameplay Test` and `Camera Test`. The unavailable camera must not remove the other state indicators or crash the plugin.

## 8. Capture exclusion experiment

For Display Capture, stay on `Gameplay Test`, make sure the overlay is showing, and create both a desktop still and a short recording. Inspect artifacts, not only API return values:

```powershell
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$bounds = [Windows.Forms.Screen]::PrimaryScreen.Bounds
$bitmap = New-Object Drawing.Bitmap($bounds.Width, $bounds.Height)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bounds.Size)
$bitmap.Save('.\tmp\desktop-capture.png', [Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose(); $bitmap.Dispose()

# Start and stop a 4-6 second recording from the OBS UI, then inspect the newest file.
$recording = Get-ChildItem "$env:USERPROFILE\Videos" -File |
    Where-Object { $_.Extension -in '.mp4','.mkv' } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
& (Get-Command ffprobe.exe).Source -v error -show_entries format=duration:stream=codec_name,width,height -of json -- $recording.FullName
& (Get-Command ffmpeg.exe).Source -y -loglevel error -sseof -1 -i $recording.FullName -frames:v 1 .\tmp\recording-last-frame.png
```

The expected result is desktop/game content without the overlay marker. For Window Capture, open Notepad and add a `Window Capture` source targeting `Untitled - Notepad` with the default/BitBlt method; the source must capture the Notepad client area. For Game Capture, use a real hookable DirectX/OpenGL game or test application and record whether a hook is established; Notepad is deliberately not a valid Game Capture target.

Also record the cases that are not available on the fixture: normal/maximized/borderless/exclusive fullscreen, monitor changes, and coexistence with Discord/Steam/NVIDIA overlays. Do not mark them passed without an actual captured frame.

## 9. Topmost ordering check

With the overlay visible, open a topmost utility such as Start11's custom control panel or another always-on-top test window. Interact with that panel repeatedly, then verify that the OBS indicator remains above it without changing OBS recording or Replay Buffer state. The renderer listens for foreground and top-level window show/hide/reorder events and reasserts `HWND_TOPMOST` on the overlay UI thread; it does not use a periodic timer. Confirm the startup log contains:

```text
[obs-status-indicators] topmost order recovery hooks enabled
```

If the third-party utility still wins z-order, record its windowing mode and whether it uses a compositor or exclusive-fullscreen path; Windows does not expose an absolute immutable “highest topmost” level.

## 10. Teardown and final assertion

Close OBS gracefully after each experiment. If a process remains, stop that exact process before launching anything else. At the end:

```powershell
$obs = @(Get-Process -Name obs64 -ErrorAction SilentlyContinue)
if ($obs.Count -gt 0) { $obs | ForEach-Object { $_.CloseMainWindow() | Out-Null } }
Start-Sleep -Seconds 5
$obs = @(Get-Process -Name obs64 -ErrorAction SilentlyContinue)
if ($obs.Count -gt 0) { $obs | ForEach-Object { Stop-Process -Id $_.Id -Force } }
if (@(Get-Process -Name obs64 -ErrorAction SilentlyContinue).Count -ne 0) { throw 'OBS is still running' }

$ws = Get-Content -Raw .\obs-dev\config\obs-studio\plugin_config\obs-websocket\config.json | ConvertFrom-Json
if ($ws.server_enabled) { throw 'websocket must be disabled after experiments' }
```
