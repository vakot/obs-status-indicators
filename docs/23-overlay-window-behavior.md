# Overlay window behavior investigation

Date: 2026-09-09

## Verdict

**Possible and fixed for the supported Windows topmost model.** The indicator window now behaves as a persistent, non-activating, click-through overlay and survives the tested `Win+D` show-desktop transition.

## Findings

The previous window already used `WS_EX_LAYERED`, `WS_EX_TRANSPARENT`, `WS_EX_NOACTIVATE`, `WS_EX_TOOLWINDOW`, and `HTTRANSPARENT`. Those settings explain the intended click-through and no-focus behavior, but the window was not created with `WS_EX_TOPMOST`; it only requested `HWND_TOPMOST` during selected positioning operations. Startup and shell z-order changes could therefore leave it below another window.

The previous recovery hook also only reasserted z-order. If Windows or the shell attempted to hide the overlay during Show Desktop, the recovery path did not explicitly restore visibility.

## Changes

- Create the popup with `WS_EX_TOPMOST` in addition to the existing layered, transparent, non-activating tool-window styles.
- Register a shell hook alongside the existing foreground and object z-order hooks.
- Reassert topmost state with `SWP_SHOWWINDOW` whenever the active layout contains indicators.
- Intercept `WM_WINDOWPOSCHANGING` to reject external hide requests while indicators are active.
- Ignore focus activation and external close requests in the overlay window procedure.
- Preserve `HTTRANSPARENT` and `MA_NOACTIVATE`; the overlay does not register or process application hotkeys.

The recovery remains event-driven. It does not use a periodic timer or re-promote the overlay every few seconds. Windows does not provide an absolute z-order above every other `HWND_TOPMOST` window, so a third-party compositor or privileged/exclusive-fullscreen surface remains outside the guarantee.

## Validation

With one standalone OBS instance, launch:

```powershell
$obs = (Resolve-Path .\obs-dev\bin\64bit\obs64.exe).Path
Start-Process -FilePath $obs -WorkingDirectory (Split-Path $obs) -ArgumentList @(
    '--portable', '--profile', 'Sync_Replay_Dev',
    '--startreplaybuffer', '--minimize-to-tray'
)
```

The final runtime check confirmed:

- the startup arguments were received;
- Replay Buffer and microphone indicators appeared;
- the log reported both WinEvent and shell recovery hooks enabled;
- the overlay extended styles included `WS_EX_TOPMOST`, `WS_EX_NOACTIVATE`, `WS_EX_TRANSPARENT`, `WS_EX_TOOLWINDOW`, and `WS_EX_LAYERED`;
- the overlay remained visible before, during, and after `Win+D`;
- no overlay message or paint errors were logged.
