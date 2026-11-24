# Gaming input lag audit (Windows)

Audit date: 2026-11-24, at commit `7c3a0072a` (after "Reduce input lag from global shortcuts on Windows").
Method: code reading only. Four Haiku scouts covered the overlay, positional audio/plugins, audio threads/timers, and other input/UI. Key claims were re-checked by hand. Nothing was profiled.

## Current state / Next step

- **Current state:** fixes 1 and 2 are implemented in `src/mumble/GlobalShortcut_win.cpp`/`.h` (2026-11-24). Gamepad reports only handle buttons that changed. XInput polls only connected slots and re-checks empty ones once a second. `m_stateMutex` is taken only around `handleButton()` for HID/XInput/GKey. Held buttons are re-reported after a remap. The local log (`%APPDATA%\Mumble\Mumble\Console.txt`) shows keyboard suppression in use, so the keyboard low-level hook is active. XInput is on (default).
- **Next step:** test with a gamepad in a game (button shortcuts still trigger, PTT on a pad works). Open follow-ups, all pre-existing:
  - `deviceRemoved()` runs on the GUI thread and erases from `m_devices` without a lock, which races with the engine thread.
  - The GKey block still queries the SDK under `m_stateMutex`. GKey is off by default.
  - A controller that disconnects with a button held leaves that button "down".
  - Fix 3 (raw mouse registered only when needed) is optional.

## How Mumble can lag a game

1. **Blocking the input path:** low-level hooks (`WH_KEYBOARD_LL`/`WH_MOUSE_LL`). Windows waits for Mumble's hook before *any* app sees the input. This is the only true input delay.
2. **Stealing CPU from the game:** high-rate wakeups (raw input, HID reports), DSP on high-priority threads.
3. **Work inside the game's frame:** the overlay DLL in `Present`.
4. **Breaking fullscreen flip:** any window drawn over a borderless game (toasts, topmost windows) makes DWM composite. That adds about one frame while visible.

## Findings, ranked

### Active with your setup

**1. Gamepad HID reports are fully processed at report rate, even with no gamepad shortcut bound.** *(biggest continuous cost if a controller is plugged in)*
- `GlobalShortcut_win.cpp:823-841` registers joystick, gamepad and multi-axis with `RIDEV_INPUTSINK`. Every report wakes the GUI thread (`WM_INPUT` → `injectRawInputMessage`, 2× `GetRawInputData` + a heap allocation) and the engine thread.
- `processMsgHid` (`:1014-1052`) calls `handleButton()` for **every** button on **every** report, changed or not. `handleButton` does `QVariant` list searches.
- `run()` calls `processOther()` (`:874`) before each HID message. With an Xbox controller present (`m_xinputDevices > 0`), that polls `XInputGetState` on **all 4 slots per report** (`:1245-1266`). Microsoft documents empty-slot polling as expensive.
- Many pads stream reports continuously at 250-1000 Hz, DualShock/DualSense even when idle. The Windows 11 throttling of background raw input covers mice, not HID gamepads.
- All of this runs while holding `m_stateMutex`, which links to finding 2.
- **Fix:** in `processMsgHid`, call `handleButton` only for buttons whose state changed (compare with `device.buttons` before overwriting). In `processOther`, poll only slots that returned `ERROR_SUCCESS` last time, and re-check empty slots rarely (e.g. once a second).

**2. A suppressed key can stall all system keyboard input behind slow engine-thread work.**
- Because a shortcut uses Suppress, `WH_KEYBOARD_LL` is installed. Non-suppressed keys pass lock-free (`:597-601`), which is good.
- A suppressed key calls `handleHookedButton` → locks `m_stateMutex` (`:566`). The engine thread holds that lock for each queued message (`:870`). That includes `addDevice()` (`:1104-1197`: `CreateFile` + `HidD_GetManufacturerString`/`HidD_GetProductString`, which is synchronous device I/O and slow on Bluetooth pads), XInput polling, and `remap()`.
- While the hook waits, Windows holds back every key on the system. If the wait passes `LowLevelHooksTimeout`, Windows silently removes the hook.
- **Fix:** take `m_stateMutex` only around the `handleButton` calls. Run `addDevice` and the XInput `GetState` calls outside it. `m_devices` and the XInput state are only touched by the engine thread.

**3. Raw mouse input wakes the GUI thread on every mouse event.**
- `:818-821` registers the mouse with `RIDEV_INPUTSINK`. Since `7c3a0072a`, movement is dropped after `GetRawInputData`, but the `WM_INPUT` wakeup still happens.
- Windows 11 (since KB5028185, July 2023) coalesces background raw *mouse* listeners to about 125 Hz, so on this PC (build 26200) the cost is small. On Windows 10 it runs at the full mouse rate (1-8 kHz).
- **Optional fix:** register the mouse only while a mouse shortcut exists or the shortcut dialog is open. Low priority on Windows 11.

