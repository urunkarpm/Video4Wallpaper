# Task 5 Report: Multi-Monitor Detection & Aspect Scaling (`MonitorManager`)

## Status
DONE

## Commit Hash
`d12c8766e6096e0856f40212aef8da6d3af4a04d`

## Implementation Details

### 1. `MonitorManager` (`src/monitor/MonitorManager.h` & `src/monitor/MonitorManager.cpp`)
- Defined `MonitorInfo` struct holding:
  - `HMONITOR hMonitor`
  - `std::wstring deviceName`
  - `RECT rect`
  - `UINT width`, `UINT height`
  - `UINT refreshRate`
  - `UINT dpi`
  - `bool isPrimary`
- Implemented `EnumerateMonitors()` using `EnumDisplayMonitors`, `GetMonitorInfoW`, `EnumDisplaySettingsW` (for display frequency), and dynamic `GetDpiForMonitor` from `Shcore.dll` with fallback to 96 DPI.
- Implemented `GetVirtualScreenBounds()` calculating the bounding box covering all active monitors (with `SM_XVIRTUALSCREEN` fallback).
- Implemented `GetPrimaryMonitor()` returning primary display metadata.
- Included ponytail annotation: `// ponytail: [Win32 EnumDisplayMonitors API] -> [Per-monitor virtual desktop placement with DXGI desktop duplication and HDR color space metadata]`.

### 2. Aspect Scaling Modes & Viewport Dynamic Scaling (`src/renderer/D3D11Renderer.h` & `src/renderer/D3D11Renderer.cpp`)
- Defined `enum class ScalingMode { Fill, Fit, Stretch, Crop, Original }`.
- Configured dynamic vertex buffer (`D3D11_USAGE_DYNAMIC`, `D3D11_CPU_ACCESS_WRITE`) updated via `UpdateGeometry()`.
- Implemented `UpdateGeometry()` to calculate UV quad transformations and NDC vertex quad coordinates on GPU side based on video aspect ratio vs render viewport aspect ratio:
  - **Fill**: Crops video overflow while preserving aspect ratio and filling screen.
  - **Fit**: Fits video inside viewport preserving aspect ratio (letterbox/pillarbox).
  - **Stretch**: Stretches video to full quad ignoring aspect ratio.
  - **Crop** / **Original**: 1:1 pixel mapping centered with cropping or boundary fit.
- Implemented `OnResize(width, height)`: Resizes DXGI swapchain backbuffers, recreates RTV, updates viewport, and recalculates quad geometry.
- `RenderVideoFrame` automatically queries `ID3D11Texture2D` dimensions via `GetDesc()` and updates video geometry scaling.

### 3. Display Event & Window Management (`src/renderer/DesktopHost.cpp`, `CMakeLists.txt`, `src/main.cpp`)
- Updated `DesktopHost::CreateWallpaperWindow` to position the host window across virtual screen bounds at startup.
- Updated `WallpaperWndProc` to handle `WM_DISPLAYCHANGE`, `WM_DPICHANGED`, and `WM_SIZE` messages:
  - Re-evaluates virtual screen bounds via `MonitorManager::GetVirtualScreenBounds()`.
  - Repositions window using `SetWindowPos`.
  - Dispatches `OnResize` callback to `D3D11Renderer` retrieved via `GWLP_USERDATA`.
- Updated `CMakeLists.txt` to compile `src/monitor/MonitorManager.cpp` and link `Shcore.lib`.
- Updated `src/main.cpp` to perform startup monitor enumeration logging, set `GWLP_USERDATA` for `renderer`, and test/log scaling mode calculations.

## Verification Results
- **Compilation**: Compiled with zero errors using MSVC 2022 (`vcvars64.bat`) and CMake Release build.
- **Execution**: Ran `build/Release/WallpaperEngine.exe`.
- **Telemetry Log Verification (`wallpaper_engine.log`)**:
  - `[INFO] Monitor Enumerated: Device=\\.\DISPLAY1, Primary=true, Bounds=[0,0 - 1463x914], RefreshRate=60Hz, DPI=96`
  - `[INFO] Virtual Screen Bounds: [0, 0 - 1463x914]`
  - `[INFO] Primary Monitor: Device=\\.\DISPLAY1, Resolution=1463x914 @ 60Hz, DPI=96`
  - `[INFO] Wallpaper window virtual screen placement: [0, 0 - 1463x914]`
  - Dynamic aspect scaling calculations logged for all 5 modes (`Fill`, `Fit`, `Stretch`, `Crop`, `Original`).
  - Video dimensions updated from decoded texture (`2558x1598`) with aspect ratio scaling applied.
  - Rendered 350+ video frames smoothly at ~29.7 FPS with automated pause/resume cycle and clean shutdown.
