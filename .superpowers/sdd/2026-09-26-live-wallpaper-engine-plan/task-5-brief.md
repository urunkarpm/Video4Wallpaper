# Task 5 Brief: Multi-Monitor Detection & Aspect Scaling (`MonitorManager`)

## Target Files
- `src/monitor/MonitorManager.h`
- `src/monitor/MonitorManager.cpp`
- `src/renderer/D3D11Renderer.h`
- `src/renderer/D3D11Renderer.cpp`
- `CMakeLists.txt`
- `src/main.cpp`

## Requirements
1. `MonitorManager` (`src/monitor/MonitorManager.h` & `src/monitor/MonitorManager.cpp`):
   - Struct `MonitorInfo`:
     ```cpp
     struct MonitorInfo {
         HMONITOR hMonitor = NULL;
         std::wstring deviceName;
         RECT rect{};
         UINT width = 0;
         UINT height = 0;
         UINT refreshRate = 60;
         UINT dpi = 96;
         bool isPrimary = false;
     };
     ```
   - `static std::vector<MonitorInfo> EnumerateMonitors()`: Uses `EnumDisplayMonitors` & `GetMonitorInfoW` / `DEVMODEW` to query resolution, virtual screen rect, refresh rate, and DPI (`GetDpiForMonitor`).
   - `static RECT GetVirtualScreenBounds()`: Returns full virtual screen bounds covering all active displays.
   - `static MonitorInfo GetPrimaryMonitor()`: Returns primary monitor info.
2. Aspect Scaling Modes (`D3D11Renderer`):
   - Enum `enum class ScalingMode { Fill, Fit, Stretch, Crop, Original }`.
   - Update `D3D11Renderer::SetScalingMode(ScalingMode mode)`: Calculates UV vertex quad transformations based on video aspect ratio vs render viewport aspect ratio on GPU side (without CPU pixel scaling).
3. System Event Handling (`WM_DISPLAYCHANGE`, `WM_DPICHANGED`):
   - Win32 WndProc handles monitor geometry changes and updates viewport/swapchain buffers.
4. `CMakeLists.txt`:
   - Add `src/monitor/MonitorManager.cpp` to target.
5. Verification:
   - Compile via CMake + MSVC. Run `WallpaperEngine.exe`, verify monitor enumeration logs primary and secondary display geometry, refresh rates, DPIs, and scaling calculations.

## Ponytail Rules
- Handle multi-monitor virtual screen bounds cleanly with fallback for single monitor setups.
- Include `// ponytail: [ceiling] -> [upgrade path]` annotations.
