[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string] $Artifact,

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $ObsRoot = (Join-Path $PSScriptRoot '..\obs-dev')
)

$ErrorActionPreference = 'Stop'

function Get-ObsProcesses {
    @(Get-Process -Name obs64 -ErrorAction SilentlyContinue | Where-Object { $_ })
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

    if ((Get-ObsProcesses).Count -gt 0) {
        throw 'OBS is still running; refusing to update installed files.'
    }
}

$resolvedArtifact = (Resolve-Path -LiteralPath $Artifact -ErrorAction Stop).Path
$resolvedObsRoot = (Resolve-Path -LiteralPath $ObsRoot -ErrorAction Stop).Path
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) "obs-status-indicators-update-$PID"

if ([System.IO.Path]::GetExtension($resolvedArtifact) -ne '.zip') {
    throw "Release artifact must be a ZIP file: $resolvedArtifact"
}

if (Test-Path -LiteralPath $temporaryRoot) {
    Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null

try {
    Expand-Archive -LiteralPath $resolvedArtifact -DestinationPath $temporaryRoot -Force

    $pluginSource = Join-Path $temporaryRoot 'obs-plugins\64bit\obs-status-indicators.dll'
    $dataSource = Join-Path $temporaryRoot 'data\obs-plugins\obs-status-indicators'
    if (-not (Test-Path -LiteralPath $pluginSource -PathType Leaf)) {
        throw "Release artifact is missing the plugin DLL: $pluginSource"
    }
    if (-not (Test-Path -LiteralPath $dataSource -PathType Container)) {
        throw "Release artifact is missing plugin data: $dataSource"
    }

    Stop-ExistingObs

    Copy-Item -LiteralPath (Join-Path $temporaryRoot 'obs-plugins') -Destination $resolvedObsRoot -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $temporaryRoot 'data') -Destination $resolvedObsRoot -Recurse -Force
    Write-Host "Updated OBS plugin from $resolvedArtifact"
    Write-Host "Installed plugin: $(Join-Path $resolvedObsRoot 'obs-plugins\64bit\obs-status-indicators.dll')"
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
