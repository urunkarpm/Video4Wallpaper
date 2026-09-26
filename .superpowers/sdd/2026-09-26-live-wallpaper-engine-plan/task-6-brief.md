# Task 6 Brief: Dynamic Performance Profiles & Fullscreen / Battery Detection (`PerformanceManager`)

## Target Files
- `src/performance/PerformanceManager.h`
- `src/performance/PerformanceManager.cpp`
- `CMakeLists.txt`
- `src/main.cpp`

## Requirements
1. `PerformanceManager` (`src/performance/PerformanceManager.h` & `src/performance/PerformanceManager.cpp`):
   - Callback Signature: `using PauseCallback = std::function<void(bool pause, const std::string& reason)>;`
   - `bool Initialize(HWND hWnd, PauseCallback callback)`:
     - Installs event-based WinEvent hook via `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT)`.
     - Registers power setting notifications via `RegisterPowerSettingNotification(hWnd, &GUID_POWRMGMT_SESSION_CHANGE_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE)` and `GUID_MONITOR_POWER_ON`.
     - Stores window handle `hWnd` and callback.
   - `void Shutdown()`:
     - Unhooks WinEvent hook via `UnhookWinEvent`.
     - Unregisters power notifications.
   - Event Handling Logic:
     - Fullscreen Detection: When foreground window changes (`EVENT_SYSTEM_FOREGROUND` or periodic low-overhead check), query window rectangle (`GetWindowRect`). Compare against monitor bounds from `MonitorManager`. If foreground window is borderless/fullscreen and non-desktop/workerw window, set `isFullscreen = true`.
     - Power Broadcast: Handles `WM_POWERBROADCAST` (`PBT_APMPOWERSTATUSCHANGE`, `PBT_POWERSETTINGCHANGE`). Query `GetSystemPowerStatus` (`ACLineStatus == 0` for battery).
     - Lock/Sleep State: Handles `WM_WTSSESSION_CHANGE` or monitor power off (`GUID_MONITOR_POWER_ON` value == 0).
   - Dynamic Auto-Pause Decision:
     - Evaluates rules: If (`pauseOnFullscreen` AND `isFullscreen`) OR (`pauseOnBattery` AND `onBattery`) OR `monitorPowerOff` OR `sessionLocked`: trigger `PauseCallback(true, reason)`. Else: trigger `PauseCallback(false, "")`.
2. `CMakeLists.txt`:
   - Add `src/performance/PerformanceManager.cpp` to target.
3. `src/main.cpp`:
   - Integrate `PerformanceManager` with `RenderPipeline` pause/resume callbacks and message loop.
4. Verification:
   - Compile via CMake + MSVC. Run `WallpaperEngine.exe`, test foreground window state transitions, verify auto-pause logic and clean resumption when conditions clear.

## Ponytail Rules
- Use event-driven `SetWinEventHook` and `WM_POWERBROADCAST` to avoid high-frequency process polling loops.
- Include `// ponytail: [ceiling] -> [upgrade path]` annotations.
