# Windows installation

The project currently ships as a portable ZIP bundle for Windows x64. The package contains the plugin DLL and its locale/icon resources; OBS Studio itself is not included.

## Build the package

From PowerShell at the repository root, configure the Windows x64 preset if `build_x64` does not exist:

```powershell
$cmake = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake --preset windows-x64
```

Then build the package:

```powershell
.\scripts\package-windows.ps1
```

The script builds the selected configuration and writes:

```text
release/obs-status-indicators-windows-x64-0.1.0.zip
```

To use another configuration or build directory:

```powershell
.\scripts\package-windows.ps1 -Configuration Release -BuildDir .\build_x64
```

The script uses `cmake` from `PATH` when available and otherwise checks the standard Visual Studio Build Tools installation. Use `-CMakePath` if CMake is installed elsewhere.

The package layout is:

```text
obs-plugins/
└── 64bit/
    └── obs-status-indicators.dll
data/
└── obs-plugins/
    └── obs-status-indicators/
        ├── icons/
        ├── locale/
        └── ...
```

## Install into OBS Studio

1. Exit every OBS Studio process. Do not copy plugin files while OBS is running.
2. Extract the ZIP into the OBS Studio installation root, preserving the package folders.
   - Typical installation: `C:\Program Files\obs-studio`
   - Portable installation: the directory containing `bin\64bit\obs64.exe`
3. Approve the administrator prompt if OBS is installed under `Program Files`.
4. Start OBS and open `Tools > Status Indicators` to verify the plugin.

After extraction, the two important paths under the OBS root are:

```text
obs-plugins\64bit\obs-status-indicators.dll
data\obs-plugins\obs-status-indicators\locale\en-US.ini
```

No Node.js installation is required at runtime. The package is built for 64-bit Windows and requires an OBS Studio x64 installation compatible with the build.

## Uninstall or reset

Exit OBS, then remove these package-owned paths from the OBS installation root:

```text
obs-plugins\64bit\obs-status-indicators.dll
data\obs-plugins\obs-status-indicators\
```

Plugin settings are stored separately from the installed files:

```text
%APPDATA%\obs-studio\plugin_config\obs-status-indicators\settings.json
```

For a portable OBS installation, use the equivalent `config\obs-studio\plugin_config\obs-status-indicators\settings.json` under the portable root. Remove that JSON only when resetting saved indicator settings is intended.
