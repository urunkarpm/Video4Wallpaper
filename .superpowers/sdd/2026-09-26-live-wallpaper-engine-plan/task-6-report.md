# Task 6 Implementation Report: Dynamic Performance Profiles & Fullscreen / Battery Detection (`PerformanceManager`)

## Summary
Successfully implemented `PerformanceManager` to handle event-based detection of fullscreen applications, AC/battery power transitions, monitor display state changes, and Windows lock/unlock session events without continuous CPU polling loops. Integrated `PerformanceManager` into `main.cpp` and `DesktopHost.cpp` to dynamically trigger `RenderPipeline::Pause()` and `RenderPipeline::Resume()` upon state evaluation changes.

## Modified & Created Files
1. `src/performance/PerformanceManager.h`:
   - Header declaring `PerformanceManager` class, `PauseCallback` signature `std::function<void(bool pause, const std::string& reason)>`.
   - Thread-safe state accessors, event hook callbacks, power/session notification handlers, and `EvaluatePauseState()` logic.
   - Includes ponytail rule annotation: `// ponytail: [Polling Win32 foreground window and system power state] -> [RawInput + D3D11 swapchain occlusion state queries]`.
2. `src/performance/PerformanceManager.cpp`:
   - `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ...)` for zero-overhead foreground window change notifications.
   - `RegisterPowerSettingNotification(hWnd, &GUID_POWRMGMT_SESSION_CHANGE_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE)` and `RegisterPowerSettingNotification(hWnd, &GUID_MONITOR_POWER_ON, DEVICE_NOTIFY_WINDOW_HANDLE)`.
   - `WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_THIS_SESSION)` for lock/unlock detection.
   - Initial power query via `GetSystemPowerStatus(&sps)` (`ACLineStatus == 0` for battery).
   - Window geometry check `GetWindowRect(fgHWnd)` vs monitor rectangle `mi.rcMonitor` with desktop/shell window exclusion logic.
   - Window message handler `HandleWindowMessage(...)` processing `WM_POWERBROADCAST` (`PBT_APMPOWERSTATUSCHANGE`, `PBT_POWERSETTINGCHANGE`) and `WM_WTSSESSION_CHANGE`.
3. `src/renderer/DesktopHost.cpp`:
   - Forwarded `WM_POWERBROADCAST` and `WM_WTSSESSION_CHANGE` to `PerformanceManager::HandleWindowMessage(...)`.
4. `CMakeLists.txt`:
   - Added `src/performance/PerformanceManager.cpp` and `src/performance/PerformanceManager.h` to executable target.
   - Linked `Wtsapi32.lib` and `PowrProf.lib`.
5. `src/main.cpp`:
   - Initialized `PerformanceManager` with window handle `hWnd` and auto-pause/resume callback targeting `RenderPipeline`.
   - Clean shutdown call `perfManager.Shutdown()` prior to stopping `RenderPipeline`.

## Verification Results
- **Compilation**: Compiled cleanly using MSVC and CMake in Release configuration.
- **Runtime Execution**: Verified with `build/Release/WallpaperEngine.exe`.
  - WinEvent hook installed successfully.
  - WTS session notification registered successfully.
  - Power status verified (`AC Power` detected).
  - Render loop ran 350+ frames at ~29.6 FPS with clean initialization and teardown of `PerformanceManager`.
