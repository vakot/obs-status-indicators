# Startup overlay visibility investigation

Date: 2026-09-07

## Verdict

**Possible and fixed.** The inconsistent startup visibility was caused by an unreliable delivery path for renderer updates, not by missing OBS state transitions or the `--minimize-to-tray` argument.

## Evidence

The supplied startup log shows the complete expected state sequence:

1. The plugin loads settings and reconciles an initial empty state.
2. The overlay window is created successfully and topmost hooks are installed.
3. OBS finishes loading the scene and resolves the microphone source.
4. The state changes to Replay Buffer active and continues to produce microphone snapshots.

No renderer paint or update errors were logged, but the original renderer posted layout and settings updates with `PostThreadMessageW`. The renderer signaled initialization immediately after creating the window, before entering `GetMessageW`. That left a window in which the target thread's message queue was not guaranteed to exist. Posts made by plugin startup or settings changes could therefore be rejected or lost, leaving the overlay hidden after its initial empty render.

## Fix

Renderer control messages now target the overlay HWND with `PostMessageW`:

- layout updates;
- settings updates;
- topmost reassertion;

The HWND is created before the renderer reports startup success, so the messages remain queued until the renderer's normal dispatch loop processes them. Post failures are logged. The existing pending-layout/pending-settings mutex continues to coalesce rapid updates safely.

## Runtime validation

Validate with only one standalone OBS process at a time:

```powershell
if (Get-Process -Name obs64 -ErrorAction SilentlyContinue) {
    Stop-Process -Name obs64 -Force
    Start-Sleep -Seconds 2
}

cmake --build build_x64 --config RelWithDebInfo
Copy-Item build_x64\RelWithDebInfo\obs-status-indicators.dll `
    obs-dev\obs-plugins\64bit\obs-status-indicators.dll -Force
```

Start the standalone instance with the affected arguments:

```powershell
$obs = (Resolve-Path .\obs-dev\bin\64bit\obs64.exe).Path
Start-Process -FilePath $obs -WorkingDirectory (Split-Path $obs) -ArgumentList @(
    '--portable', '--profile', 'Sync_Replay_Dev',
    '--startreplaybuffer', '--minimize-to-tray'
)
```

Repeat the launch after closing OBS, then open `Tools > Status Indicators`, change a setting, and apply it. The overlay must appear after Replay Buffer starts and must react to the live setting change. Confirm the OBS log contains no `overlay ... message failed` entries and that only one `obs64` process is running.

## Remaining limitation

The overlay still depends on Windows layered-window and topmost behavior. The fix removes the identified startup/update race; it does not claim that third-party window managers can never interfere with topmost ordering.
