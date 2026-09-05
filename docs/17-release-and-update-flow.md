# Release and update flow

Date: 2026-09-05

## Scope and policy

Releases are intentionally local-build operations. There are no GitHub Actions workflows in this repository. The local release script uses GitHub CLI only for tag publication, draft release creation, generated release notes, asset upload, and final publication.

The first release version is `0.0.1`, recorded in `buildspec.json`. The release tag and the compiled plugin metadata must remain aligned, so the release script rejects a requested version that differs from `buildspec.json`.

## One-command release

Run this from a clean, up-to-date local `master` checkout:

```powershell
Set-Location 'C:\Users\vakot\Documents\GitHub\obs-status-indicators'
.\scripts\release.ps1
```

The script performs the following sequence:

1. Requires the current branch to be `master` and the working tree to be clean.
2. Fetches `origin/master` and tags, then fast-forwards local `master` when it is safely behind.
3. Creates and pushes the annotated tag `v0.0.1`.
4. Creates a draft GitHub release from that tag with GitHub-generated release notes.
5. Builds the Windows x64 `RelWithDebInfo` binary and runs the local CTest suite.
6. Produces a ZIP containing the plugin DLL, plugin data, `INSTALL.txt`, and `release-manifest.json`.
7. Produces a SHA-256 checksum file.
8. Uploads both files to the draft release and publishes it as the latest release.

The generated files are:

```text
release/obs-status-indicators-windows-x64-v0.0.1.zip
release/obs-status-indicators-windows-x64-v0.0.1-SHA256.txt
```

Before publishing the first real release, use the non-mutating build/package check:

```powershell
.\scripts\release.ps1 -DryRun
```

`-DryRun` still builds, tests, packages, and hashes the artifacts, but does not create or push a tag, create a GitHub release, or upload anything.

## Optional signing

Signing is not required for OBS to load the module or for an update to replace it. The update script requires OBS to be stopped and copies the plugin DLL and data directory into the OBS installation; no signature is checked by the script or by OBS.

For public Windows distribution, Authenticode signing is recommended because it identifies the publisher and can improve trust/reputation. It requires a real code-signing certificate, the Windows SDK `signtool.exe`, and a timestamp service. Signing is opt-in:

```powershell
.\scripts\release.ps1 -Sign -SignToolPath 'C:\path\to\signtool.exe' -CertificateThumbprint '<certificate-thumbprint>'
```

The release script signs the DLL before packaging, verifies the signature, and records `signed: true` in the manifest. Without a certificate, release `v0.0.1` remains unsigned; this is a distribution-trust limitation, not an OBS installation limitation.

Microsoft documents that Authenticode supports DLL files and that timestamping preserves signature validity after certificate expiration. Microsoft also notes that unsigned files build SmartScreen reputation separately for every update, so signing becomes more valuable as the project gains public distribution. See [Authenticode signing](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signedcode), [timestamping](https://learn.microsoft.com/en-us/windows/win32/seccrypto/time-stamping-authenticode-signatures), and [SmartScreen reputation](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation).

## Installing or updating from a release

The release ZIP is an overlay package. Close OBS and run:

```powershell
.\scripts\update.ps1 `
  -Artifact '.\release\obs-status-indicators-windows-x64-v0.0.1.zip' `
  -ObsRoot '.\obs-dev'
```

For a normal OBS installation, point `-ObsRoot` at its installation directory. The script validates the ZIP layout, closes any existing OBS process, copies `obs-plugins` and `data`, and refuses to continue if OBS cannot be stopped.

For development source changes, [`start.ps1`](../scripts/start.ps1) remains the fast path: it closes OBS, builds the current source, installs into `obs-dev`, and starts exactly one standalone instance.

## GitHub release mechanics

GitHub supports generated release notes and release asset uploads through the GitHub CLI/API. The script uses a draft release so assets can be attached before publication; this also remains compatible with repositories that enable immutable releases. See [automatically generated release notes](https://docs.github.com/en/repositories/releasing-projects-on-github/automatically-generated-release-notes), [GitHub release management](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository), and [release assets API](https://docs.github.com/en/rest/releases/assets).
