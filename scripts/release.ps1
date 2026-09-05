[CmdletBinding()]
param(
    [Parameter()]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string] $Version,

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $Configuration = 'RelWithDebInfo',

    [Parameter()]
    [switch] $Sign,

    [Parameter()]
    [string] $SignToolPath,

    [Parameter()]
    [string] $CertificateThumbprint,

    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string] $TimestampUrl = 'http://timestamp.digicert.com',

    [Parameter()]
    [switch] $DryRun
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..') -ErrorAction Stop).Path
$vsDevCmd = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat'
$cmake = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$buildDirectory = Join-Path $repositoryRoot 'build_x64'
$releaseDirectory = Join-Path $repositoryRoot 'release'

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory)]
        [string] $FilePath,

        [Parameter(Mandatory)]
        [string[]] $Arguments,

        [Parameter(Mandatory)]
        [string] $Description
    )

    Write-Host "$Description..."
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Invoke-CMakeCommand {
    param(
        [Parameter(Mandatory)]
        [string] $Arguments,

        [Parameter(Mandatory)]
        [string] $Description
    )

    $command = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 >NUL && `"$cmake`" $Arguments"
    Invoke-CheckedCommand 'cmd.exe' @('/d', '/c', $command) $Description
}

function Get-BuildSpecVersion {
    $buildSpec = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'buildspec.json') | ConvertFrom-Json
    return [string] $buildSpec.version
}

function Invoke-GitText {
    param([Parameter(Mandatory)][string[]] $Arguments)

    $result = & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE."
    }
    return ($result -join "`n").Trim()
}

function Ensure-ReleasePrerequisites {
    $currentBranch = Invoke-GitText @('branch', '--show-current')
    if ($currentBranch -ne 'master') {
        throw "Release must be created from master; current branch is '$currentBranch'."
    }

    $status = @(git status --porcelain --untracked-files=all)
    if ($status.Count -gt 0) {
        throw 'Working tree is not clean. Commit or remove changes before releasing.'
    }

    if (-not (Test-Path -LiteralPath $vsDevCmd -PathType Leaf)) {
        throw "Visual Studio developer environment was not found: $vsDevCmd"
    }
    if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
        throw "CMake executable was not found: $cmake"
    }
    if (-not (Get-Command gh.exe -ErrorAction SilentlyContinue)) {
        throw 'GitHub CLI (gh.exe) is required for release creation and asset upload.'
    }
}

function Update-ToLatestMaster {
    Invoke-CheckedCommand 'git' @('fetch', 'origin', 'master', '--tags') 'Fetching latest master and tags'
    $localMaster = Invoke-GitText @('rev-parse', 'master')
    $remoteMaster = Invoke-GitText @('rev-parse', 'origin/master')
    if ($localMaster -ne $remoteMaster) {
        & git merge-base --is-ancestor master origin/master
        if ($LASTEXITCODE -ne 0) {
            throw 'Local master is not an ancestor of origin/master; refusing to release from a diverged branch.'
        }
        Invoke-CheckedCommand 'git' @('pull', '--ff-only', 'origin', 'master') 'Fast-forwarding master'
    }
}

function Get-TagName {
    return "v$Version"
}

function Ensure-TagDoesNotExist {
    $tag = Get-TagName
    if ((Invoke-GitText @('tag', '--list', $tag))) {
        throw "Local tag already exists: $tag"
    }
    $remoteTag = & git ls-remote --tags origin "refs/tags/$tag"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not check remote tag: $tag"
    }
    if ($remoteTag) {
        throw "Remote tag already exists: $tag"
    }
}

function New-ReleasePackage {
    $tag = Get-TagName
    $stagingDirectory = Join-Path $releaseDirectory "staging-$Version"
    $artifact = Join-Path $releaseDirectory "obs-status-indicators-windows-x64-$tag.zip"
    $checksums = Join-Path $releaseDirectory "obs-status-indicators-windows-x64-$tag-SHA256.txt"

    if (Test-Path -LiteralPath $stagingDirectory) {
        Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
    }
    New-Item -ItemType Directory -Path (Join-Path $stagingDirectory 'obs-plugins\64bit') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $stagingDirectory 'data\obs-plugins') -Force | Out-Null

    $binary = Join-Path $buildDirectory "$Configuration\obs-status-indicators.dll"
    $data = Join-Path $repositoryRoot 'data'
    $dataDestination = Join-Path $stagingDirectory 'data\obs-plugins\obs-status-indicators'
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Built plugin DLL was not found: $binary"
    }
    if (-not (Test-Path -LiteralPath $data -PathType Container)) {
        throw "Plugin data directory was not found: $data"
    }

    Copy-Item -LiteralPath $binary -Destination (Join-Path $stagingDirectory 'obs-plugins\64bit\obs-status-indicators.dll')
    New-Item -ItemType Directory -Path $dataDestination -Force | Out-Null
    Copy-Item -Path (Join-Path $data '*') -Destination $dataDestination -Recurse -Force

    $installText = @"
