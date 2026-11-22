# Building this customised Mumble on Windows

This repository is Mumble 1.5.x with local customisations. It builds from a plain ZIP download, no git needed.

## Requirements (one-time)
1. **Visual Studio 2022 or newer** (the free Community edition or the Build Tools are fine) with the workload **"Desktop development with C++"**: https://visualstudio.microsoft.com/
2. **Python 3**, with **"Add python.exe to PATH"** ticked in the installer: https://www.python.org/downloads/
3. About **10 GB** of free disk space and an internet connection for the first build.

## Build
Double-click **`build_windows.bat`** in this folder, or run it from a terminal.

On the first run it downloads Mumble's prebuilt dependencies (about 320 MB, extracted to about 4.5 GB in `mumble-build-env\`), configures the build and compiles everything. That takes a while. Later runs only rebuild what changed.

The result is **`build\mumble.exe`**. Run it from there, since it loads the plugins in the same folder.

## Troubleshooting
- The script stops with a clear message if something is missing (Visual Studio, Python, CMake tools) and says what to install.
- `LNK1104: cannot open file ...dll`: Mumble is still running from `build\`. Close it and run the script again.
- If the first configuration failed, delete the `build\` folder before trying again.
- Upstream's general build documentation: `docs\dev\build-instructions\`.
