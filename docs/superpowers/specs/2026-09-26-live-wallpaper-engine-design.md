# Architecture Design Specification: Native High-Performance Windows Live Wallpaper Engine

**Date**: 2026-09-26  
**Target Platform**: Windows 10 / Windows 11 (x64)  
**Language Standard**: C++20  
**Build Tool**: CMake + MSVC / Visual Studio  

---

## 1. Executive Summary & Design Principles

The objective is to construct a production-ready, native Windows live wallpaper engine prioritizing extreme performance, smooth presentation, low CPU/GPU footprint, dynamic power management, multi-monitor support, and zero-allocation frame loops.

### Core Principles
1. **Ponytail Philosophy**: Minimum code that accomplishes the task. No unnecessary abstractions, zero heavy dependencies (Chromium/Electron/UI web frameworks are strictly prohibited), standard Windows API leverage (`Win32`, `Direct3D 11`, `Media Foundation`, `DXGI`).
2. **Zero-Copy GPU Surface Pipeline**: Hardware-decoded video frames (`NVDEC` / `QuickSync` / `AMD HW` / `DXVA2`) land directly into `ID3D11Texture2D` surfaces using `IMFDXGIDeviceManager`. Frames remain in VRAM without CPU readbacks or image copies.
3. **Zero-Waste Presentation Pacing**: Render/Present calls only trigger when a new video timestamp (PTS) requires display. High-resolution timers (`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`) wake the presentation loop precise to the millisecond.
4. **Desktop Integration**: Embeds directly into the Windows Shell desktop hierarchy behind desktop icons by injecting into `Progman` -> `WorkerW` using message `0x052C`.
5. **Robust Device & Error Recovery**: Transparently recovers from `DXGI_ERROR_DEVICE_RESET` / `DXGI_ERROR_DEVICE_REMOVED`, monitor hot-unplugging, sleep/wake cycles, and corrupt media files.

---

## 2. System Architecture & Module Boundaries

```
[ Application / Tray Controller / Settings ]
                    |
          [ WallpaperManager ]
         /          |          \
 [ MonitorManager ] |  [ PerformanceManager ]
                    |
           [ RenderPipeline ] (Render Thread + High-Res Timer)
                    |
           [ VideoEngine ] (MF Hardware Decoder + D3D11 Texture Pool)
```

### Module Specifications

#### 2.1 VideoEngine (`src/video/`)
* **Responsibilities**: Media decoding, hardware acceleration setup, timestamp handling, loop management, surface sharing.
* **Technology**: Windows Media Foundation (`IMFSourceReader`), `IMFDXGIDeviceManager`, Direct3D 11.
* **Seamless Looping Logic**:
  * Near the end of stream (`MF_SOURCE_READERF_ENDOFSTREAM`), flushing the reader and calling `SetCurrentPosition(0)` re-bases playback without destroying the MF reader or re-allocating Direct3D textures.
* **Codec Support**: H.264, H.265 / HEVC, VP9, AV1 (via system MFT HW decoders). Fallback to MF SW decode if HW transform is unavailable.

#### 2.2 Renderer & RenderPipeline (`src/renderer/`)
* **Responsibilities**: Direct3D 11 device initialization, swapchain management (`IDXGISwapChain1`), vertex/pixel shader execution, aspect-ratio scaling (Fill, Fit, Stretch, Crop), VRAM texture presentation.
* **Desktop Injection (`WorkerW`)**:
  * Finds `Progman` window handle (`FindWindowW(L"Progman", NULL)`).
  * Sends message `0x052C` to `Progman` via `SendMessageTimeoutW` to spawn the split `WorkerW` desktop layer.
  * Enumerates top-level windows to locate the `WorkerW` containing `SHELLDLL_DefView`.
  * Parents the wallpaper window (`SetParent(hWnd, hWorkerW)`).
* **Scaling Modes**: Vertex shader calculates texture coordinates based on target display viewport and source aspect ratio without CPU image manipulation.

#### 2.3 MonitorManager (`src/monitor/`)
* **Responsibilities**: Detection of active monitors, screen geometries, DPI scaling, refresh rates, orientation, and monitor connect/disconnect events (`WM_DISPLAYCHANGE`, `WM_DPICHANGED`).
* **Multi-Monitor Strategy**:
  * Independent wallpapers per monitor OR shared texture reuse across swapchains for duplicate wallpaper mode.