**4. Echo cancellation runs all the time on a Pro Audio priority thread.** *(default)*
- Default `echoOption = SPEEX_MIXED` (`Settings.h:336`). It is active whenever input and output are both WASAPI (`WASAPI.cpp:227-230`). It opens a loopback capture (`WASAPI.cpp:443-448`) and runs the Speex echo filter per 10 ms frame (`AudioInput.cpp:773-775`).
- That thread has MMCSS "Pro Audio" (`WASAPI.cpp:433`), so it preempts game threads.
- **With headphones:** Settings → Audio input → Echo cancellation → *Disabled*. This is a free CPU win with no downside.

### Only when a feature is turned on (all off by default)

**5. Mouse-button suppression.** If any non-left mouse button shortcut has Suppress, `WH_MOUSE_LL` is installed (`:518-537`). Then **every mouse movement on the system** detours through Mumble's hook thread before the game gets it. The callback is short and the thread is time-critical, but it is still a cross-thread round trip per event at the mouse rate. **Avoid suppressing mouse buttons.**

**6. Overlay** (`os.bEnable = false`, `Settings.h:110`). When on:
- The helper's global `WH_CBT` hook (`overlay/lib.cpp:396`) loads the DLL into every GUI process not excluded.
- D3D9 games: a full state block is created and captured on **every** `Present`, even when nothing is drawn (`overlay/d3d9.cpp:388-392`).
- 32-bit games: `Present` is unpatched and repatched every frame with `VirtualProtect` + `FlushInstructionCache` (`overlay/HardHook_x86.cpp:302-351`). In 64-bit builds MinHook makes this a no-op (`HardHook_minhook.cpp:83-114`).
- Every frame calls `PeekNamedPipe`. While Mumble isn't listening, `CreateFileW` on the pipe runs every frame (`overlay/lib.cpp:121-127`). The FPS report uses a blocking `WriteFile` from inside `Present` every 0.25 s (`lib.cpp:104-115`).
- Turning the overlay off does not unhook games that already loaded it. Restart the game.

**7. Positional audio** (`bTransmitPosition = false`, plugins disabled by default). When on and not linked:
- Every 1 s on the GUI thread there is a full process snapshot, plus `initPositionalData` on every enabled plugin (`PluginManager.cpp:990-1001`, `:217-238`). Some plugins pattern-scan game memory in 32 KiB `ReadProcessMemory` chunks (e.g. `plugins/amongus`).
- A lost link re-scans on the **audio** thread (`PluginManager.cpp:431-443`, called from `AudioOutput.cpp:501`).
- Enable only the plugin for the game you play.

**8. Windows drawn over a borderless game:**
- Tray toasts are on for every message type by default (`Settings.cpp:565-567`, shown via `Log.cpp:1117`). Windows 11's do-not-disturb-while-gaming usually hides them.
- TalkingUI (`TalkingUI.cpp:228`), the PTT button window and always-on-top are topmost but off by default.
- Any of these over the game window costs about a frame while visible.

**9. Persistent chat log** (this fork, off by default): opens, appends to and closes the file on the GUI thread per message (`ChatHistory.cpp:24-42`). Tiny, but noted.

## Checked and clean

- No `timeBeginPeriod`/`NtSetTimerResolution`, no process priority class change, no affinity or power-plan changes (only `src/tests/Resample.cpp`).
- Thread priorities are normal for VoIP: the ServerHandler is time-critical but blocks on I/O, the audio threads are Highest/High plus MMCSS, and the hook thread is time-critical (needed).
- WASAPI exclusive mode is off. Attenuating other apps is off and runs only on talk-state changes, not polling.
- No DirectInput, `SetWinEventHook`, `GetAsyncKeyState` polling, or `QCursor::pos` polling.
- No focus stealing without a user action or an external RPC call. `QApplication::alert` only fires with the Highlight flag, which is off by default.
- GUI timers under 100 ms exist only in dialogs (audio wizard/config, stats, connect dialog).
- The plugin updater is a one-shot at startup on a worker thread. The PositionalAudioViewer timer is only in the Developer menu.
- `PluginManager`'s app-wide event filter only sees Mumble's own key events.
- The overlay DLL is not injected while the overlay is off (`main.cpp:675`, `Overlay_win.cpp:157-167`).
- The earlier `DEBUGSUPPRESS` hook logging seen in `Console.txt` is gone from the code.

## Not checked

- No runtime profiling (ETW/PresentMon/LatencyMon). The rankings come from reading code.
- Qt's own Windows event dispatcher overhead per `WM_INPUT`.
- How often your controller actually sends reports (check with a HID monitor) and whether one is usually connected while gaming.
- Per-game positional plugins beyond `cs` and `amongus`. The MinHook internals were only checked for `restore`/`inject`.

## Sources

- Windows 11 background raw mouse listener throttling (KB5028185, July 2023): https://www.techspot.com/news/99454-new-windows-11-feature-update-addresses-game-stuttering.html, https://www.thefpsreview.com/2023/07/14/windows-11-update-reduces-game-stutter-with-high-report-rate-mice/
