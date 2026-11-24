---
name: build-mumble-windows
description: Build and launch the Mumble client of this fork on Windows from Claude Code. Use whenever a change needs compiling, a build failed, or the app must be run to check a change.
---

# Build and run the Mumble client (Windows)

## Build

1. Close a running Mumble first, or linking fails with LNK1104 (the script refuses).
   **Always ask the user first and wait for a yes**, every time, even if they
   said yes earlier: the user uses this build for live calls, and a silent kill
   looks exactly like a crash. Check with `Get-Process mumble -ErrorAction SilentlyContinue`;
   if it is running, ask, and only after the yes run:

   ```powershell
   Get-Process mumble -ErrorAction SilentlyContinue | Stop-Process
   ```

2. Run the low-load build from **PowerShell**, in the background (a full build
   takes a long time; an incremental one is much shorter):

   ```powershell
   cmd /c "C:\Users\Meh\Desktop\Projects\muminahommia\build_windows_low_load.bat" --no-pause
   ```

   - Always the full path and `cmd /c`: from Bash, `build_windows.bat` alone gives
     "is not recognized as an internal or external command".
   - Always `--no-pause`, or the script waits for a key press forever.
   - Low-load (half the CPU threads) keeps the user's PC usable. Use
     `build_windows.bat` only if the user asks for a faster build.

3. On failure, look for the first `error C`/`error LNK`/`FAILED:` line in the
   output, not the last lines. `ninja` and `vswhere` "not recognized" errors
   mean the command was run outside the script.

## Run

```powershell
Start-Process "C:\Users\Meh\Desktop\Projects\muminahommia\build\mumble.exe"
```

Keep `mumble.exe` in `build\` (it loads the plugins next to it). Ask the user to
check the behaviour; you can't see the Mumble window.

## Facts

- Output: `C:\Users\Meh\Desktop\Projects\muminahommia\build\mumble.exe` (Release, static, Qt 5).
- Dependencies: `mumble-build-env\` (4.5 GB, downloaded once by the script). Don't delete it.
- CMake is configured once (`build\build.ninja`). New `.cpp/.h/.ui` files must be
  listed in `src/mumble/CMakeLists.txt`; Ninja picks that up by itself.
- Qt 5 only: this is the 1.5.x line. Upstream master needs Qt 6 and does not build here.
