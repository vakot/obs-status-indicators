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
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..') -ErrorAction Stop).Path
$vsDevCmd = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat'
$cmake = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$buildDirectory = Join-Path $repositoryRoot 'build_x64'

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

function Invoke-CMakeCommand {
    param(
        [Parameter(Mandatory)]
        [string] $Arguments,

        [Parameter(Mandatory)]
        [string] $Description
    )

    Write-Host "$Description..."
    $command = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 >NUL && `"$cmake`" $Arguments"
    cmd /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

$resolvedObsRoot = (Resolve-Path -LiteralPath $ObsRoot -ErrorAction Stop).Path
$executable = Join-Path $resolvedObsRoot 'bin\64bit\obs64.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Standalone OBS executable was not found: $executable"
}

Stop-ExistingObs

if (-not (Test-Path -LiteralPath $vsDevCmd -PathType Leaf)) {
    throw "Visual Studio developer environment was not found: $vsDevCmd"
}
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    throw "CMake executable was not found: $cmake"
}
if (-not (Test-Path -LiteralPath (Join-Path $buildDirectory 'CMakeCache.txt') -PathType Leaf)) {
    throw "CMake build directory is not configured: $buildDirectory. Run 'cmake --preset windows-x64' first."
}

Push-Location $repositoryRoot
try {
    Invoke-CMakeCommand '--build --preset windows-x64 --parallel 8' 'Building the latest plugin'
    Invoke-CMakeCommand '--install build_x64 --config RelWithDebInfo' 'Installing the latest plugin into standalone OBS'
}
finally {
    Pop-Location
}

$installedPlugin = Join-Path $resolvedObsRoot 'obs-plugins\64bit\obs-status-indicators.dll'
if (-not (Test-Path -LiteralPath $installedPlugin -PathType Leaf)) {
    throw "Installed plugin was not found: $installedPlugin"
}
Write-Host "Updated standalone plugin: $installedPlugin"

$arguments = @('--portable', '--profile', $Profile)
$started = Start-Process `
    -FilePath $executable `
    -WorkingDirectory (Split-Path -Parent $executable) `
    -ArgumentList $arguments `
    -PassThru

Write-Host "Started standalone OBS (PID $($started.Id)) with profile '$Profile'."
