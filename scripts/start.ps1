[CmdletBinding()]
param(
    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $Profile = 'Sync_Replay_Dev',

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $ObsRoot = (Join-Path $PSScriptRoot '..\obs-dev')
)

$ErrorActionPreference = 'Stop'

function Get-ObsProcesses {
    @(Get-Process -Name obs64 -ErrorAction SilentlyContinue)
}

function Stop-ExistingObs {
    $processes = Get-ObsProcesses
    if ($processes.Count -eq 0) {
        return
    }

    Write-Host "Closing $($processes.Count) existing OBS process(es)..."
    foreach ($process in $processes) {
        $null = $process.CloseMainWindow()
    }

    $deadline = (Get-Date).AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 250
        $processes = Get-ObsProcesses
    } while ($processes.Count -gt 0 -and (Get-Date) -lt $deadline)

    if ($processes.Count -gt 0) {
        Write-Warning 'OBS did not close gracefully; stopping the remaining process(es).'
        foreach ($process in $processes) {
            Stop-Process -Id $process.Id -Force
        }
    }

    $deadline = (Get-Date).AddSeconds(15)
    while ((Get-ObsProcesses).Count -gt 0 -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 250
    }

    if ((Get-ObsProcesses).Count -gt 0) {
        throw 'OBS is still running; refusing to start another instance.'
    }
}

$resolvedObsRoot = (Resolve-Path -LiteralPath $ObsRoot -ErrorAction Stop).Path
$executable = Join-Path $resolvedObsRoot 'bin\64bit\obs64.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Standalone OBS executable was not found: $executable"
}

Stop-ExistingObs

$arguments = @('--portable', '--profile', $Profile)
$started = Start-Process `
    -FilePath $executable `
    -WorkingDirectory (Split-Path -Parent $executable) `
    -ArgumentList $arguments `
    -PassThru

Write-Host "Started standalone OBS (PID $($started.Id)) with profile '$Profile'."
