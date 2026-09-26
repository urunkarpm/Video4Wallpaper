# High-Performance Windows Live Wallpaper Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a native C++20 Windows live wallpaper engine that plays smooth video wallpapers directly on the desktop background layer with minimal CPU/GPU usage, hardware decoding, zero-copy VRAM presentation, seamless looping, multi-monitor support, and dynamic auto-pausing.

**Architecture:** A lightweight Win32 application leveraging Direct3D 11, Media Foundation (`IMFSourceReader` + `IMFDXGIDeviceManager`), and desktop `WorkerW` injection (`0x052C`). Dedicated render thread uses high-resolution waitable timers for timestamp-accurate frame scheduling.

**Tech Stack:** C++20, CMake, Visual Studio / MSVC, Direct3D 11, DXGI, Media Foundation, Win32 API.

**Spec:** [2026-09-26-live-wallpaper-engine-design.md](file:///C:/Users/uprasenjeet/Documents/VideoWallpaper/docs/superpowers/specs/2026-09-26-live-wallpaper-engine-design.md)

## Global Constraints

- **Language Standard**: C++20 (`/std:c++20` or `-std=c++20`).
- **Dependencies**: Native Windows APIs only (`Win32`, `D3D11`, `DXGI`, `Media Foundation`). No Electron, no WebView, no heavy third-party UI frameworks.
- **Performance Priority**: Zero CPU-to-GPU frame copies. Zero-allocation loop boundaries. Low CPU (<1.0%) and VRAM footprint.
- **Ponytail Annotations**: Mark deliberate corner-cuts or ceilings with `// ponytail: [ceiling] -> [upgrade path]`.

---

### Task 1: Project Setup & Core CMake Build Infrastructure

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `src/core/Logger.h`
- Create: `src/core/Logger.cpp`

**Interfaces:**
- Produces: Executable `WallpaperEngine.exe`, logger API `Logger::LogInfo`, `Logger::LogError`.

- [ ] **Step 1: Create `CMakeLists.txt` with C++20 & Windows SDK dependencies**

```cmake
cmake_minimum_required(VERSION 3.20)
project(WallpaperEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(WallpaperEngine
    src/main.cpp
    src/core/Logger.cpp
)

target_include_directories(WallpaperEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(WallpaperEngine PRIVATE
    d3d11.lib
    dxgi.lib
    mf.lib
    mfplat.lib
    mfreadwrite.lib
    mfuuid.lib
    Shlwapi.lib
    User32.lib
    Gdi32.lib
)
```

- [ ] **Step 2: Implement `src/core/Logger.h` and `src/core/Logger.cpp`**

```cpp
// src/core/Logger.h
#pragma once
#include <string>

enum class LogLevel { Info, Warning, Error, Debug };

class Logger {
public:
    static void Log(LogLevel level, const std::string& message);
    static void LogInfo(const std::string& msg) { Log(LogLevel::Info, msg); }
    static void LogError(const std::string& msg) { Log(LogLevel::Error, msg); }
};
```

- [ ] **Step 3: Implement minimal `src/main.cpp` WinMain entry point**

```cpp
// src/main.cpp
#include <windows.h>
#include "core/Logger.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Logger::LogInfo("WallpaperEngine initializing...");
    return 0;
}
```

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/main.cpp src/core/Logger.h src/core/Logger.cpp
git commit -m "feat: initialize project CMake structure and logging subsystem"
```

---

### Task 2: Direct3D 11 Renderer Engine & Win32 Desktop Window Injection (`WorkerW`)

**Files:**
- Create: `src/renderer/DesktopHost.h`
- Create: `src/renderer/DesktopHost.cpp`
- Create: `src/renderer/D3D11Renderer.h`
- Create: `src/renderer/D3D11Renderer.cpp`
- Create: `src/renderer/Shaders.hlsl`

**Interfaces:**
- Consumes: `Logger`
- Produces: `DesktopHost::GetWorkerW()`, `D3D11Renderer::Initialize()`, `D3D11Renderer::RenderFrame()`

- [ ] **Step 1: Implement `src/renderer/DesktopHost.h` & `DesktopHost.cpp` for Progman `0x052C` injection**

```cpp
// src/renderer/DesktopHost.h
#pragma once
#include <windows.h>

class DesktopHost {
public:
    static HWND GetWorkerWHandle();
    static HWND CreateWallpaperWindow(HINSTANCE hInstance, HWND hWorkerW);
};
```

- [ ] **Step 2: Implement `src/renderer/Shaders.hlsl`**

```hlsl
Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_INPUT {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

PS_INPUT VSMain(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET {
    return g_Texture.Sample(g_Sampler, input.uv);
}
```

- [ ] **Step 3: Implement `src/renderer/D3D11Renderer.h` & `.cpp`**

```cpp
// src/renderer/D3D11Renderer.h
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class D3D11Renderer {
public:
    bool Initialize(HWND hWnd);
    void RenderTestFrame();
    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_rtv;
};
```

- [ ] **Step 4: Commit**

```bash
git add src/renderer/
git commit -m "feat: implement D3D11 rendering engine and WorkerW desktop host integration"
```

---

### Task 3: Media Foundation Hardware Video Decoder (`VideoEngine`)

**Files:**
- Create: `src/video/VideoDecoder.h`
- Create: `src/video/VideoDecoder.cpp`

**Interfaces:**
- Consumes: `ID3D11Device`, `Logger`
- Produces: `VideoDecoder::OpenFile()`, `VideoDecoder::GetNextFrame()`, `VideoDecoder::Rewind()`

- [ ] **Step 1: Implement `src/video/VideoDecoder.h`**

```cpp
// src/video/VideoDecoder.h
#pragma once
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

struct DecodedFrame {
    ComPtr<ID3D11Texture2D> texture;
    LONGLONG timestamp; // 100-nanosecond units
    bool isEndOfStream = false;
};

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool Initialize(ID3D11Device* pDevice);
    bool OpenFile(const std::wstring& filePath);
    DecodedFrame GetNextFrame();
    bool Rewind(); // Zero-allocation loop

private:
    ComPtr<IMFDXGIDeviceManager> m_dxgiManager;
    ComPtr<IMFSourceReader> m_sourceReader;
    UINT m_resetToken = 0;
    DWORD m_streamIndex = (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM;
};
```

- [ ] **Step 2: Implement `src/video/VideoDecoder.cpp` with HW transform setup**

```cpp
// src/video/VideoDecoder.cpp
#include "video/VideoDecoder.h"
#include "core/Logger.h"

VideoDecoder::VideoDecoder() {
    MFStartup(MF_VERSION);
}

VideoDecoder::~VideoDecoder() {
    MFShutdown();
}

bool VideoDecoder::Initialize(ID3D11Device* pDevice) {
    HRESULT hr = MFCreateDXGIDeviceManager(&m_resetToken, &m_dxgiManager);
    if (FAILED(hr)) return false;
    hr = m_dxgiManager->ResetDevice(pDevice, m_resetToken);
    return SUCCEEDED(hr);
}
```

- [ ] **Step 3: Commit**

```bash
git add src/video/
git commit -m "feat: implement Media Foundation HW video decoder with zero-copy D3D11 surface output"
```

---

### Task 4: High-Precision Frame Scheduling & Render Pipeline

**Files:**
- Create: `src/renderer/RenderPipeline.h`
- Create: `src/renderer/RenderPipeline.cpp`

**Interfaces:**
- Consumes: `VideoDecoder`, `D3D11Renderer`
- Produces: `RenderPipeline::Start()`, `RenderPipeline::Pause()`, `RenderPipeline::Resume()`

- [ ] **Step 1: Implement `src/renderer/RenderPipeline.h` & `.cpp`**

```cpp
// src/renderer/RenderPipeline.h
#pragma once
#include <windows.h>
#include <thread>
#include <atomic>
#include "video/VideoDecoder.h"
#include "renderer/D3D11Renderer.h"

class RenderPipeline {
public:
    RenderPipeline(D3D11Renderer* renderer, VideoDecoder* decoder);
    ~RenderPipeline();

    void Start();
    void Stop();
    void Pause();
    void Resume();

private:
    void RenderLoop();

    D3D11Renderer* m_renderer;
    VideoDecoder* m_decoder;
    std::thread m_renderThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    HANDLE m_waitableTimer = nullptr;
};
```

- [ ] **Step 2: Commit**

```bash
git add src/renderer/RenderPipeline.h src/renderer/RenderPipeline.cpp
git commit -m "feat: add high-precision waitable-timer frame pacing pipeline"
```

---

### Task 5: Multi-Monitor Detection & Aspect Scaling

**Files:**
- Create: `src/monitor/MonitorManager.h`
- Create: `src/monitor/MonitorManager.cpp`

**Interfaces:**
- Consumes: Win32 EnumDisplayMonitors API
- Produces: `MonitorManager::EnumerateMonitors()`, `MonitorManager::GetActiveMonitors()`

- [ ] **Step 1: Implement `src/monitor/MonitorManager.h` & `.cpp`**

```cpp
// src/monitor/MonitorManager.h
#pragma once
#include <windows.h>
#include <vector>

struct MonitorInfo {
    HMONITOR hMonitor;
    RECT rect;
    UINT refreshRate;
    bool isPrimary;
};

class MonitorManager {
public:
    static std::vector<MonitorInfo> EnumerateMonitors();
};
```

- [ ] **Step 2: Commit**

```bash
git add src/monitor/
git commit -m "feat: add multi-monitor enumeration and viewport dynamic scaling"
```

---

### Task 6: Dynamic Performance Profiles & Fullscreen Detection

**Files:**
- Create: `src/performance/PerformanceManager.h`
- Create: `src/performance/PerformanceManager.cpp`

**Interfaces:**
- Consumes: `SetWinEventHook`, `RegisterPowerSettingNotification`
- Produces: `PerformanceManager::StartMonitoring()`, Callbacks for Auto-Pause/Resume

- [ ] **Step 1: Implement `src/performance/PerformanceManager.h` & `.cpp`**

```cpp
// src/performance/PerformanceManager.h
#pragma once
#include <windows.h>
#include <functional>

class PerformanceManager {
public:
    using PauseCallback = std::function<void(bool pause)>;

    void Initialize(PauseCallback callback);
    void Shutdown();

private:
    static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);
    PauseCallback m_callback;
    HWINEVENTHOOK m_hook = nullptr;
};
```

- [ ] **Step 2: Commit**

```bash
git add src/performance/
git commit -m "feat: add event-based fullscreen app detection and battery power auto-pause"
```

---

### Task 7: System Tray Controller, Config Persistence & HUD

**Files:**
- Create: `src/tray/SystemTray.h`
- Create: `src/tray/SystemTray.cpp`
- Create: `src/core/Config.h`
- Create: `src/core/Config.cpp`
- Create: `src/ui/PerformanceHud.h`
- Create: `src/ui/PerformanceHud.cpp`

**Interfaces:**
- Consumes: `Shell_NotifyIconW`, `RenderPipeline`
- Produces: `SystemTray::CreateIcon()`, `Config::Load()`, `Config::Save()`, `PerformanceHud::Draw()`

- [ ] **Step 1: Implement SystemTray and Config modules**
- [ ] **Step 2: Commit**

```bash
git add src/tray/ src/core/Config.h src/core/Config.cpp src/ui/
git commit -m "feat: add system tray controller, json config persistence, and developer HUD"
```

---

### Task 8: Verification & Long-Running Benchmark Harness

**Files:**
- Create: `tests/BenchmarkTest.cpp`

**Interfaces:**
- Consumes: Full Video Engine & Renderer Pipeline
- Produces: Performance statistics (FPS, dropped frames, memory drift)

- [ ] **Step 1: Implement `tests/BenchmarkTest.cpp`**
- [ ] **Step 2: Run verification tests and benchmark suite**
- [ ] **Step 3: Commit**

```bash
git add tests/
git commit -m "test: add automated benchmark suite for frame pacing and memory drift verification"
```
