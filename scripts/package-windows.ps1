[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'Release',

    [Parameter()]
    [string] $BuildDir = (Join-Path $PSScriptRoot '..\build_x64'),

    [Parameter()]
    [string] $OutputDir = (Join-Path $PSScriptRoot '..\release'),

    [Parameter()]
    [string] $CMakePath
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir -ErrorAction Stop).Path
$buildspec = Get-Content -Raw (Join-Path $repositoryRoot 'buildspec.json') | ConvertFrom-Json
$version = $buildspec.version
$cmakeCommand = if ($CMakePath) {
    $CMakePath
} else {
    $pathCommand = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue
    if ($pathCommand) {
        $pathCommand.Source
    } else {
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    }
}
$cmake = (Resolve-Path -LiteralPath $cmakeCommand -ErrorAction Stop).Path
$binary = Join-Path $resolvedBuildDir "$Configuration\obs-status-indicators.dll"
$stagingRoot = Join-Path ([System.IO.Path]::GetTempPath()) "obs-status-indicators-package-$PID"
$archive = Join-Path ((Resolve-Path (New-Item -ItemType Directory -Force -Path $OutputDir)).Path) "obs-status-indicators-windows-x64-$version.zip"

try {
    & $cmake --build $resolvedBuildDir --config $Configuration --parallel 8
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Built plugin was not found: $binary"
    }

    New-Item -ItemType Directory -Force -Path (Join-Path $stagingRoot 'obs-plugins\64bit') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $stagingRoot 'data\obs-plugins\obs-status-indicators') | Out-Null
    Copy-Item -LiteralPath $binary -Destination (Join-Path $stagingRoot 'obs-plugins\64bit\obs-status-indicators.dll')
    Copy-Item -Path (Join-Path $repositoryRoot 'data\*') -Destination (Join-Path $stagingRoot 'data\obs-plugins\obs-status-indicators') -Recurse -Force

    if (Test-Path -LiteralPath $archive) {
        Remove-Item -LiteralPath $archive -Force
    }

    Compress-Archive -Path (Join-Path $stagingRoot '*') -DestinationPath $archive -CompressionLevel Optimal
    Write-Output "Created package: $archive"
}
finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