#### 2.4 PerformanceManager (`src/performance/`)
* **Responsibilities**: Auto-pause handling and performance profiles.
* **Event Detection**:
  * **Fullscreen Apps**: Listens to WinEvent `EVENT_SYSTEM_FOREGROUND` via `SetWinEventHook` or polls active window style (`GetGUIThreadInfo` / `GetWindowRect` matching monitor bounds).
  * **Battery Status**: Listens to `WM_POWERBROADCAST` / `RegisterPowerSettingNotification` (`GUID_POWRMGMT_SESSION_CHANGE_STATUS`, `GUID_MONITOR_POWER_ON`).
  * **Session Lock / Sleep**: Handles `WM_WTSSESSION_CHANGE` (`WTS_SESSION_LOCK` / `WTS_SESSION_UNLOCK`).

#### 2.5 Settings & System Tray (`src/settings/` and `src/tray/`)
* **Responsibilities**: System tray notification icon (`Shell_NotifyIconW`), quick menu (Pause, Resume, Switch Wallpaper, Performance Mode, Exit), lightweight configuration file persistence (`settings.json`).

---

## 3. Class Structure & File Map

```text
VideoWallpaper/
├── CMakeLists.txt
├── README.md
├── docs/
│   └── superpowers/specs/2026-09-26-live-wallpaper-engine-design.md
├── src/
│   ├── main.cpp                        # WinMain entry point, loop setup
│   ├── core/
│   │   ├── Application.h / .cpp        # Core application lifecycle & event bus
│   │   ├── Config.h / .cpp             # Settings serialization
│   │   └── Logger.h / .cpp             # Lightweight non-blocking logger
│   ├── video/
│   │   ├── VideoDecoder.h / .cpp       # MF SourceReader & HW decode engine
│   │   └── FrameBuffer.h               # Texture wrapper & timestamp metadata
│   ├── renderer/
│   │   ├── D3D11Renderer.h / .cpp      # D3D11 Device, SwapChain, Shaders
│   │   ├── DesktopHost.h / .cpp        # Progman/WorkerW window integration
│   │   └── Shaders.hlsl                # Simple vertex/pixel shader pair
│   ├── monitor/
│   │   └── MonitorManager.h / .cpp     # Multi-monitor enumeration & display events
│   ├── performance/
│   │   └── PerformanceManager.h / .cpp # Fullscreen, battery, sleep auto-pause
│   ├── tray/
│   │   └── SystemTray.h / .cpp         # Win32 tray icon & context menu
│   └── ui/
│       └── PerformanceHud.h / .cpp     # Optional D2D/D3D HUD overlay (FPS/CPU/RAM)
└── tests/
    └── BenchmarkTest.cpp               # Frame timing & loop stress test
```

---

## 4. Error Handling & Device Loss Recovery

1. **DXGI Device Removed / Reset**:
   * If `Present` or `GetSample` returns `DXGI_ERROR_DEVICE_REMOVED` or `DXGI_ERROR_DEVICE_RESET`:
   * Safely release DXGI Device Manager, MF Source Reader, D3D11 Render Targets, and Swap Chains.
   * Re-initialize `ID3D11Device` and `IMFDXGIDeviceManager`.
   * Re-open media stream at current presentation timestamp.
2. **Corrupt Media Files**:
   * MF initialization failure triggers an error callback, logs the failure, and places the specific monitor renderer in a paused state without crashing the process.

---

## 5. Verification Plan & Performance Targets

### Metrics & Budgets
* **CPU Utilization**: < 1.0% average on 1080p60/1440p60/4K60 video playback.
* **RAM Footprint**: < 60 MB resident working set.
* **Frame Drops**: 0 drops during normal playback.
* **Loop Transition**: < 1 ms seamless timeline re-base.
* **Auto-Pause Response**: < 100 ms on fullscreen game launch.

### Verification Steps
1. Build with MSVC (Release - O2 / C++20).
2. Run single and multi-monitor test configurations.
3. Validate fullscreen detection with borderless/exclusive games and media players.
4. Run long-duration benchmark harness (`BenchmarkTest`) for leak verification.