OBS Status Indicators $tag

Installation/update:
1. Close OBS completely.
2. Extract this archive into the OBS installation directory.
3. Allow the obs-plugins and data directories to merge.
4. Start OBS again.

The plugin is a native Windows x64 OBS module and requires a compatible OBS installation.
"@
    Set-Content -LiteralPath (Join-Path $stagingDirectory 'INSTALL.txt') -Value $installText -Encoding utf8NoBOM

    $manifest = [ordered]@{
        name = 'obs-status-indicators'
        version = $Version
        tag = $tag
        platform = 'windows-x64'
        configuration = $Configuration
        commit = (Invoke-GitText @('rev-parse', 'HEAD'))
        signed = $Sign.IsPresent
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stagingDirectory 'release-manifest.json') -Encoding utf8NoBOM

    if (Test-Path -LiteralPath $artifact) {
        Remove-Item -LiteralPath $artifact -Force
    }
    Compress-Archive -Path (Join-Path $stagingDirectory '*') -DestinationPath $artifact -CompressionLevel Optimal
    $hash = (Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath $checksums -Value "$hash  $([System.IO.Path]::GetFileName($artifact))" -Encoding ascii
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force

    return @($artifact, $checksums)
}

if ([string]::IsNullOrEmpty($Version)) {
    $Version = Get-BuildSpecVersion
}
$buildSpecVersion = Get-BuildSpecVersion
if ($Version -ne $buildSpecVersion) {
    throw "Release version '$Version' does not match buildspec.json version '$buildSpecVersion'."
}

Push-Location $repositoryRoot
try {
    Ensure-ReleasePrerequisites
    Update-ToLatestMaster
    Ensure-TagDoesNotExist

    if ($Sign) {
        if ([string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
            throw '-CertificateThumbprint is required when -Sign is specified.'
        }
        if ([string]::IsNullOrWhiteSpace($SignToolPath)) {
            $signTool = Get-Command signtool.exe -ErrorAction SilentlyContinue
            if (-not $signTool) {
                throw 'signtool.exe was not found. Provide -SignToolPath or install the Windows SDK signing tools.'
            }
            $SignToolPath = $signTool.Source
        }
    }

    if (-not $DryRun) {
        $tag = Get-TagName
        Invoke-CheckedCommand 'git' @('tag', '-a', $tag, '-m', "Release $tag") "Creating tag $tag"
        Invoke-CheckedCommand 'git' @('push', 'origin', $tag) "Pushing tag $tag"
        Invoke-CheckedCommand 'gh' @('release', 'create', $tag, '--draft', '--generate-notes', '--title', $tag, '--verify-tag') "Creating draft GitHub release $tag"
    }

    Invoke-CMakeCommand "--build --preset windows-x64 --config $Configuration --parallel 8" 'Building release binaries'
    Invoke-CMakeCommand "--build build_x64 --config $Configuration --target RUN_TESTS" 'Running release tests'

    if ($Sign) {
        $binary = Join-Path $buildDirectory "$Configuration\obs-status-indicators.dll"
        Invoke-CheckedCommand $SignToolPath @('sign', '/fd', 'SHA256', '/sha1', $CertificateThumbprint, '/tr', $TimestampUrl, '/td', 'SHA256', $binary) 'Signing release binary'
        Invoke-CheckedCommand $SignToolPath @('verify', '/pa', $binary) 'Verifying release signature'
    }

    $artifacts = New-ReleasePackage
    Write-Host "Built artifacts:"
    $artifacts | ForEach-Object { Write-Host "  $_" }

    if (-not $DryRun) {
        $tag = Get-TagName
        foreach ($artifact in $artifacts) {
            Invoke-CheckedCommand 'gh' @('release', 'upload', $tag, $artifact, '--clobber') "Uploading $([System.IO.Path]::GetFileName($artifact))"
        }
        Invoke-CheckedCommand 'gh' @('release', 'edit', $tag, '--draft=false', '--latest', '--verify-tag') "Publishing GitHub release $tag"
        Write-Host "Published release: $tag"
    } else {
        Write-Host 'Dry run complete; no tag, GitHub release, or upload was created.'
    }
}
finally {
    Pop-Location
}
